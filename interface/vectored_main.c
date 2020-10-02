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

	printf("[main.c line 34]checking params for TTC alloc, bpc: %d, _NOC %d\n",BPC, _NOC);
	//int temp_cnt=bench_set_params(argc,argv,temp_argv);
	inf_init(0,0,argc,argv);
	bench_init();
	bench_vectored_configure();
//	bench_add(VECTOREDRSET,0,RANGE,RANGE);
	bench_add(VECTOREDRSET,0         , RANGE*1/16 - 1, RANGE/4);
	bench_add(VECTOREDRSET,RANGE*1/16, RANGE*2/16, RANGE/4);
	//bench_add(VECTOREDRSET,RANGE*3/16, RANGE*4/16 - 1, RANGE/4);
	//bench_add(VECTOREDRGET,0,RANGE/100*99,RANGE/100*99);
	printf("range: %lu!\n",RANGE);
	//bench_add(VECTOREDRW,0,RANGE,RANGE*2);

	//allocate new thread for interface_main.
	pthread_t pth[3];
	int tid1, tid2, tid3, status;
	task_info task1, task2, task3;
	
	task1.bench_idx = 0;
	task1.num_op = 3;
	task1.period = 5000;

	task2.bench_idx = 1;
	task2.num_op = 1;
	task2.period = 1000;

	task3.bench_idx = 2;
	task3.num_op = 3;
	task3.period = 1500;
	
	tid1 = pthread_create(&pth[0],NULL,inf_main,(void*)&task1);
	tid2 = pthread_create(&pth[1],NULL,inf_main,(void*)&task2);
	//tid3 = pthread_create(&pth[2],NULL,inf_main,(void*)&task3);
	pthread_join(pth[0],(void**)&status);
	pthread_join(pth[1],(void**)&status);
	//pthread_join(pth[2],(void**)&status);
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
