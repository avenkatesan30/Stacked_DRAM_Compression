#ifndef CORE_H
#define CORE_H

#include "types.h"
#include "memsys.h"
#include <zlib.h>
#include <stdbool.h>

typedef struct Core Core;



////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
typedef struct FTR_Record{
	 uint64_t inst_num; // last record should be around 4 billion instruction
	 uint64_t va;       // This must be line address
	 uint8_t iswb;      // for L4 RD miss this is 0, for L4 dirty eviction this is 1
	 uint32_t delay;    // delay = L2HITS*6 + L3HITS*20 + L4HITS*50 (reset HITS after dump)
	 char olddata[64];  // should be all zeros if iswb=0
	 char newdata[64];  // should be all zeros if iswb=0
}FTR_Record;

struct Core {
  uns   core_id;

  Memsys *memsys;
    
  char  trace_fname[1024];
  gzFile trace;
    
  uns   done;

  uint64_t  trace_inst_addr;
  bool  trace_inst_type;
  uint64_t  trace_ldst_addr;
  char   trace_ldst_data[64];
  
  uns64 snooze_end_cycle; // when waiting for data to return

  uns64 inst_count;
  uns64 done_inst_count;
  uns64 done_cycle_count;
};



//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

Core  *core_new(Memsys *memsys, char *trace_fname, uns core_id);
void   core_cycle(Core *core);
void   core_print_stats(Core *c);
void   core_read_trace(Core *c);
void   core_init_trace(Core *c);

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

#endif // CORE_H
