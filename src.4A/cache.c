#include "cache.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

extern uns64 cycle; // You can use this as timestamp for LRU
#define line_size 70
#define DCACHE_DECOMPRESSION_LATENCY 5
#define DCACHE_HIT_LATENCY   42
#define DCACHE_MISS_LATENCY  100


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
  printf("\n%s_TOTAL_MISS_RATE \t\t : %10.3f", header, (double)(c->stat_write_miss + c->stat_read_miss)/(double)(c->stat_read_access+c->stat_write_access)*100);
  printf("\n%s_TOTAL_HIT_RATE \t\t : %10.3f", header, 100 - (double)(c->stat_write_miss + c->stat_read_miss)/(double)(c->stat_read_access+c->stat_write_access)*100);
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
  if(outcome == HIT)
	  cycle += DCACHE_HIT_LATENCY;
  else
	  cycle += DCACHE_MISS_LATENCY;
  //if(cycle>=6425700 && cycle<=6425800)
  //printf("--Access %d\n",outcome);
  if(is_write == 1)
  {
    c->stat_write_access++;
    if(outcome == MISS)
     c->stat_write_miss++;
  } 
  else
  {
    c->stat_read_access++;
    if(outcome == MISS)
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
 /* if(cycle>=6425700 && cycle<=6425800)
  {
   set associative 
    for(jj=0;jj<5;jj++)
    {
    	printf("%llu-%d-%d\t",c->sets[line_num].line[0].comp_cl[jj].tag,c->sets[line_num].line[0].comp_cl[jj].valid,c->sets[line_num].line[0].size_occupied);
    }
    printf("   ---  incoming tag - %llu ---Line number-%d",tag,line_num);
  }*/
    for(jj=0;jj<5;jj++)
    {
    	   
           if(c->sets[line_num].line[0].comp_cl[jj].tag == tag)
           {   // tag match
        	   //printf("\nTag match");
              if(c->sets[line_num].line[0].comp_cl[jj].valid == FALSE)
              {
            	  //printf("\ncache is cold");
                // data should be valid
            	  if(jj<4)
            	  continue;
                   cache_cold = TRUE;
                   match_index = jj;
              }
              if(!cache_cold)
              {
                  c->sets[line_num].line[jj].comp_cl[match_index].last_access_time = cycle;    // update lru time
                  if(c->sets[line_num].line[jj].comp_cl[match_index].compressed_size < 64) //hit for a compressed line
                	  cycle += DCACHE_DECOMPRESSION_LATENCY;
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
  //printf("%d",comp_data_size);
  //printf("\n");
  int line_num   = lineaddr % c->num_sets;   /* cache index */
  Addr tag        = (Addr) lineaddr / c->num_sets;   /* cache tag */
  uns e_index = 0;
  int jj,size = 0;
  //if incoming cache line's compressed size can be fit in without exceeding the limit - just directly install
    if((comp_data_size + 6) <= line_size - c->sets[line_num].line[0].size_occupied)
    {
    	  //printf("\nInside");
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
		 size += c->sets[line_num].line[0].comp_cl[jj].compressed_size + 6;
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

uns evict_everything(Cache *c, uns set_index, int num)//evicts every entry which has comp_data_size >= num
{
	uns victim = 0;
	int ii;
	for(ii=0;ii<5;ii++)
	{
		if(c->sets[set_index].line[0].comp_cl[ii].valid && c->sets[set_index].line[0].comp_cl[ii].compressed_size >= num)
		{
			c->sets[set_index].line[0].comp_cl[ii].valid = 0;
			victim = ii;
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
	int num_eights,num_sixteens,num_24s;
	if(comp_data_size == 64 || comp_data_size == 56)
	   victim = evict_everything(c,set_index,8); //evict everything that has compressed size greater than or equal to 8
	else if(comp_data_size == 48) //can co-exist with only a single 8
	{
		//evict everything that is not an 8
		victim = evict_everything(c,set_index,16);
		num_eights = count_nums(c,set_index,8);
		//Retain just one 8 and evict everything else - evict (n-1) 8's
		if(num_eights>1) victim = evict(c,set_index,8,num_eights-1);
	}
	else if(comp_data_size == 40)
	{
		//Evict everything other than 16 and 8
		victim = evict_everything(c,set_index,24);
		num_sixteens = count_nums(c,set_index,16);
		num_eights = count_nums(c,set_index,8);
		if(num_sixteens > 1)
		{
			if(num_eights == 0)
			  victim = evict(c,set_index,16,num_sixteens-1);
			else
			   victim = evict(c,set_index,16,num_sixteens);
		}
		else if(num_sixteens == 1)
		{
			if(num_eights!=0)
			{
				//evict the 16 and m-1 LRU 8's
				victim = evict(c,set_index,16,1);
				if(num_eights>1) victim = evict(c,set_index,8,num_eights-1);
			}
		}
		else //Evict m-1 LRU 8's
		{
			if(num_eights>1)	victim = evict(c,set_index,8,num_eights-1);
		}
	}
	else if(comp_data_size == 32)
	{
	    //can co-exist with one 8, two 8s, one 16, one 24
		num_24s = count_nums(c,set_index,24);
		num_sixteens = count_nums(c,set_index,16);
		num_eights = count_nums(c,set_index,8);
		if(num_24s == 2) //evict LRU 24
			victim = evict(c,set_index,24,1);
		else if(num_24s == 1)
		{
			//evict 24
			victim = evict(c,set_index,24,1);
			if(num_sixteens == 1 && num_eights == 1) //evict 16
				victim = evict(c,set_index,16,1);
		}
		else
		{
			if(num_sixteens == 3 || num_sixteens == 2) //evict 2 LRU 16s
			    victim = evict(c,set_index,16,2);
			else if(num_sixteens == 1)
			{
				//evict the 16
				victim = evict(c,set_index,16,1);
				if(num_eights == 3) //evict one 8
					victim = evict(c,set_index,8,1);
			}
			else //evict r-2 8's
			{
				if(num_eights>2) victim = evict(c,set_index,8,num_eights-2);
			}
		}
	}
	else if(comp_data_size == 24)
	{
		num_sixteens = count_nums(c,set_index,16);
		num_eights = count_nums(c,set_index,8);
		if(num_sixteens > 1) //evict n-1 16s
			victim = evict(c,set_index,16,num_sixteens-1);
		else if(num_sixteens == 1)
		{
			//evict the 16
			victim = evict(c,set_index,16,1);
			if(num_eights == 3) //evict one 8
				victim = evict(c,set_index,8,1);
		}
		else //evict r-2 8's
		{
			if(num_eights>2) victim = evict(c,set_index,8,num_eights-2);
		}
	}
	else //comp_data_size == 16
	{
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
     // printf("%d-%u-  \t",jj, c->sets[set_index].line[0].comp_cl[jj].last_access_time);
      ind++;
      flag = 1;
    }
    
  }
  
  if(!flag)
  {
	//printf("\nCalling comb victim");
    //OLD CODE//
	  victim = get_comb_victim(c,comp_data_size,set_index);
    ///////////
	  //uns temp_arr[5]={0,1,2,3,4};
	    //victim = lru_repl(c, temp_arr, 5, set_index);
	    //printf("%d\n",victim);
	 
  }
  else
  {
    //printf("\nCalling lru victim");
    victim = lru_repl(c, replacement_candidates, ind, set_index);
    //printf("%d\n",victim);
  }
  //printf("\n");
  return victim;
}

