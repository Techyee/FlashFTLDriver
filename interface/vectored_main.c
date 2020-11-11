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
//task_info task1, task2, task3, task4;
task_info* tinfo;

int main(int argc,char* argv[]){

	printf("[main.c line 34]checking params for TTC alloc, bpc: %d, _NOC %d\n",BPC, _NOC);
	//int temp_cnt=bench_set_params(argc,argv,temp_argv);
	printf("range: %lu!\n",RANGE);

	//allocate new threads for interface_main.
	pthread_t pth[7];
	int tid1, tid2, tid3, tid4, tid5, tid6, tid7, status;
	tinfo = (task_info*)malloc(sizeof(task_info)*7);
	
	tinfo[0].bench_idx = 0;
	tinfo[0].num_op = 12;
	tinfo[0].period = 129600;
	tinfo[0].chip_num = 1;
	tinfo[0].gc_threshold = _PPB/2*tinfo[0].chip_num/tinfo[0].num_op;
	tinfo[0].chip_idx = (int*)malloc(sizeof(int)*tinfo[0].chip_num);
	for(int i=0;i<tinfo[0].chip_num;i++)
		tinfo[0].chip_idx[i] = i;

	
	tinfo[1].bench_idx = 1;
	tinfo[1].num_op = 12;
	tinfo[1].period = 129600;
	tinfo[1].chip_num = 1;
	tinfo[1].gc_threshold = _PPB/2*tinfo[1].chip_num/tinfo[1].num_op;
	tinfo[1].chip_idx = (int*)malloc(sizeof(int)*tinfo[1].chip_num);
	for(int i=0;i<tinfo[1].chip_num;i++)
		tinfo[1].chip_idx[i] = i+1;

	tinfo[2].bench_idx = 2;
	tinfo[2].num_op = 24;
	tinfo[2].period = 28800;
	tinfo[2].chip_num = 1;
	tinfo[2].gc_threshold = _PPB/2*tinfo[2].chip_num/tinfo[2].num_op;
	tinfo[2].chip_idx = (int*)malloc(sizeof(int)*tinfo[2].chip_num);
	for(int i=0;i<tinfo[2].chip_num;i++)
		tinfo[2].chip_idx[i] = i+2;
	
	//read task.
	tinfo[3].bench_idx = 3;
	tinfo[3].num_op = 80;
	tinfo[3].period = 25000;
	tinfo[3].chip_num = 1;
	tinfo[3].gc_threshold =  _PPB/2*tinfo[3].chip_num/tinfo[3].num_op;
	tinfo[3].chip_idx = (int*)malloc(sizeof(int)*tinfo[3].chip_num);
	for(int i=0;i<tinfo[3].chip_num;i++)
		tinfo[3].chip_idx[i] = i;

	
	tinfo[4].bench_idx = 4;
	tinfo[4].num_op = 80;
	tinfo[4].period = 25000;
	tinfo[4].gc_threshold = 1;
	tinfo[4].chip_num = 1;
	tinfo[4].chip_idx = (int*)malloc(sizeof(int)*tinfo[4].chip_num);
	for(int i=0;i<tinfo[4].chip_num;i++)
		tinfo[4].chip_idx[i] = i;

	tinfo[5].bench_idx = 5;
	tinfo[5].num_op = 40;
	tinfo[5].period = 36000;
	tinfo[5].gc_threshold = 1;
	tinfo[5].chip_num = 1;
	tinfo[5].chip_idx = (int*)malloc(sizeof(int)*tinfo[5].chip_num);
	for(int i=0;i<tinfo[5].chip_num;i++)
		tinfo[5].chip_idx[i] = i;

	tinfo[6].bench_idx = 6;
	tinfo[6].num_op = 40;
	tinfo[6].period = 12000;
	tinfo[6].gc_threshold = 1;
	tinfo[6].chip_num = 1;
	tinfo[6].chip_idx = (int*)malloc(sizeof(int)*tinfo[6].chip_num);
	for(int i=0;i<tinfo[6].chip_num;i++)
		tinfo[6].chip_idx[i] = i;
	//read task end.
	inf_init(0,0,argc,argv);
	bench_init();
	bench_vectored_configure();
	bench_add(VECTOREDRSET,0         ,RANGE*1/16 - 1, RANGE/4);
	bench_add(VECTOREDRSET,RANGE*1/16,RANGE*2/16 - 1, RANGE/4);
	bench_add(VECTOREDRSET,RANGE*2/16,RANGE*3/16 - 1, RANGE/4 * 6);
	
	bench_add(VECTOREDRGET,0		 ,RANGE*1/16 - 1, RANGE/4 * 24);
	bench_add(VECTOREDRGET,RANGE*1/16,RANGE*2/16 - 1, RANGE/4 * 24);
	bench_add(VECTOREDRGET,RANGE*2/16,RANGE*3/16 - 1, RANGE/4 * 12);

	//bench_add(VECTOREDRGET,RANGE*2/16, RANGE*3/16 - 1, RANGE/4 * 12);
	tid1 = pthread_create(&pth[0],NULL,inf_main,(void*)&tinfo[0]);
	tid2 = pthread_create(&pth[1],NULL,inf_main,(void*)&tinfo[1]);
	tid3 = pthread_create(&pth[2],NULL,inf_main,(void*)&tinfo[2]);
	tid4 = pthread_create(&pth[3],NULL,inf_main,(void*)&tinfo[3]);
	tid5 = pthread_create(&pth[4],NULL,inf_main,(void*)&tinfo[4]);
	tid6 = pthread_create(&pth[5],NULL,inf_main,(void*)&tinfo[5]);
	//tid7 = pthread_create(&pth[6],NULL,inf_main,(void*)&tinfo[6]);
	pthread_join(pth[0],(void**)&status);
	pthread_join(pth[1],(void**)&status);
	pthread_join(pth[2],(void**)&status);
	pthread_join(pth[3],(void**)&status);
	pthread_join(pth[4],(void**)&status);
	pthread_join(pth[5],(void**)&status);
	//pthread_join(pth[6],(void**)&status);
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
