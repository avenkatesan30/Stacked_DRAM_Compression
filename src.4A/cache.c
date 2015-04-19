#include "cache.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
extern uns64 inst_id;
extern uns64 cycle; // You can use this as timestamp for LRU
#define line_size 70
#define DCACHE_DECOMPRESSION_LATENCY 5
#define DCACHE_HIT_LATENCY   42
#define DCACHE_MISS_LATENCY  200
#define BYPASS_LEADER_SETS 32
long long int THRESHOLD = 262000;
long long int GCP;
double hit_rate_1=1.0;
double hit_rate_2=1.0;
uns64 hit_count_1=1;
uns64 hit_count_2=1;
double miss_rate=1.0;
uns64 miss_count=1;
uns64 sim_inst_count=0;
uns64 compression_count=0;
uns64 dontcompress_count=0;
uns64 PSEL_bypass = 512;
bool evict_flag = FALSE;//evict everything has not been called
unsigned long long int bypass_count = 0;
unsigned long long int no_bypass_count = 0;
unsigned long long int num_regions;
uns64 leader_bypass = 0;
uns64 leader_no_bypass = 0;


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
   num_regions = c->num_sets/BYPASS_LEADER_SETS;

   uns64 ii;
   for(ii=0;ii<c->num_sets;ii++)
   {
    if(ii % (num_regions+1) == 0)
      leader_bypass++;
    else if(ii % (num_regions-1) == 0)
      leader_no_bypass++;
   }
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
  printf("\n INST ID %llu ",inst_id);
  printf("\nCompression Count %llu- - - - - Dont Compress Count %llu ",compression_count,dontcompress_count);
  printf("\nBypass count %llu- - - - -  No Bypass Count %llu", bypass_count, no_bypass_count);
  printf("\nLeader sets for bypass %llu - - - - -  Leader sets for no bypass %llu",leader_bypass,leader_no_bypass);
  printf("\n");
}

bool tag_match(atd a[],Addr tag)
{
	 int ii;
     for(ii=0;ii<5;ii++)
     {
    	 if(a[ii].valid && a[ii].tag == tag)
    	 {
    		 return TRUE;
    	 }
     }
     return FALSE;
}

////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Return HIT if access hits in the cache, MISS otherwise 
// Also if is_write is TRUE, then mark the resident line as dirty
// Update appropriate stats
////////////////////////////////////////////////////////////////////
void train_bypass_predictor(int category, Cache *c, int line_num, Addr tag)
{
     int ii;
     for(ii=0;ii<5;ii++)
     {
    	 if(category == 0)//leader bypass
    	 {
    		 if(c->sets[line_num].line[0].bypass_candidates[0].tag == tag)//bypassing is ineffective
    		 {
    			 if(PSEL_bypass<1024) PSEL_bypass++;
    		 }
    		 else if(tag_match(c->sets[line_num].line[0].eviction_candidates,tag))
    		 {
    			 if(PSEL_bypass>0) PSEL_bypass--;
    		 }
    	 }
    	 else
    	 {
    		 if(c->sets[line_num].line[0].eviction_candidates[0].tag == tag)//bypassing is ineffective
    		 {
    			 if(PSEL_bypass<1024) PSEL_bypass++;
    		 }
    		 else if(tag_match(c->sets[line_num].line[0].bypass_candidates,tag))
    		 {
    			 if(PSEL_bypass>0) PSEL_bypass--;
    		 }
    	 }
     }
}

