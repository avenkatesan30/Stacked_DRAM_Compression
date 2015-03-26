#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
//Ajit Made this change
#include "cache.h"
extern uns64 cycle; // You can use this as timestamp for LRU

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE INIT FUNCTION -----------
////////////////////////////////////////////////////////////////////

Cache  *cache_new(uns64 size, uns64 assoc, uns64 linesize, uns64 repl_policy){

   Cache *c = (Cache *) calloc (1, sizeof (Cache));
   c->num_ways = assoc;
   c->repl_policy = repl_policy;

   if(c->num_ways > MAX_WAYS){
     printf("Change MAX_WAYS in cache.h to support %llu ways\n", c->num_ways);
     exit(-1);
   }

   // determine num sets, and init the cache
   c->num_sets = size/(linesize*assoc);
   c->sets  = (Cache_Set *) calloc (c->num_sets, sizeof(Cache_Set));

   return c;
}

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE PRINT STATS FUNCTION -----------
////////////////////////////////////////////////////////////////////

void    cache_print_stats    (Cache *c, char *header){
  double read_mr =0;
  double write_mr =0;

  if(c->stat_read_access){
    read_mr=(double)(c->stat_read_miss)/(double)(c->stat_read_access);
  }

  if(c->stat_write_access){
    write_mr=(double)(c->stat_write_miss)/(double)(c->stat_write_access);
  }

  printf("\n%s_READ_ACCESS    \t\t : %10llu", header, c->stat_read_access);
  printf("\n%s_WRITE_ACCESS   \t\t : %10llu", header, c->stat_write_access);
  printf("\n%s_READ_MISS      \t\t : %10llu", header, c->stat_read_miss);
  printf("\n%s_WRITE_MISS     \t\t : %10llu", header, c->stat_write_miss);
  printf("\n%s_TOTAL_MISSES   \t\t : %10llu", header, c->stat_write_miss + c->stat_read_miss);
  printf("\n%s_READ_MISSPERC  \t\t : %10.3f", header, 100*read_mr);
  printf("\n%s_WRITE_MISSPERC \t\t : %10.3f", header, 100*write_mr);
  printf("\n%s_TOTAL_MISSPERC \t\t : %10.3f", header, 100*read_mr + 100*write_mr);
  printf("\n%s_DIRTY_EVICTS   \t\t : %10llu", header, c->stat_dirty_evicts);//???

  printf("\n");
}



////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Return HIT if access hits in the cache, MISS otherwise 
// Also if is_write is TRUE, then mark the resident line as dirty
// Update appropriate stats
////////////////////////////////////////////////////////////////////

Flag cache_access(Cache *c, Addr lineaddr, uns is_write, uns core_id){	
  Flag outcome=MISS;
  outcome = cache_read(c,lineaddr);
  if(is_write == 1)
  {
	  c->stat_write_access++;
	  if(outcome == FALSE)
		 c->stat_write_miss++;
  } 
  else
  {
	  c->stat_read_access++;
	  if(outcome == FALSE)
		 c->stat_read_miss++;
  }
  return outcome;
}

Flag cache_read(Cache *c, Addr lineaddr)
{   
  uns64 ii;
  int line_num   = lineaddr % c->num_sets;   /* cache index */
  Addr tag        = (Addr) lineaddr / c->num_sets;   /* cache tag */
  int cache_cold = FALSE;
  /* set associative */ 
  for(ii = 0; ii < c->num_ways; ii++) {
    if(c->sets[line_num].line[ii].tag == tag ) {   // tag match 
      if(c->sets[line_num].line[ii].valid == FALSE) {  // data should be valid 
	cache_cold = TRUE;                      
      }
      //cache->cache_entry[line_num][ii].lru = cycle_count;    // update lru time 
      //cache->cache_entry[line_num][ii].valid = true;

      if(!cache_cold) { 
	c->sets[line_num].line[ii].last_access_time = cycle;    // update lru time 
	return HIT;
      }
      else return MISS; // cache block was empty so we don't need to evict cache block 
    }
  }
    
  return MISS;     
}

////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Install the line: determine victim using repl policy (LRU/RAND)
// copy victim into last_evicted_line for tracking writebacks
////////////////////////////////////////////////////////////////////

void cache_install(Cache *c, Addr lineaddr, uns is_write, uns core_id){

  // Your Code Goes Here
  // Find victim using cache_find_victim
  // Initialize the evicted entry
  // Initialize the victime entry
	
    int line_num   = lineaddr % c->num_sets;   /* cache index */
    Addr tag        = (Addr) lineaddr / c->num_sets;   /* cache tag */

    /* cache replacement */
    int e_index = cache_find_victim(c, line_num, core_id) ;    


    if(c->sets[line_num].line[e_index].valid == TRUE) {      
      if(c->sets[line_num].line[e_index].dirty) {

        // for a write-back cache, we have to insert a new memory request into the memory system 
        // for this assignment, we will just drop this request 
	 
      }
       
    }

    c->sets[line_num].line[e_index].tag   = tag;
    c->sets[line_num].line[e_index].last_access_time   = cycle;
    c->sets[line_num].line[e_index].valid = TRUE;
    c->sets[line_num].line[e_index].dirty = FALSE;  // it should set a dirty bit correctly 

 
}

////////////////////////////////////////////////////////////////////
// You may find it useful to split victim selection from install
////////////////////////////////////////////////////////////////////


uns cache_find_victim(Cache *c, uns set_index, uns core_id){
  uns victim=0;
  uns64 ii;
  for (ii = 0; ii < c->num_ways; ii++) {
	  if(c->sets[set_index].line[ii].last_access_time < c->sets[set_index].line[victim].last_access_time) {
		  victim = ii;
	  }
  }
  
  return victim;
}

