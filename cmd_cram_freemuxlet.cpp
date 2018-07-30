#include "cramore.h"
#include "bcf_filtered_reader.h"
#include "sam_filtered_reader.h"
#include "sc_drop_seq.h"
#include "louvain.h"

struct dropD {
  int32_t nsnps;
  double llk0;
  double llk1;
  double llk2;

  dropD() : nsnps(0), llk0(0), llk1(0), llk2(0) {}
  dropD(int32_t _nsnps, double _llk0, double _llk1, double _llk2) :
    nsnps(_nsnps), llk0(_llk0), llk1(_llk1), llk2(_llk2) {}

  void set(int32_t _nsnps, double _llk0, double _llk1, double _llk2) {
    nsnps = _nsnps;
    llk0 = _llk0;
    llk1 = _llk1;
    llk2 = _llk2;
  }
};


///////////////////////////////////////////////////////////////////
// Freemuxlet : Genotype-free deconvolution of scRNA-seq doublets
//////////////////////////////////////////////////////////////////
int32_t cmdCramFreemuxlet(int32_t argc, char** argv) {
  //std::string gtfFile;
  std::string outPrefix;
  std::string plpPrefix;
  int32_t capBQ = 40;
  int32_t minBQ = 13;
  std::vector<double> gridAlpha;
  double doublet_prior = 0.5;
  std::string groupList;
  int32_t minTotalReads = 0;
  int32_t minUniqReads = 0;
  int32_t minCoveredSNPs = 0;

  paramList pl;

  BEGIN_LONG_PARAMS(longParameters)
    LONG_PARAM_GROUP("Options for input pileup", NULL)
    LONG_STRING_PARAM("plp",&plpPrefix, "Prefix of input files generated by dsc-pileup")

    LONG_PARAM_GROUP("Output Options", NULL)
    LONG_STRING_PARAM("out",&outPrefix,"Output file prefix")
    LONG_MULTI_DOUBLE_PARAM("alpha",&gridAlpha, "Grid of alpha to search for (default is 0, 0.5)")
    LONG_DOUBLE_PARAM("doublet-prior",&doublet_prior, "Prior of doublet")
    
    LONG_PARAM_GROUP("Read filtering Options", NULL)
    LONG_INT_PARAM("cap-BQ", &capBQ, "Maximum base quality (higher BQ will be capped)")
    LONG_INT_PARAM("min-BQ", &minBQ, "Minimum base quality to consider (lower BQ will be skipped)")

    LONG_PARAM_GROUP("Cell/droplet filtering options", NULL)
    LONG_STRING_PARAM("group-list",&groupList, "List of tag readgroup/cell barcode to consider in this run. All other barcodes will be ignored. This is useful for parallelized run")    
    LONG_INT_PARAM("min-total", &minTotalReads, "Minimum number of total reads for a droplet/cell to be considered")
    LONG_INT_PARAM("min-uniq", &minUniqReads, "Minimum number of unique reads (determined by UMI/SNP pair) for a droplet/cell to be considered")
    LONG_INT_PARAM("min-snp", &minCoveredSNPs, "Minimum number of SNPs with coverage for a droplet/cell to be considered")
  END_LONG_PARAMS();

  pl.Add(new longParams("Available Options", longParameters));
  pl.Read(argc, argv);
  pl.Status();

  if ( plpPrefix.empty() || outPrefix.empty() )
    error("Missing required option(s) : --plp and --out");

  if ( gridAlpha.empty() ) {
    gridAlpha.push_back(0);    
    gridAlpha.push_back(0.5);    
  }

  std::set<std::string> bcdSet;
  sc_dropseq_lib_t scl;
  int32_t nAlpha = (int32_t)gridAlpha.size();

  // Read droplet information from the mux-pileup output
  notice("Reading barcode information from %s.cel.gz..", plpPrefix.c_str());
  tsv_reader tsv_bcdf( (plpPrefix + ".cel.gz").c_str() );
  while( tsv_bcdf.read_line() > 0 ) {
    scl.add_cell(tsv_bcdf.str_field_at(1));
  }

  // Read SNP information from the mux-pileup output  
  tsv_reader tsv_varf( (plpPrefix + ".var.gz").c_str() );

  std::map<std::string, int32_t> chr2rid;
  while( tsv_varf.read_line() > 0 ) {
    const char* chr = tsv_varf.str_field_at(1);
    if ( chr2rid.find(chr) == chr2rid.end() ) {
      int32_t newrid = chr2rid.size();
      chr2rid[chr] = newrid;
    }
    int32_t rid = chr2rid[chr];
    int32_t pos = tsv_varf.int_field_at(2);
    char    ref = tsv_varf.str_field_at(3)[0];
    char    alt = tsv_varf.str_field_at(4)[0];
    double  af  = tsv_varf.double_field_at(5);

    if ( scl.add_snp(rid, pos, ref, alt, af, NULL) + 1 != tsv_varf.nlines )
      error("Expected SNP nID = %d but observed %s", tsv_varf.nlines-1, scl.nsnps-1);
  }

  // Read pileup information
  char buf[255];
  notice("Reading pileup information from %s.plp.gz..", plpPrefix.c_str());
  tsv_reader tsv_plpf( (plpPrefix + ".plp.gz").c_str() );
  int32_t numi = 0;
  while( tsv_plpf.read_line() > 0 ) {
    const char* pa = tsv_plpf.str_field_at(2);
    const char* pq = tsv_plpf.str_field_at(3);
    int32_t l = (int32_t)strlen(pq);
    
    if ( (int32_t)strlen(pq) != l )
      error("Length are different between %s and %s", pa, pq);
    
    for(int32_t i=0; i < l; ++i) {
      sprintf(buf, "%x", numi++);
      ++scl.cell_totl_reads[tsv_plpf.int_field_at(0)];    	
      scl.add_read( tsv_plpf.int_field_at(1), tsv_plpf.int_field_at(0), buf, (char)(pa[i]-(char)'0'), (char)(pq[i]-(char)33) ); 
    }
  }

  notice("Finished reading pileup information from %s.plp.gz..", plpPrefix.c_str());  

  struct sc_drop_comp_t {
    sc_dropseq_lib_t* pscl;
    sc_drop_comp_t(sc_dropseq_lib_t* p) : pscl(p) {}
    bool operator()(const int32_t& lhs, const int32_t& rhs) const {
      int32_t cmp = (int32_t) pscl->cell_uniq_reads[lhs] - (int32_t) pscl->cell_uniq_reads[rhs];
      if ( cmp != 0 ) return cmp > 0;
      else return lhs > rhs;
    }
  };

  // sort cells based on the number of SNP-overlapping unique reads.
  std::vector<int32_t> drops_srted(scl.nbcs);
  for(int32_t i=0; i < scl.nbcs; ++i) drops_srted[i] = i;
  sc_drop_comp_t sdc(&scl);
  std::sort( drops_srted.begin(), drops_srted.end(), sdc );

  /* test code: print out the sorted cells
  for(int32_t i=0; i < scl.nbcs; ++i) {
    int32_t j = drops_srted[i];
    if ( i % 10 == 0 )
      notice("%d\t%d\t%d\t%d\t%d\t%u", i, j, scl.cell_totl_reads[j], scl.cell_pass_reads[j], scl.cell_uniq_reads[j], scl.cell_umis[j].size());
  }
  */

  // compute Bayes Factor for every pair of droplets sequentially
  std::vector<int32_t> clusts;
  int32_t nclusts = 0;

  // First, calculate the heterozygosity of each droplet to determine which droplet is
  // likely potentially doublets

  htsFile* wmix = hts_open((outPrefix+".lmix").c_str(),"w");
  hprintf(wmix, "INT_ID\tBARCODE\tNSNPs\tNREADs\tDBL.LLK\tSNG.LLK\tLOG.BF\tBFpSNP\n");
    
  for(int32_t i=0; i < scl.nbcs; ++i) {
    int32_t si = drops_srted[i];
    if (i % 1000 == 0 )
      notice("Processing doublet likelihoods for %d droplets..", i+1);

    int32_t nSNPs = 0;
    int32_t nReads = 0;
    
    // likelihood calculation across the overlapping SNPs
    std::map<int32_t,sc_snp_droplet_t* >::iterator it = scl.cell_umis[si].begin();

    double llk0 = 0, llk2 = 0; // LLK of IBD0, IBD1, IBD2     
    
    while( it != scl.cell_umis[si].end() ) {
      double gls[9] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
      double af = scl.snps[it->first].af;

      // calculate genotype likelihoods
      calculate_snp_droplet_doublet_GL(it->second, gls, 0.5);

      double lk0 = 0, lk2 = 0;
      double gps[3];
      gps[0] = (1.0-af) * (1.0-af);
      gps[1] = 2.0 * af * (1.0-af);
      gps[2] = af * af;
	
      for(int32_t gi=0; gi < 3; ++gi) {
	lk2 += ( gls[gi*3 + gi] * gps[gi] );
	for(int32_t gj=0; gj < 3; ++gj) {
	  lk0 += ( gls[gi*3 + gj] * gps[gi] * gps[gj] );
	}
      }
      nReads += (int32_t)it->second->size();
      ++nSNPs;
      
      ++it;

      llk0 += log(lk0);
      llk2 += log(lk2);
    }

    hprintf(wmix,"%d\t%s\t%d\t%d\t%.2lf\t%.2lf\t%.2lf\t%.4lf\n", si, scl.bcs[si].c_str(), nSNPs, nReads, llk0, llk2, llk0-llk2, (llk0-llk2)/nSNPs);
  }
  hts_close(wmix);

  // store pairwise distances
  std::vector< std::vector<dropD> > dropDs;

  //louvain lv(scl.nbcs);

  htsFile* wf = hts_open((outPrefix+".ldist").c_str(),"w");
  hprintf(wf, "ID1\tID2\tNSNP\tREAD1\tREAD2\tREADMIN\tLLK0\tLLK1\tLLK2\tLDIFF\tDIFF.SNP\n");

  for(int32_t i=0; i < scl.nbcs; ++i) {
    int32_t si = drops_srted[i];
    bool clique = true;
    int32_t clust = -1;

    dropDs.resize(i+1);

    if (i % 50 == 0 )
      notice("Processing %d droplets..", i+1);
    
    for(int32_t j=0; j < i; ++j) {
      int32_t sj = drops_srted[j];

      // likelihood calculation across the overlapping SNPs
      std::map<int32_t,sc_snp_droplet_t* >::iterator iti = scl.cell_umis[si].begin();
      std::map<int32_t,sc_snp_droplet_t* >::iterator itj = scl.cell_umis[sj].begin();

      int32_t nInformativeSNPs = 0;
      int32_t nInformativeRead1 = 0;
      int32_t nInformativeRead2 = 0;
      int32_t nInformativeReadMin = 0;       
      double llk0 = 0, llk1 = 0, llk2 = 0; // LLK of IBD0, IBD1, IBD2 
      while( ( iti != scl.cell_umis[si].end() ) && ( itj != scl.cell_umis[sj].end() ) ) {
	if ( iti->first == itj->first ) {  // overlapping SNPs
	  // calculate Pr(D|g)
	  double glis[3] = {1.0, 1.0, 1.0};
	  double gljs[3] = {1.0, 1.0, 1.0};
	  double af = scl.snps[iti->first].af;
	  double lk0 = 0, lk1 = 0, lk2 = 0;
	  double gps[3];
	  double tps[9] = {0,0,0,0,0,0,0,0,0};
	  gps[0] = (1.0-af) * (1.0-af);
	  gps[1] = 2.0 * af * (1.0-af);
	  gps[2] = af * af;
	  tps[0] = (1.0-af) * (1.0-af) * (1.0-af);
	  tps[1] = tps[3] = (1.0-af) * (1.0-af) * af;
	  tps[5] = tps[7] = (1.0-af) * af * af;
	  tps[4] = tps[1] + tps[5];
	  tps[8] = af * af * af;

	  // calculate genotype likelihoods
	  calculate_snp_droplet_GL(iti->second, glis);
	  calculate_snp_droplet_GL(itj->second, gljs);

	  for(int32_t gi=0; gi < 3; ++gi) {
	    lk2 += ( glis[gi] * gljs[gi] * gps[gi] );
	    for(int32_t gj=0; gj < 3; ++gj) {
	      lk0 += ( glis[gi] * gljs[gj] * gps[gi] * gps[gj] );
	      lk1 += ( glis[gi] * gljs[gj] * tps[gi*3+gj] );
	    }
	  }

	  llk2 += log(lk2);
	  llk1 += log(lk1);	  
	  llk0 += log(lk0);

	  ++nInformativeSNPs;
	  ++iti;
	  ++itj;
	  nInformativeRead1 += (int32_t)scl.cell_umis[si].size();
	  nInformativeRead2 += (int32_t)scl.cell_umis[sj].size();
	  nInformativeReadMin += (int32_t)(scl.cell_umis[si].size() > scl.cell_umis[sj].size() ? scl.cell_umis[sj].size() : scl.cell_umis[si].size() );
	}
	else if ( iti->first < itj->first ) ++iti;
	else ++itj;
      }

      
      dropDs[i].push_back( dropD(nInformativeSNPs, llk0, llk1, llk2) );

      hprintf(wf, "%d\t%d\t%d\t%d\t%d\t%d\t%.2lf\t%.2lf\t%.2lf\t%.2lf\t%.4lf\n", si, sj, nInformativeSNPs, nInformativeRead1, nInformativeRead2, nInformativeReadMin, llk0, llk1, llk2, llk2-llk0, (llk2-llk0)/nInformativeSNPs);      

      //if ( llk2 > llk0 + 2 ) {
      //lv.add_edge(si, sj, 1.0);
      //}
    }
  }

  notice("Finished calculating pairwise distance between the droplets..");

  notice("Finding clusters...");

  /*
  lv.make_cluster_pass(true);
  lv.print_summary();

  // print out cluster assignments
  std::vector<int32_t> iclust(scl.nbcs, -1);
  std::vector<lnode*>& nodes = lv.root->children;
  for(int32_t i=0; i < (int32_t)nodes.size(); ++i) {
    for(int32_t j=0; j < (int32_t)nodes[i]->children.size(); ++j) {
      iclust[nodes[i]->children[j]->id] = i;
    }
  }
  for(int32_t i=0; i < (int32_t)iclust.size(); ++i) {
    printf("%s\t%d\n", scl.bcs[i].c_str(), iclust[i]);
  }
  */  

  return 0;
}