Flag cache_access(Cache *c, Addr lineaddr, uns is_write, uns core_id){
  Flag outcome=MISS;
  outcome = cache_read(c,lineaddr);
  if(outcome == HIT)
  {
	  cycle += DCACHE_HIT_LATENCY;
  }
  else
  {
	  cycle += DCACHE_MISS_LATENCY;
  }
  //if(inst_id>=20000 && inst_id<=20150)
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
  int match_index = 0;
  int line_num   = lineaddr % c->num_sets;   /* cache index */
  Addr tag        = (Addr) lineaddr / c->num_sets;   /* cache tag */

   if(line_num % (num_regions+1)== 0)//Leader set for bypass
    {
      c->sets[line_num].line[0].bypass_category = 1;

    }
    else if(line_num % (num_regions-1) == 0)//Leader set for no bypass
    {
      c->sets[line_num].line[0].bypass_category = 2;
    }
    else
      c->sets[line_num].line[0].bypass_category = 0;

  if(c->sets[line_num].line[0].bypass_category!=0) //Train PSEL only on accesses for leader sets
  {
  train_bypass_predictor(c->sets[line_num].line[0].bypass_category,c,line_num,tag);
  //printf("PSEL=%llu\n",PSEL_bypass);
  }
  if(PSEL_bypass<512)
  bypass_count++;
  else
    no_bypass_count++;
  int cache_cold = FALSE;
  /*if(inst_id>=20000 && inst_id<=20150)
  {
   //set associative
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
                  if(c->sets[line_num].line[0].comp_cl[match_index].compressed_size < 64) //hit for a compressed line
                  {
                	  cycle += DCACHE_DECOMPRESSION_LATENCY;
                	  if(inst_id>sim_inst_count)
                	  {
						  if(lru_compute(c,line_num,tag)==1)
						  {
							  hit_count_1++;
							  dontcompress_count++;
							  hit_rate_1=(double)hit_count_1/(double)(c->stat_read_access+c->stat_write_access);
							  if(GCP>-THRESHOLD)
							  GCP-=1;//*((int)(hit_rate_1*10)+1)*1;//(int)((float)DCACHE_DECOMPRESSION_LATENCY/(float)DCACHE_DECOMPRESSION_LATENCY);
						  }
						  else
						  {
							  //printf("Compression Count for Hit is increasing\n");
							  compression_count++;
							  hit_count_2++;
							  hit_rate_2=(double)hit_count_2/(double)(c->stat_read_access+c->stat_write_access);
							  GCP+=40;//*((int)(hit_rate_2*10)+1);//(int)((float)DCACHE_MISS_LATENCY/(float)DCACHE_DECOMPRESSION_LATENCY);
							  if(GCP>THRESHOLD) GCP=THRESHOLD;
						  }
                	  }
                  }
                  c->sets[line_num].line[0].comp_cl[match_index].last_access_time = cycle;    // update lru time
                  return HIT;
              }
              else
              {
            	  return MISS; // cache block was empty so we don't need to evict cache block
              }
           }
    }
    if(inst_id>sim_inst_count)
    {
		  if(c->sets[line_num].line[0].comp_cl[match_index].compressed_size >= 56) //hit for a compressed line
		  {
			//printf("Compression Count for Miss is increasing\n");
			compression_count++;
			miss_count++;
			miss_rate=(double)miss_count/(double)(c->stat_read_access+c->stat_write_access);
			GCP+=40;//*((int)(miss_rate*10)+1);//(int)((float)DCACHE_MISS_LATENCY/(float)DCACHE_DECOMPRESSION_LATENCY);
			if(GCP>THRESHOLD) GCP=THRESHOLD;
		  }
    }
  return MISS;     
}

////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Install the line: determine victim using repl policy (LRU/RAND)
// copy victim into last_evicted_line for tracking writebacks
////////////////////////////////////////////////////////////////////
void bypass(Cache *c,int line_num,int e_index,Addr tag,int comp_data_size,bool flag)
{
	if(flag == TRUE)//bypass
	{
		c->sets[line_num].line[0].bypass_candidates[0].tag = tag;
		c->sets[line_num].line[0].bypass_candidates[0].valid = TRUE;
	}
	else
	{
		c->sets[line_num].line[0].comp_cl[e_index].tag = tag;
		c->sets[line_num].line[0].comp_cl[e_index].last_access_time   = cycle;
		c->sets[line_num].line[0].comp_cl[e_index].compressed_size = comp_data_size;
		c->sets[line_num].line[0].comp_cl[e_index].valid = TRUE;
		c->sets[line_num].line[0].comp_cl[e_index].dirty = FALSE;  // it should set a dirty bit correctly
	}
}
void print_tags(Cache *c, Addr line_num)
{
	int ii;
	printf("Bypass tags -->");
	for(ii=0;ii<5;ii++)
		printf("%llu\t",c->sets[line_num].line[0].bypass_candidates[ii].tag);
	printf("Eviction tags -->");
	for(ii=0;ii<5;ii++)
		printf("%llu\t",c->sets[line_num].line[0].eviction_candidates[ii].tag);
	printf("\n");
	return;
}
void cache_install(Cache *c, Addr lineaddr, int comp_data_size, uns is_write, uns core_id)
{
  //printf("%d",comp_data_size);
  //printf("\n");
  int line_num   = lineaddr % c->num_sets;   /* cache index */
  Addr tag        = (Addr) lineaddr / c->num_sets;   /* cache tag */
  uns e_index = 0;
  int jj,size = 0;
  //set duel category for bypass
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
    {
    	if(c->sets[line_num].line[0].bypass_category == 0 && PSEL_bypass<512)
    		return;
    	else
    		e_index = cache_find_victim(c, line_num, comp_data_size, core_id) ;
    }

  //if bypassed - store incoming tag in bypassed tag
  //else store incoming line in competitor tag
  if(c->sets[line_num].line[0].bypass_category == 0)//Follower set
  {
	  //no bypass - PSEL_bypass>=512
	 bypass(c,line_num,e_index,tag,comp_data_size,FALSE);
  }
  else if(c->sets[line_num].line[0].bypass_category == 2) //leader set for no bypass or when decision is no bypass
  {
	  bypass(c,line_num,e_index,tag,comp_data_size,FALSE); //no bypass
	  c->sets[line_num].line[0].eviction_candidates[0].tag = tag;
	  c->sets[line_num].line[0].eviction_candidates[0].valid = TRUE;
	  //Updating size occupied
	  for(jj=0;jj<5;jj++)
	    {
	  	  if(c->sets[line_num].line[0].comp_cl[jj].valid)
	  	  {
	  		 size += c->sets[line_num].line[0].comp_cl[jj].compressed_size + 6;
	  	  }
	    }
	    c->sets[line_num].line[0].size_occupied = size;
	    //print_tags(c,line_num);
  }
  else//leader set for bypass or when decision is bypass
  {
	  bypass(c,line_num,e_index,tag,comp_data_size,TRUE);
	  //print_tags(c,line_num);
  }
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

void flush_everything(atd a[])
{
	int ii;
	for(ii=0;ii<5;ii++)
	   a[ii].valid = FALSE;
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
    if(c->sets[set_index].line[0].bypass_category == 1)//leader with bypass
    {
    	flush_everything((c->sets[set_index].line[0].eviction_candidates));
    	c->sets[set_index].line[0].eviction_candidates[0].tag = c->sets[set_index].line[0].comp_cl[victim].tag;
    	c->sets[set_index].line[0].eviction_candidates[0].valid = TRUE;
    }
    else if(c->sets[set_index].line[0].bypass_category == 2)//leader no bypass
    {
    	flush_everything((c->sets[set_index].line[0].bypass_candidates));
    	c->sets[set_index].line[0].bypass_candidates[0].tag = c->sets[set_index].line[0].comp_cl[victim].tag;
    	c->sets[set_index].line[0].bypass_candidates[0].valid = TRUE;
    }
    //do nothing for follower
    return victim;
}

uns evict_everything(Cache *c, uns set_index, int num)//evicts every entry which has comp_data_size >= num
{
	uns victim = 0;
	evict_flag = TRUE; //has been called
	int ii;
	int ind = 0;

	/*Invalidation*/
	if(c->sets[set_index].line[0].bypass_category == 1)
		flush_everything((c->sets[set_index].line[0].eviction_candidates));
    else if(c->sets[set_index].line[0].bypass_category == 2)
    	flush_everything((c->sets[set_index].line[0].bypass_candidates));

	for(ii=0;ii<5;ii++)
	{
		if(c->sets[set_index].line[0].comp_cl[ii].valid && c->sets[set_index].line[0].comp_cl[ii].compressed_size >= num)
		{
			//before invalidation, check bypass prob
			//if bypass chosen - don't invalidate, instead store victim tags in competitor tags
			//else if bypass not chosen - invalidate and
			if(c->sets[set_index].line[0].bypass_category == 2 || c->sets[set_index].line[0].bypass_category == 0)
			c->sets[set_index].line[0].comp_cl[ii].valid = FALSE;
			victim = ii;
			if(c->sets[set_index].line[0].bypass_category == 1)//leader bypass
			{
				//store tags
				c->sets[set_index].line[0].eviction_candidates[ind].tag = c->sets[set_index].line[0].comp_cl[victim].tag;
				c->sets[set_index].line[0].eviction_candidates[ind].valid = TRUE;
			}
			else if(c->sets[set_index].line[0].bypass_category == 2)//leader no bypass
			{
				c->sets[set_index].line[0].bypass_candidates[ind].tag = c->sets[set_index].line[0].comp_cl[victim].tag;
			    c->sets[set_index].line[0].bypass_candidates[ind].valid = TRUE;
			}
			ind++;
		}
	}
	return victim;
}

int first_valid_index(atd a[])
{
	int ii;
	for(ii=0;ii<5;ii++)
	{
		if(!a[ii].valid)
			return ii;
	}
	return 0;
}

uns evict(Cache *c, uns set_index, int num, int count)
{
   uns victim = 0;
   int ind = 0,ii,jj;
   uns temp_array[5];

   /*Invalidate only when evict everything has not been called before*/
   if(evict_flag == FALSE)
   {
	   if(c->sets[set_index].line[0].bypass_category == 1)
		   flush_everything(c->sets[set_index].line[0].eviction_candidates);
	   else if(c->sets[set_index].line[0].bypass_category == 2)
		   flush_everything(c->sets[set_index].line[0].bypass_candidates);
   }

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
    	 if(c->sets[set_index].line[0].bypass_category == 2 || c->sets[set_index].line[0].bypass_category == 0)
         c->sets[set_index].line[0].comp_cl[jj].valid = 0;
         victim = jj;
         if(c->sets[set_index].line[0].bypass_category == 1)//leader bypass
         {
        	 //store tags
        	 ind = first_valid_index(c->sets[set_index].line[0].eviction_candidates);
        	 c->sets[set_index].line[0].eviction_candidates[ind].tag = c->sets[set_index].line[0].comp_cl[victim].tag;
        	 c->sets[set_index].line[0].eviction_candidates[ind].valid = TRUE;
         }
         else if(c->sets[set_index].line[0].bypass_category == 2)//leader no bypass
         {
        	 ind = first_valid_index(c->sets[set_index].line[0].bypass_candidates);
        	 c->sets[set_index].line[0].bypass_candidates[ind].tag = c->sets[set_index].line[0].comp_cl[victim].tag;
        	 c->sets[set_index].line[0].bypass_candidates[ind].valid = TRUE;
         }
         ind++;
         break;
       }
     }
   }
   return victim;
}

int lru_compute(Cache *c, uns set_index, Addr tag)
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
    if(c->sets[set_index].line[0].comp_cl[max_index].tag == tag)
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
	uns victim = 0;
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
	  evict_flag = FALSE;
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

