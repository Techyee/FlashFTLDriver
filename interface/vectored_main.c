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
	bench_add(VECTOREDRSET,RANGE*1/16, RANGE*2/16 - 1, RANGE/4);
	bench_add(VECTOREDRSET,RANGE*2/16, RANGE*3/16 - 1, RANGE/4);
	bench_add(VECTOREDRSET,RANGE*3/16, RANGE*4/16 - 1, RANGE/4 * 5);
	printf("range: %lu!\n",RANGE);
	//bench_add(VECTOREDRW,0,RANGE,RANGE*2);

	//allocate new thread for interface_main.
	pthread_t pth[4];
	int tid1, tid2, tid3, tid4, status;
	task_info task1, task2, task3, task4;
	
	task1.bench_idx = 0;
	task1.num_op = 12;
	task1.period = 60000;
	task1.chip_num = 1;
	task1.chip_idx = (int*)malloc(sizeof(int)*task1.chip_num);
	for(int i=0;i<task1.chip_num;i++)
		task1.chip_idx[i] = 0;

	task2.bench_idx = 1;
	task2.num_op = 12;
	task2.period = 60000;
	task2.chip_num = 1;
	task2.chip_idx = (int*)malloc(sizeof(int)*task2.chip_num);
	for(int i=0;i<task2.chip_num;i++)
		task2.chip_idx[i] = 1;

	task3.bench_idx = 2;
	task3.num_op = 12;
	task3.period = 60000;
	task3.chip_num = 1;
	task3.chip_idx = (int*)malloc(sizeof(int)*task3.chip_num);
	for(int i=0;i<task3.chip_num;i++)
		task3.chip_idx[i] = 2;

	task4.bench_idx = 3;
	task4.num_op = 12;
	task4.period = 12000;
	task4.chip_num = 1;
	task4.chip_idx = (int*)malloc(sizeof(int)*task4.chip_num);
	for(int i=0;i<task4.chip_num;i++)
		task4.chip_idx[i] = 3;

	tid1 = pthread_create(&pth[0],NULL,inf_main,(void*)&task1);
	tid2 = pthread_create(&pth[1],NULL,inf_main,(void*)&task2);
	tid3 = pthread_create(&pth[2],NULL,inf_main,(void*)&task3);
	tid4 = pthread_create(&pth[3],NULL,inf_main,(void*)&task4);
	pthread_join(pth[0],(void**)&status);
	pthread_join(pth[1],(void**)&status);
	pthread_join(pth[2],(void**)&status);
	pthread_join(pth[3],(void**)&status);

	printf("thread is joined!!\n");
	/*
	
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
