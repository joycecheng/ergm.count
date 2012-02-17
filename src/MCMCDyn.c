#include "MCMCDyn.h"

/*****************
 Note on undirected networks:  For j<k, edge {j,k} should be stored
 as (j,k) rather than (k,j).  In other words, only directed networks
 should have (k,j) with k>j.
*****************/

/*R_INLINE*/ void MCMCDyn_init_common(int *tails, int *heads, int n_edges,
				  int n_nodes, int dflag, int bipartite, Network *nw,

				  int F_nterms, char *F_funnames, char *F_sonames, double *F_inputs, Model **F_m,
				  int D_nterms, char *D_funnames, char *D_sonames, double *D_inputs, Model **D_m,
				  
				  int *attribs, int *maxout, int *maxin, int *minout,
				  int *minin, int condAllDegExact, int attriblength,

				  char *F_MHproposaltype, char *F_MHproposalpackage, MHproposal *F_MH,
				  char *D_MHproposaltype, char *D_MHproposalpackage, MHproposal *D_MH,
				  int fVerbose){
  GetRNGstate();  /* R function enabling uniform RNG */
  
  *F_m=ModelInitialize(F_funnames, F_sonames, &F_inputs, F_nterms);
  *D_m=ModelInitialize(D_funnames, D_sonames, &D_inputs, D_nterms);

  nw[0]=NetworkInitialize(tails, heads, n_edges, 
                          n_nodes, dflag, bipartite, 1);
  nw[1]=NetworkInitialize(NULL, NULL, 0,
                          n_nodes, dflag, bipartite, 0);
  
  MH_init(F_MH, F_MHproposaltype, F_MHproposalpackage, F_inputs, fVerbose, nw, attribs, maxout, maxin, minout, minin,
			    condAllDegExact, attriblength);
  MH_init(D_MH, D_MHproposaltype, D_MHproposalpackage, D_inputs, fVerbose, nw, attribs, maxout, maxin, minout, minin,
			    condAllDegExact, attriblength);

}

/*R_INLINE*/ void MCMCDyn_finish_common(Network *nw,

				    Model *F_m,
				    Model *D_m,
				  
				    MHproposal *F_MH,
				    MHproposal *D_MH){
  MH_free(F_MH);
  MH_free(D_MH);
  ModelDestroy(F_m);
  ModelDestroy(D_m);
  NetworkDestroy(nw);
  NetworkDestroy(&nw[1]);
  PutRNGstate();  /* Disable RNG before returning */

}

