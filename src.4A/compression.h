#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

uint32_t upper_28f_int=0xFFFFFFF0;
uint32_t upper_280_int = 0x00000000;

char *upper_280=(char*)  (&upper_280_int);
char *upper_28f= (char*) (&upper_28f_int);



int FPC(char *data_segment){
	
	int best_size=32;

	uint32_t *data = (uint32_t*)(data_segment);

	//001 4-bit sign extended
	uint32_t masked_data29= *data & 0xFFFFFFF8;
	uint32_t masked_data25= *data & 0xFFFFFF80;
	uint32_t masked_data17= *data & 0xFFFF8000;
	uint32_t masked_data16= *data & 0xFFFF0000;
	uint32_t upper_word9 = *data & 0xFF800000;
	uint32_t lower_word9 = *data & 0x0000FF80;


	if( (masked_data29==0xFFFFFFF8) || (masked_data29==0 ) )
		best_size=4;

	else if( (masked_data25==0xFFFFFF80) || (masked_data25==0 ) )
		best_size=8;

	else if( (data_segment[0]==data_segment[1]) && (data_segment[2]==data_segment[3]) && (data_segment[1]==data_segment[2]))
		best_size=8;

	else if( (masked_data17==0xFFFF8000) || (masked_data17==0 ) )
		best_size=16;

	else if( masked_data16 == 0 )
		best_size=16;

	else if( ( (upper_word9 == 0xFF800000)|| (upper_word9 == 0) ) && ( (lower_word9 == 0xFF80)|| (lower_word9 == 0) ) )
		best_size=16;

	/*std::cout<<"DATA:\t";
	for(int i=0;i<4;i++)
		std::cout<<(int)data_segment[i]<<"\t";
	std::cout<<"Best Size"<<best_size<<"\n";*/

	return best_size;
}
int compress(char *data){
	float sum=0;
	for(int i=0;i<16;i++){
		sum+=FPC(data+(i*4));
	}
	sum=ceil(sum/8);
	double next = pow(2, ceil(log(sum)/log(2)));
	//printf("%d \n",(int)next);
	return (int)next;
}

