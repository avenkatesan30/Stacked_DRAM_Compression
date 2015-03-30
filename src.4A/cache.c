#include "cache.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern uns64 cycle; // You can use this as timestamp for LRU
#define line_size 70

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
  uns64 jj;
  int match_index;
  int line_num   = lineaddr % c->num_sets;   /* cache index */
  Addr tag        = (Addr) lineaddr / c->num_sets;   /* cache tag */
  int cache_cold = FALSE;
  /* set associative */ 
    for(jj=0;jj<5;jj++)
    {
           if(c->sets[line_num].line[0].comp_cl[jj].tag == tag)
           {   // tag match
              if(c->sets[line_num].line[0].comp_cl[jj].valid == FALSE)
              {
                // data should be valid
                   cache_cold = TRUE;
                   match_index = jj;
              }
              if(!cache_cold)
              {
                  c->sets[line_num].line[jj].comp_cl[match_index].last_access_time = cycle;    // update lru time
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

void cache_install(Cache *c, Addr lineaddr, int comp_data_size, uns is_write, uns core_id)
{
  int line_num   = lineaddr % c->num_sets;   /* cache index */
  Addr tag        = (Addr) lineaddr / c->num_sets;   /* cache tag */
  uns e_index = 0;
  int jj,size = 0;
  //if incoming cache line's compressed size can be fit in without exceeding the limit - just directly install
    if(comp_data_size <= line_size - c->sets[line_num].line[0].size_occupied)
    {
          for(jj=0;jj<5;jj++)
          {
           if(c->sets[line_num].line[0].comp_cl[jj].valid == 0)
           {
        	   e_index = jj;
        	   break;
           }
          }
    }
    else
       e_index = cache_find_victim(c, line_num, comp_data_size, core_id) ;

  c->sets[line_num].line[0].comp_cl[e_index].tag = tag;
  c->sets[line_num].line[0].comp_cl[e_index].last_access_time   = cycle;
  c->sets[line_num].line[0].comp_cl[e_index].compressed_size = comp_data_size;
  c->sets[line_num].line[0].comp_cl[e_index].valid = TRUE;
  c->sets[line_num].line[0].comp_cl[e_index].dirty = FALSE;  // it should set a dirty bit correctly

  for(jj=0;jj<5;jj++)
  {
	  if(c->sets[line_num].line[0].comp_cl[jj].valid)
	  {
		 size += c->sets[line_num].line[0].comp_cl[jj].compressed_size;
	  }
  }
  c->sets[line_num].line[0].size_occupied = size;
}

////////////////////////////////////////////////////////////////////
// You may find it useful to split victim selection from install
////////////////////////////////////////////////////////////////////

int compare(const void *a, const void *b)
{
    const uns *x = a, *y = b;
    if(*x > *y)
        return 1;
    else
        return (*x < *y) ? -1 : 0;
}

uns lru_repl(Cache *c, uns replacement_candidates[], int size, uns set_index)
{
  int ii;
  uns victim = replacement_candidates[0];
    for (ii = 0; ii < size; ii++)
    {
      if(c->sets[set_index].line[0].comp_cl[replacement_candidates[ii]].last_access_time < c->sets[set_index].line[0].comp_cl[victim].last_access_time)
      {
        victim = replacement_candidates[ii];
      }
    }
    return victim;
}

uns evict(Cache *c, uns set_index, int num, int count)
{
   uns victim = 0;
   int ind = 0,ii,jj;
   uns temp_array[5];
   for(ii=0;ii<5;ii++)
   {
       if(c->sets[set_index].line[0].comp_cl[ii].valid && c->sets[set_index].line[0].comp_cl[ii].compressed_size == num)
       {
               temp_array[ind] = c->sets[set_index].line[0].comp_cl[ii].last_access_time;
               ind++;
       }
   }
   qsort(temp_array,ind,sizeof(uns),compare);
   for(ii=0;ii<count;ii++)
   {
     for(jj=0;jj<5;jj++)
     {
       if(c->sets[set_index].line[0].comp_cl[jj].last_access_time == temp_array[ii])
       {
         c->sets[set_index].line[0].comp_cl[jj].valid = 0;
         victim = jj;
         break;
       }
     }
   }
   return victim;
}

int lru_compute(Cache *c, uns set_index, int num)
{
  int ii;
  uns max_timestamp = c->sets[set_index].line[0].comp_cl[0].last_access_time;
    int max_index = 0;
    for(ii=0;ii<5;ii++)
    {
       if(c->sets[set_index].line[0].comp_cl[ii].valid && c->sets[set_index].line[0].comp_cl[ii].last_access_time > max_timestamp)
      {

        max_timestamp = c->sets[set_index].line[0].comp_cl[ii].last_access_time;
        max_index = ii;
      }
    }
    if(c->sets[set_index].line[0].comp_cl[max_index].compressed_size == 8)
      return 1;
    else
      return 0;
}

int count_nums(Cache *c, uns set_index, int num)
{
  int ii;
  int count=0;
  for(ii=0;ii<5;ii++)
  {
    if(c->sets[set_index].line[0].comp_cl[ii].valid && c->sets[set_index].line[0].comp_cl[ii].compressed_size==num)
      count++;
  }
  return count;
}

uns get_comb_victim(Cache *c, int comp_data_size, uns set_index)
{
  uns victim;
  if(comp_data_size == 38)
  {
	//printf("\nHi");
    int num_eights = count_nums(c,set_index,8);
    if(num_eights == 5 || num_eights == 4)
           victim = evict(c,set_index,8,4); //evict 4 best 8s
    else if(num_eights == 3)
    {
      if(count_nums(c,set_index,16) == 0)
         victim = evict(c,set_index,8,3); //evict 3 8s
      else
      {
        victim = evict(c,set_index,16,1);
        victim = evict(c,set_index,8,2);
      }
    }
    else if(num_eights == 2)
      victim = evict(c,set_index,16,1);
    else if(num_eights == 1)
    {
      if(!lru_compute(c,set_index,8))
      {
        victim = evict(c,set_index,8,1);
          victim = evict(c,set_index,16,1);
      }
        else
      victim = evict(c,set_index,16,2);
    }
    else
    {
      if(count_nums(c,set_index,16)==3)
          victim = evict(c,set_index,16,2);
      else
        victim = evict(c,set_index,16,1);
    }
  }
  else //comp_data_size = 16
  {
	//printf("\nHello");
    if(count_nums(c,set_index,8)==4)
      victim = evict(c,set_index,8,1);
    else
      victim = evict(c,set_index,8,2);
  }
  return victim;
}

uns cache_find_victim(Cache *c, uns set_index, int comp_data_size, uns core_id)
{
  //printf("%d\n",comp_data_size);
  uns victim=0;
  int jj,flag = 0;
  uns replacement_candidates[5];
  int ind=0;
  for(jj=0;jj<5;jj++)
  {
    if(c->sets[set_index].line[0].comp_cl[jj].valid && c->sets[set_index].line[0].comp_cl[jj].compressed_size>=comp_data_size)
    {
      replacement_candidates[ind]=jj;
      ind++;
      flag = 1;
    }
  }
  if(!flag)
    victim = get_comb_victim(c,comp_data_size,set_index);
  else
    victim = lru_repl(c, replacement_candidates, ind, set_index);
  return victim;
}

