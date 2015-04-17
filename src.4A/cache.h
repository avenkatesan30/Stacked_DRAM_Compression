#ifndef CACHE_H
#define CACHE_H

#include "types.h"
#include <string.h>

#define MAX_WAYS 16

typedef struct Cache_Line Cache_Line;
typedef struct Cache_Set Cache_Set;
typedef struct Cache Cache;


//////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////
typedef struct ATD
{
    Addr tag;
	bool valid;
}atd;

typedef struct Cache_Line {
    Flag    valid;
    Flag    dirty;
    Addr    tag;
    uns     core_id;
    uns     last_access_time; // for LRU
    int 	compressed_size;
    char    data[64];
   // Note: No data as we are only estimating hit/miss 
}cache_line;

typedef struct Big_cache_line {
	cache_line comp_cl[5];
	//int num_occupied_slots;
	int size_occupied;
	atd eviction_candidates[5];
	atd bypass_candidates[5];
	int bypass_category; // 0 for bypass, 1 for no bypass, 2 for follower
}big_cache_line;

struct Cache_Set {
    big_cache_line line[MAX_WAYS];
};


struct Cache{
  uns64 num_sets;
  uns64 num_ways;
  uns64 repl_policy;
  
  Cache_Set *sets;
  Cache_Line last_evicted_line; // for checking writebacks

  //stats
  uns64 stat_read_access; 
  uns64 stat_write_access; 
  uns64 stat_read_miss; 
  uns64 stat_write_miss; 
  uns64 stat_dirty_evicts; // how many dirty lines were evicted?
};


/////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

Cache  *cache_new(uns64 size, uns64 assocs, uns64 linesize, uns64 repl_policy);
Flag    cache_access         (Cache *c, Addr lineaddr, uns is_write, uns core_id);
void    cache_install        (Cache *c, Addr lineaddr, int comp_data_size, uns is_write, uns core_id);
void    cache_print_stats    (Cache *c, char *header);
Flag    cache_read           (Cache *c, Addr lineaddr);
uns cache_find_victim        (Cache *c, uns set_index, int comp_data_size, uns core_id);
int lru_compute(Cache *c, uns set_index, Addr tag);

//////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////

#endif // CACHE_H