/*****************
 void MCMCDyn_wrapper

 Wrapper for a call from R.
*****************/
void MCMCDyn_wrapper(// Starting network.
		     int *tails, int *heads, int *n_edges,
		     int *n_nodes, int *dflag, int *bipartite,
		     // Formation terms and proposals.
		     int *F_nterms, char **F_funnames, char **F_sonames, 
		     char **F_MHproposaltype, char **F_MHproposalpackage,
		     double *F_inputs, double *F_theta, 
		     // Dissolution terms and proposals.
		     int *D_nterms, char **D_funnames, char **D_sonames,
		     char **D_MHproposaltype, char **D_MHproposalpackage,
		     double *D_inputs, double *D_theta,
		     // Degree bounds.
		     int *attribs, int *maxout, int *maxin, int *minout,
		     int *minin, int *condAllDegExact, int *attriblength, 
		     // MCMC settings.
		     double *nsteps,  int *MH_interval,
		     double *burnin, double *interval,  
		     // Space for output.
		     double *F_sample, double *D_sample, 
		     int *maxedges,
		     int *newnetworktails, int *newnetworkheads, 
		     int *maxchanges,
		     int *diffnetworktime, int *diffnetworktail, int *diffnetworkhead, 
		     // Verbosity.
		     int *fVerbose,
		     int *status){
  Network nw[2];
  Model *F_m, *D_m;
  MHproposal F_MH, D_MH;

  Vertex *difftime, *difftail, *diffhead;
  difftime = (Vertex *) diffnetworktime;
  difftail = (Vertex *) diffnetworktail;
  diffhead = (Vertex *) diffnetworkhead;

  if(diffnetworktime==NULL){
    difftime = (Vertex *) calloc(*maxchanges,sizeof(Vertex));
    difftail = (Vertex *) calloc(*maxchanges,sizeof(Vertex));
    diffhead = (Vertex *) calloc(*maxchanges,sizeof(Vertex));
  }

  
  if(*burnin==0 && *interval==1){
    memset(difftime,0,*maxchanges*sizeof(Vertex));
    memset(difftail,0,*maxchanges*sizeof(Vertex));
    memset(diffhead,0,*maxchanges*sizeof(Vertex));
  }

  MCMCDyn_init_common(tails, heads, *n_edges,
		      *n_nodes, *dflag, *bipartite, nw,
		      *F_nterms, *F_funnames, *F_sonames, F_inputs, &F_m,
		      *D_nterms, *D_funnames, *D_sonames, D_inputs, &D_m,
		      attribs, maxout, maxin, minout,
		      minin, *condAllDegExact, *attriblength, 
		      *F_MHproposaltype, *F_MHproposalpackage, &F_MH,
		      *D_MHproposaltype, *D_MHproposalpackage, &D_MH,
		      *fVerbose);

  *status = MCMCSampleDyn(nw,
			 F_m, &F_MH, F_theta,
			 D_m, &D_MH, D_theta,
			 F_sample, D_sample, *maxedges, *maxchanges, difftime, difftail, diffhead,
			 *nsteps, *MH_interval, *burnin, *interval,
			 *fVerbose);
   
  /* record new generated network to pass back to R */

  if(*status == MCMCDyn_OK && *maxedges>0 && newnetworktails && newnetworkheads)
    newnetworktails[0]=newnetworkheads[0]=EdgeTree2EdgeList(newnetworktails+1,newnetworkheads+1,nw,*maxedges-1);

  if(diffnetworktime==NULL){
    free(difftime);
    free(difftail);
    free(diffhead);
  }

  MCMCDyn_finish_common(nw, F_m, D_m, &F_MH, &D_MH);

}

/*********************
 MCMCDynStatus MCMCSampleDyn

 Using the parameters contained in the array F_theta, obtain the
 network statistics for a sample of size nsteps.  burnin is the
 initial number of Markov chain steps before sampling anything
 and interval is the number of MC steps between successive 
 networks in the sample.  Put all the sampled statistics into
 the F_statistics array. 
*********************/
MCMCDynStatus MCMCSampleDyn(// Observed and discordant network.
		   Network *nwp,
		   // Formation terms and proposals.
		   Model *F_m, MHproposal *F_MH,
		   double *F_theta,
		   // Dissolution terms and proposals.
		   Model *D_m, MHproposal *D_MH,
		   double *D_theta,
		   // Space for output.
		   double *F_stats, double *D_stats, // Do we still need these?
		   Edge maxedges,
		   Edge maxchanges,
		   Vertex *difftime, Vertex *difftail, Vertex *diffhead,		    
		   // MCMC settings.
		   unsigned int nsteps, unsigned int MH_interval,
		   unsigned int burnin, unsigned int interval, 
		   // Verbosity.
		   int fVerbose){

  int i, j, log_toggles = (burnin==0 && interval==1);
  Edge nextdiffedge=1;

  if (fVerbose)
    Rprintf("Total m->n_stats is %i; total nsteps is %d\n",
	    F_m->n_stats,nsteps);
  
  
  /* Burn in step. */

  for(i=0;i<burnin;i++)
    MCMCDyn1Step(nwp,
		 F_m, F_MH, F_theta, D_m, D_MH, D_theta,
		 log_toggles, F_stats, D_stats,
		 maxchanges, &nextdiffedge, difftime, difftail, diffhead,
		 MH_interval, fVerbose);
  
  // If we need to return a network, then stop right there, since the network is too big to return, so stop early.
  if(maxedges!=0 && nwp->nedges >= maxedges-1)
    return MCMCDyn_TOO_MANY_EDGES;
  
  //Rprintf("MCMCSampleDyn post burnin numdissolve %d\n", *numdissolve);
  
  if (fVerbose){
    Rprintf("Returned from Metropolis-Hastings burnin\n");
  }
  
  /* Now sample networks */
  for (i=0; i < nsteps; i++){
    /* Set current vector of stats equal to previous vector */
    for (j=0; j<F_m->n_stats; j++){
      F_stats[j+F_m->n_stats] = F_stats[j];
    }
    F_stats += F_m->n_stats;

    for (j=0; j<D_m->n_stats; j++){
      D_stats[j+D_m->n_stats] = D_stats[j];
    }
    D_stats += D_m->n_stats;

    /* This then adds the change statistics to these values */
    for(j=0;j<interval;j++){
      MCMCDyn1Step(nwp,
		   F_m, F_MH, F_theta, D_m, D_MH, D_theta,
		   log_toggles, F_stats, D_stats,
		   maxchanges, &nextdiffedge, difftime, difftail, diffhead,
		   MH_interval, fVerbose);

      // Check that we didn't run out of log space.
      if(log_toggles && nextdiffedge >= maxchanges) 
        return MCMCDyn_TOO_MANY_CHANGES;
      
      // If we need to return a network, then stop right there, since the network is too big to return, so stop early.
      if(maxedges!=0 && nwp->nedges >= maxedges-1)
	return MCMCDyn_TOO_MANY_EDGES;

    }
    
    //Rprintf("MCMCSampleDyn loop numdissolve %d\n", *numdissolve);
    if (fVerbose){
      if( ((3*i) % nsteps)<3 && nsteps > 500){
        Rprintf("Advanced %d time steps.\n", i);
      }
    }
  }

  if(log_toggles) difftime[0]=difftail[0]=diffhead[0]=nextdiffedge-1;
  return MCMCDyn_OK;
}

