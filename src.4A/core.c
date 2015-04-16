#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <zlib.h>
#include "core.h"
FILE *infile;
extern uns64 cycle;

extern void die_message(const char * msg);
int flag = 0;
FTR_Record rec;
FTR_Record *ptr;
////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

Core *core_new(Memsys *memsys, char *trace_fname, uns core_id)
{
  Core *c = (Core *) calloc (1, sizeof (Core));
  c->core_id = core_id;
  c->memsys  = memsys;

  strcpy(c->trace_fname, trace_fname);
  core_init_trace(c);
  core_read_trace(c);

  return c;
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
void core_init_trace(Core *c)
{

  char cmdline[1024];
  sprintf(cmdline, "gunzip -c %s", c->trace_fname);
  infile = popen(cmdline, "r");
  if(!infile) {
   printf("Cannot open %s: \n", c->trace_fname);
   exit(1);
  }
  /*sprintf(command_string,"%s", c->trace_fname);
  if ((c->trace = gzopen(command_string, "r")) == NULL){
    printf("Command string is %s\n", command_string);
    die_message("Unable to open the file with gzip option \n");*/
  //}
}

////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void core_cycle (Core *c)
{
  if(c->done){
    return;
  }

  // if core is snoozing on DRAM hits, return ..
  if(cycle <= c->snooze_end_cycle){
      return;
  }

  c->inst_count++;

  uns ifetch_delay=0, ld_delay=0, st_delay=0, bubble_cycles=0;
	
  ifetch_delay = memsys_access(c->memsys, c->trace_inst_addr, c->trace_ldst_data, ACCESS_TYPE_IFETCH, c->core_id);
  if(ifetch_delay>1){
    bubble_cycles += (ifetch_delay-1);
  }

  if((uns64)(c->trace_inst_type) + 1==INST_TYPE_LOAD){
    ld_delay = memsys_access(c->memsys, c->trace_ldst_addr, c->trace_ldst_data, ACCESS_TYPE_LOAD, c->core_id);
  }
  if(ld_delay>1){
    bubble_cycles += (ld_delay-1);
  }
  
  if((uns64)(c->trace_inst_type) + 1==INST_TYPE_STORE){
    st_delay = memsys_access(c->memsys, c->trace_ldst_addr, c->trace_ldst_data, ACCESS_TYPE_STORE, c->core_id);
  }
  //No bubbles for store misses


  if(bubble_cycles){
    c->snooze_end_cycle = (cycle+bubble_cycles);
  }

  core_read_trace(c);
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////

void core_read_trace (Core *c){
    int iter;
	fread (&(rec.inst_num), 8, 1, infile);//inst_num
	fread (&(rec.va), 8, 1, infile);//va
	fread (&(rec.iswb), 1, 1, infile);//iswb
	fread (&(rec.delay), 4, 1, infile);//delay
	fread (&(rec.olddata), 64, 1, infile);//olddata
	fread (&(rec.newdata), 64, 1, infile);//newdata

    c->trace_ldst_addr = rec.va;
    c->trace_inst_type = rec.iswb;
    for(iter=0;iter<64;iter++)
       rec.newdata[iter]=abs(rec.newdata[iter]);
    memcpy(c->trace_ldst_data,rec.newdata,64);
    //printf("\n%d\t",c->trace_inst_type);
    //printf("%llu\t",c->trace_ldst_addr);
    //for(iter = 0; iter<64; iter++)
    //printf("%02x",c->trace_ldst_data[iter]);

    //flag++;
    //if(flag==20)
    	//exit(0);
	/*int iter;
  gzread(c->trace, &buf, sizeof(FTR_Record));
  ptr = &buf;
  memcpy(&(c->trace_ldst_addr),&(ptr->va),64);
  memcpy(&(c->trace_inst_type),&(ptr->iswb),sizeof(bool));
  //c->trace_inst_type = ptr->iswb;
  memcpy(c->trace_ldst_data,ptr->newdata,64);
  //printf("%llu\t",c->trace_inst_addr);
  printf("\n%d\t",c->trace_inst_type);
  printf("%llu\t",c->trace_ldst_addr);
  for(iter = 0; iter<64; iter++)
  printf("%02x",ptr->newdata[iter]);
  flag ++;
  if(flag == 50)
  exit(0);
  */
  //printf("\n");
  if(feof(infile)){
    c->done=TRUE;
    c->done_inst_count  = c->inst_count;
    c->done_cycle_count = cycle;
  }
  //ptr++;
}

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

void core_print_stats(Core *c)
{
  char header[256];
  double ipc = (double)(c->done_inst_count)/(double)(c->done_cycle_count);
  sprintf(header, "CORE_%01d", c->core_id);
  
  printf("\n");
  printf("\n%s_INST         \t\t : %10llu", header,  c->done_inst_count);
  printf("\n%s_CYCLES       \t\t : %10llu", header,  c->done_cycle_count);
  printf("\n%s_IPC          \t\t : %10.3f", header,  ipc);

  pclose(infile);
}


////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////
