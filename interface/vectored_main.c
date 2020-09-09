#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <getopt.h>
#include "../include/lsm_settings.h"
#include "../include/FS.h"
#include "../include/settings.h"
#include "../include/types.h"
#include "../bench/bench.h"
#include "interface.h"
#include "vectored_interface.h"
#include "../include/utils/kvssd.h"

extern int req_cnt_test;
extern uint64_t dm_intr_cnt;
extern int LOCALITY;
extern float TARGETRATIO;
extern int KEYLENGTH;
extern int VALUESIZE;
extern uint32_t INPUTREQNUM;
extern master *_master;
extern bool force_write_start;
extern int seq_padding_opt;
MeasureTime write_opt_time[11];
extern master_processor mp;
extern uint64_t cumulative_type_cnt[LREQ_TYPE_NUM];
int main(int argc,char* argv[]){
	//int temp_cnt=bench_set_params(argc,argv,temp_argv);
	inf_init(0,0,argc,argv);
	bench_init();
	bench_vectored_configure();
//	bench_add(VECTOREDRSET,0,RANGE,RANGE);
	bench_add(VECTOREDRSET,0,RANGE,RANGE/8);
	//bench_add(VECTOREDRGET,0,RANGE/100*99,RANGE/100*99);
	printf("range: %lu!\n",RANGE);
	//bench_add(VECTOREDRW,0,RANGE,RANGE*2);

	//allocate new thread for interface_main.
	pthread_t pth;
	int thread_id, status;
	thread_id = pthread_create(&pth,NULL,inf_main,NULL);
	pthread_join(pth,(void**)&status);
	printf("thread is joined!!\n");
	/*
	char *value;
	uint32_t mark;
	int exec_time_usec;
	int print_limit = 10;
	int bench_init_stage = 1;
	int op_num = 10;
	int cur_num = 0;
	int period_usec = 5000; //hard-coded period.
	int elapsed_usec = 0;
	//real-time version make request building.
	struct timeval rt_start;
	struct timeval rt_end;
	
	while(1){
		//inf_make_req must be done in periodic manner.
		
		//interface body.	
		gettimeofday(&rt_start,NULL);
		value=get_vectored_bench_pinned(&mark, 0);
		if(bench_init_stage == 1){//time for preparing bench data should not be counted.
			gettimeofday(&rt_start,NULL);
			bench_init_stage = 0;
		}
		if (value == NULL){
			break;
		}
		inf_vector_make_req(value, bench_transaction_end_req, mark);
		gettimeofday(&rt_end,NULL);
		//!interface body
		
		elapsed_usec += (rt_end.tv_sec - rt_start.tv_sec)*1000000 + (rt_end.tv_usec - rt_start.tv_usec);
		cur_num++;
		if(cur_num == op_num){
			usleep(period_usec - elapsed_usec);
			cur_num = 0;
			elapsed_usec = 0;
		}
	}

	force_write_start=true;
	
	printf("bench finish\n");
	while(!bench_is_finish()){
	
#ifdef LEAKCHECK
		sleep(1);
#endif
	}
	*/
	inf_free();
	//bench_custom_print(write_opt_time,11);
	return 0;
}