/* Helper function to run the formation or the dissolution side of the process, 
   depending on proposal, statistics, and parameters passed. 

   NOTE: Here, "stats" and "m" are for the process that "drives" the sampling 
   in this phase, while "O_stats" and "O_m" are for the other process (whose
   statistcs are still affected and need to be updated).
*/
/*R_INLINE*/ void MCMCDyn1Step_sample(MHproposal *MH,
				  double *par, double *stats,
				  int MH_interval, 
				  Network *nwp,
				  Model *m){
  Vertex step;
  double cutoff, ip;
  unsigned int i;
  
  MH->ntoggles = 0;
  (*(MH->func))(MH, nwp); /* Call MH proposal function to initialize */
  
  for(step = 0; step < MH_interval; step++) {
    
    MH->logratio = 0;
    (*(MH->func))(MH, nwp); /* Call MH function to propose toggles */
    //      Rprintf("Back from proposal; step=%d\n",step);

    // Proposal failed.
    if(MH->toggletail[0]==MH_FAILED){
      if(MH->togglehead[0]==MH_UNRECOVERABLE) 
	error("Something very bad happened during proposal. Memory has not been deallocated, so restart R soon.");
      if(MH->togglehead[0]==MH_IMPOSSIBLE) break;
    }

    ChangeStats(MH->ntoggles, MH->toggletail, MH->togglehead, nwp, m);
    
    //  Rprintf("change stats:"); 
    /* Calculate inner product */
    for (i=0, ip=0.0; i<m->n_stats; i++){
      ip += par[i] * m->workspace[i];
      //  Rprintf("%f ", m->workspace[i]); 
    }
    //  Rprintf("\n ip %f dedges %f\n", ip, m->workspace[0]); 
    /* The logic is to set exp(cutoff) = exp(ip) * qratio ,
       then let the MH probability equal min{exp(cutoff), 1.0}.
       But we'll do it in log space instead.  */
    cutoff = ip + MH->logratio;
    
    /* if we accept the proposed network */
    if (cutoff >= 0.0 || log(unif_rand()) < cutoff) { 
      /* Hold off updating timesteamps until the changes are committed,
      which doesn't happen until later. */
      for (i=0; i < MH->ntoggles; i++){
        ToggleEdge(MH->toggletail[i], MH->togglehead[i], &nwp[0]);
        ToggleEdge(MH->toggletail[i], MH->togglehead[i], &nwp[1]);  /* Toggle the discord for this edge */
      }
      /* Do NOT record network statistics for posterity yet. */
    }
  }
}

/*
  MCMCDyn1Step_commit
  Applies a list of toggles to a network, updating change statistics and time stamps.
*/
/*R_INLINE*/ void MCMCDyn1Step_commit(unsigned int ntoggles,
				  Vertex *difftail, Vertex *diffhead,
				  Network *nwp,
				  Model *F_m, double *F_stats,
				  Model *D_m, double *D_stats){
  ChangeStats(ntoggles,difftail,diffhead,nwp,F_m);
  ChangeStats(ntoggles,difftail,diffhead,nwp,D_m);
  for (unsigned int i = 0; i < F_m->n_stats; i++)
    F_stats[i] += F_m->workspace[i];
  for (unsigned int i = 0; i < D_m->n_stats; i++)
    D_stats[i] += D_m->workspace[i];

  for(Edge i=0;i<ntoggles;i++){
    ToggleEdgeWithTimestamp(difftail[i],diffhead[i],nwp);
  }
}

/* 
   MCMCDyn1Step_record_reset
   Record a set of changes to the state of the sampler at the end of a phase:
   * record new generated network differences to pass back to R
   * undo the toggles to nwp[0]
   * empty the discordant network (nwp[1])
   * return the number of edges toggled
*/
/*R_INLINE*/ unsigned int MCMCDyn1Step_record_reset(Edge maxchanges,
						Vertex *difftime, Vertex *difftail, Vertex *diffhead,
						Network *nwp, 
						Edge *nextdiffedge){
  Vertex tail, head;
  const unsigned int t=nwp->duration_info.MCMCtimer;
  Edge ntoggles = nwp[1].nedges;
  
  for(unsigned int i=0; i<ntoggles; i++){
    FindithEdge(&tail, &head, 1, &nwp[1]); // Grab the next edge that changed;
    ToggleEdge(tail, head, &nwp[1]); // delete it from nwp[1];
    ToggleEdge(tail, head, nwp); // undo the toggle in nwp[0];
    
    if(*nextdiffedge<maxchanges){
      // and record the toggle.
      difftime[*nextdiffedge] = t;
      difftail[*nextdiffedge] = tail;
      diffhead[*nextdiffedge] = head;
      (*nextdiffedge)++;
    }
  }
  return(ntoggles);
}

/*********************
 void MCMCDyn1Step

 Simulate evolution of a dynamic network for nsteps steps.
*********************/
void MCMCDyn1Step(// Observed and discordant network.
			   Network *nwp,
			   // Formation terms and proposals.
			   Model *F_m, MHproposal *F_MH, double *F_theta,
			   // Dissolution terms and proposals.
			   Model *D_m, MHproposal *D_MH, double *D_theta,
			   // Space for output.
			   unsigned log_toggles,
			   double *F_stats, double *D_stats,
			   unsigned int maxchanges, Edge *nextdiffedge,
			   Vertex *difftime, Vertex *difftail, Vertex *diffhead,
			   // MCMC settings.
			   unsigned int MH_interval,
			   // Verbosity.
			   int fVerbose){
  
  Edge ntoggles;

  Edge nde=1;
  if(nextdiffedge) nde=*nextdiffedge;
  
  /* Increment the MCMC timer. */
  nwp->duration_info.MCMCtimer++;

  /* Run the dissolution process. */
  MCMCDyn1Step_sample(D_MH, D_theta, D_stats, MH_interval, nwp, D_m);
  ntoggles = MCMCDyn1Step_record_reset(maxchanges, difftime, difftail, diffhead, nwp, &nde);
  
  /* Run the formation process. */
  MCMCDyn1Step_sample(F_MH, F_theta, F_stats, MH_interval, nwp, F_m);
  ntoggles += MCMCDyn1Step_record_reset(maxchanges, difftime, difftail, diffhead, nwp, &nde);
  
  /* Commit both. */
  MCMCDyn1Step_commit(ntoggles, difftail+nde-ntoggles, diffhead+nde-ntoggles, nwp, F_m, F_stats, D_m, D_stats);
  
  // If we don't keep a log of toggles, reset the position to save space.
  if(log_toggles)
    *nextdiffedge=nde;
}
