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

#define tbsdebug
//#define motive
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

//my structures. 
task_info* tinfo;
cluster_info* cluster_def;
cluster_status* cluster_stat;
struct timeval _g_tbs_start;
int _g_tbs_wnum = 0;
int rt_num = 3;
int cluster_num = 2;
int type;
int case_num;
int reclaim_page = _PPB/2;

#ifdef tbsdebug
int main(int argc,char* argv[]){
	//cluster info init.
	cluster_num = 3;
	cluster_def = (cluster_info*)malloc(sizeof(cluster_info)*cluster_num);
	cluster_stat = (cluster_status*)malloc(sizeof(cluster_status));
	//taskinfo
	tinfo = (task_info*)malloc(sizeof(task_info)*7);
	pthread_t pth[7];
	int tid1, tid2, tid3, tid4, tid5, tid6, tid7, status;

	//set cluseter information.
	//hardcode chipnum.
	cluster_def[0].chip_num = 1;
	cluster_def[1].chip_num = 2;
	cluster_def[2].chip_num = 1;
	cluster_def[0].cluster_util_left = 1.0 - 0.892901;
	cluster_def[1].cluster_util_left = 1.0 - 0.989536;
	cluster_def[2].cluster_util_left = 1.0 - 0.1;

	int cur_chip = 0;
	for(int i=0;i<cluster_num;i++){
		cluster_def[i].chip_idx = (int*)malloc(sizeof(int)*cluster_def[i].chip_num);
		for(int j=0;j<cluster_def[i].chip_num;j++){
			cluster_def[i].chip_idx[j] = cur_chip;
			cur_chip++;
		}
	}

	//set cluster stat monitor.
	cluster_stat->cluster_num = cluster_num;
	cluster_stat->tbs_deadlines = (uint32_t*)malloc(sizeof(uint32_t)*cluster_num);
	cluster_stat->cluster_cur_wnum = (int*)malloc(sizeof(int)*cluster_num);
	for(int i=0;i<cluster_num;i++){
		cluster_stat->tbs_deadlines[i] = 0;
		cluster_stat->cluster_cur_wnum[i] = 0;
	}
	//!cluster init.
	


	tinfo[0].bench_idx = 0;
	tinfo[0].type = RT;
	tinfo[0].num_op = 12;
	tinfo[0].period = 64800;
	tinfo[0].chip_num = 1;
	tinfo[0].gc_threshold = reclaim_page*tinfo[0].chip_num/tinfo[0].num_op;
	tinfo[0].chip_idx = (int*)malloc(sizeof(int)*tinfo[0].chip_num);
	for(int i=0;i<tinfo[0].chip_num;i++)
		tinfo[0].chip_idx[i] = i;

	tinfo[1].bench_idx = 1;
	tinfo[1].type = RT;
	tinfo[1].num_op = 12;
	tinfo[1].period = 64800;
	tinfo[1].chip_num = 1;
	tinfo[1].gc_threshold = reclaim_page*tinfo[1].chip_num/tinfo[1].num_op;
	tinfo[1].chip_idx = (int*)malloc(sizeof(int)*tinfo[1].chip_num);
	for(int i=0;i<tinfo[1].chip_num;i++)
		tinfo[1].chip_idx[i] = i;

	tinfo[2].bench_idx = 2;
	tinfo[2].type = RT;
	tinfo[2].num_op = 12;
	tinfo[2].period = 64800;
	tinfo[2].chip_num = 1;
	tinfo[2].gc_threshold = reclaim_page*tinfo[2].chip_num/tinfo[2].num_op;
	tinfo[2].chip_idx = (int*)malloc(sizeof(int)*tinfo[2].chip_num);
	for(int i=0;i<tinfo[2].chip_num;i++)
		tinfo[2].chip_idx[i] = i;

	tinfo[3].bench_idx = 3;
	tinfo[3].type = RT;
	tinfo[3].num_op = 12;
	tinfo[3].period = 16200;
	tinfo[3].chip_num = 2;
	tinfo[3].gc_threshold = reclaim_page*tinfo[3].chip_num/tinfo[3].num_op;
	tinfo[3].chip_idx = (int*)malloc(sizeof(int)*tinfo[3].chip_num);
	for(int i=0;i<tinfo[3].chip_num;i++)
		tinfo[3].chip_idx[i] = i+1;
	
	//BG(sched BG for each chip)
	//tinfo[4].bench_idx = 0;
	//tinfo[4].type = TBS;
	//tinfo[4].num_op = 128;
	//tinfo[4].period = 125000;
	//tinfo[4].gc_threshold = reclaim_page*tinfo[4].chip_num/tinfo[4].num_op;
	//tinfo[4].chip_num = 1;
	//tinfo[4].chip_idx = (int*)malloc(sizeof(int)*tinfo[4].chip_num);
	//for(int i=0;i<tinfo[4].chip_num;i++)
	//	tinfo[4].chip_idx[i] = i;
	//!TBS end

	//TBS
	tinfo[4].bench_idx = 4;
	tinfo[4].type = TBS;
	tinfo[4].num_op = 128;
	tinfo[4].period = 62500;
	tinfo[4].gc_threshold = reclaim_page*tinfo[4].chip_num/tinfo[4].num_op;
	tinfo[4].chip_num = 4;
	tinfo[4].chip_idx = (int*)malloc(sizeof(int)*tinfo[4].chip_num);
	for(int i=0;i<tinfo[4].chip_num;i++)
		tinfo[4].chip_idx[i] = i;

	inf_init(0,0,argc,argv);
	bench_init();	
	bench_vectored_configure();
	bench_add(VECTOREDRSET,0         ,RANGE*1/16-1, RANGE/4);
	bench_add(VECTOREDRSET,0		 ,RANGE*1/16-1, RANGE/4);
	bench_add(VECTOREDRSET,0		 ,RANGE*1/16-1, RANGE/4);
	bench_add(VECTOREDRSET,RANGE*1/16,RANGE*3/16-1, RANGE/4*4);
	bench_add(VECTOREDRSET,RANGE*3/16,RANGE*4/16-1, RANGE/4*4);
	tid1 = pthread_create(&pth[0],NULL,inf_main,(void*)&tinfo[0]);
	tid2 = pthread_create(&pth[1],NULL,inf_main,(void*)&tinfo[1]);
	tid3 = pthread_create(&pth[2],NULL,inf_main,(void*)&tinfo[2]);
	tid4 = pthread_create(&pth[3],NULL,inf_main,(void*)&tinfo[3]);
	tid5 = pthread_create(&pth[4],NULL,inf_main,(void*)&tinfo[4]);
	pthread_join(pth[0],(void**)&status);
	pthread_join(pth[1],(void**)&status);
	pthread_join(pth[2],(void**)&status);
	pthread_join(pth[3],(void**)&status);
	pthread_join(pth[4],(void**)&status);
	inf_free();
	
	return 0;
}
#endif

#ifdef motive
int main(int argc,char* argv[]){

	printf("checking params for TTC alloc, bpc: %d, _NOC %d\n",BPC, _NOC);
	//int temp_cnt=bench_set_params(argc,argv,temp_argv);
	printf("range: %lu!\n",RANGE);
	int period1 = atoi(argv[1]);
	int period2 = atoi(argv[2]);
	int period3 = atoi(argv[3]);
	type = atoi(argv[4]);
	case_num = atoi(argv[5]);
	int glob = 1;
	int good_a = 2;
	int good_b = 3;
	int isol = 4;
	printf("received period : %d, %d, %d\n",period1,period2,period3);
	//allocate new threads for interface_main.
	pthread_t pth[7];
	int tid1, tid2, tid3, tid4, tid5, tid6, tid7, status;

	//cluster info init.
	cluster_num = 2;
	cluster_def = (cluster_info*)malloc(sizeof(cluster_info)*cluster_num);
	cluster_stat = (cluster_status*)malloc(sizeof(cluster_status));
	tinfo = (task_info*)malloc(sizeof(task_info)*7);

	//set cluseter information.
	//hardcode chipnum.
	cluster_def[0].chip_num = 2;
	cluster_def[1].chip_num = 2;
	int cur_chip = 0;
	for(int i=0;i<cluster_num;i++){
		cluster_def[i].cluster_util_left = 0.1;
		cluster_def[i].chip_idx = (int*)malloc(sizeof(int)*cluster_def[i].chip_num);
		for(int j=0;j<cluster_def[i].chip_num;j++){
			cluster_def[i].chip_idx[j] = cur_chip;
			cur_chip++;
		}
	}
	//set cluster stat monitor.
	cluster_stat->cluster_num = cluster_num;
	cluster_stat->tbs_busy = (bool*)malloc(sizeof(bool)*cluster_num);
	cluster_stat->tbs_deadlines = (uint32_t*)malloc(sizeof(uint32_t)*cluster_num);
	cluster_stat->cluster_cur_wnum = (int*)malloc(sizeof(int)*cluster_num);
	for(int i=0;i<cluster_num;i++){
		cluster_stat[i].tbs_deadlines = 0;
		cluster_stat[i].cluster_cur_wnum = 0;
	}
	//!cluster init.
	//////////////////////////////////////////////////////////////////DUMMY START
	tinfo[0].bench_idx = 0;
	tinfo[0].type = RT;
	tinfo[0].num_op = 1;
	tinfo[0].period = 1000;
	tinfo[0].chip_num = 1;
	tinfo[0].gc_threshold = _PPB/3*tinfo[0].chip_num/tinfo[0].num_op;
	tinfo[0].chip_idx = (int*)malloc(sizeof(int)*tinfo[0].chip_num);
	for(int i=0;i<tinfo[0].chip_num;i++)
		tinfo[0].chip_idx[i] = i;

	tinfo[1].bench_idx = 1;
	tinfo[1].type = RT;
	tinfo[1].num_op = 1;
	tinfo[1].period = 1000;
	tinfo[1].chip_num = 1;
	tinfo[1].gc_threshold = _PPB/3*tinfo[1].chip_num/tinfo[1].num_op;
	tinfo[1].chip_idx = (int*)malloc(sizeof(int)*tinfo[1].chip_num);
	for(int i=0;i<tinfo[1].chip_num;i++)
		tinfo[1].chip_idx[i] = i+1;

	tinfo[2].bench_idx = 2;
	tinfo[2].type = RT;
	tinfo[2].num_op = 1;
	tinfo[2].period = 1000;
	tinfo[2].chip_num = 1;
	tinfo[2].gc_threshold = _PPB/3*tinfo[2].chip_num/tinfo[2].num_op;
	tinfo[2].chip_idx = (int*)malloc(sizeof(int)*tinfo[2].chip_num);
	for(int i=0;i<tinfo[2].chip_num;i++)
		tinfo[2].chip_idx[i] = i+2;

	/////////////////////////////////////////////////////////////////DUMMY END
	
	tinfo[3].bench_idx = 3;
	tinfo[3].type = RT;
	tinfo[3].num_op = 1;
	tinfo[3].period = period1;
	
	tinfo[4].bench_idx = 4;
	tinfo[4].type = RT;
	tinfo[4].num_op = 1;
	tinfo[4].period = period2;

	tinfo[5].bench_idx = 5;
	tinfo[5].type = RT;
	tinfo[5].num_op = 4;
	tinfo[5].period = period3;
	

	if(type == isol){
		tinfo[3].chip_num = 1;
		tinfo[3].gc_threshold =  reclaim_page*tinfo[3].chip_num/tinfo[3].num_op;
		tinfo[3].chip_idx = (int*)malloc(sizeof(int)*tinfo[3].chip_num);
		for(int i=0;i<tinfo[3].chip_num;i++)
			tinfo[3].chip_idx[i] = i;
		tinfo[4].chip_num = 1;
		tinfo[4].gc_threshold =  reclaim_page*tinfo[4].chip_num/tinfo[4].num_op;
		tinfo[4].chip_idx = (int*)malloc(sizeof(int)*tinfo[4].chip_num);
		for(int i=0;i<tinfo[4].chip_num;i++)
			tinfo[4].chip_idx[i] = i+1;
		tinfo[5].chip_num = 1;
		tinfo[5].gc_threshold =  reclaim_page*tinfo[5].chip_num/tinfo[5].num_op;
		tinfo[5].chip_idx = (int*)malloc(sizeof(int)*tinfo[5].chip_num);
		for(int i=0;i<tinfo[5].chip_num;i++)
			tinfo[5].chip_idx[i] = i+2;
	}

	if(type == good_a){
		tinfo[3].chip_num = 2;
		tinfo[3].gc_threshold =  reclaim_page*tinfo[3].chip_num/tinfo[3].num_op;
		tinfo[3].chip_idx = (int*)malloc(sizeof(int)*tinfo[3].chip_num);
		for(int i=0;i<tinfo[3].chip_num;i++)
			tinfo[3].chip_idx[i] = i;
		tinfo[4].chip_num = 2;
		tinfo[4].gc_threshold =  reclaim_page*tinfo[4].chip_num/tinfo[4].num_op;
		tinfo[4].chip_idx = (int*)malloc(sizeof(int)*tinfo[4].chip_num);
		for(int i=0;i<tinfo[4].chip_num;i++)
			tinfo[4].chip_idx[i] = i;
		tinfo[5].chip_num = 1;
		tinfo[5].gc_threshold =  reclaim_page*tinfo[5].chip_num/tinfo[5].num_op;
		tinfo[5].chip_idx = (int*)malloc(sizeof(int)*tinfo[5].chip_num);
		for(int i=0;i<tinfo[5].chip_num;i++)
			tinfo[5].chip_idx[i] = i+2;
	}

	if(type == good_b){
		tinfo[3].chip_num = 1;
		tinfo[3].gc_threshold =  reclaim_page*tinfo[3].chip_num/tinfo[3].num_op;
		tinfo[3].chip_idx = (int*)malloc(sizeof(int)*tinfo[3].chip_num);
		for(int i=0;i<tinfo[3].chip_num;i++)
			tinfo[3].chip_idx[i] = i;
		tinfo[4].chip_num = 1;
		tinfo[4].gc_threshold =  reclaim_page*tinfo[4].chip_num/tinfo[4].num_op;
		tinfo[4].chip_idx = (int*)malloc(sizeof(int)*tinfo[4].chip_num);
		for(int i=0;i<tinfo[4].chip_num;i++)
			tinfo[4].chip_idx[i] = i;
		tinfo[5].chip_num = 2;
		tinfo[5].gc_threshold =  reclaim_page*tinfo[5].chip_num/tinfo[5].num_op;
		tinfo[5].chip_idx = (int*)malloc(sizeof(int)*tinfo[5].chip_num);
		for(int i=0;i<tinfo[5].chip_num;i++)
			tinfo[5].chip_idx[i] = i+1;
	}

	if(type == glob){
		tinfo[3].chip_num = 3;
		tinfo[3].gc_threshold =  reclaim_page*tinfo[3].chip_num/tinfo[3].num_op;
		tinfo[3].chip_idx = (int*)malloc(sizeof(int)*tinfo[3].chip_num);
		for(int i=0;i<tinfo[3].chip_num;i++)
			tinfo[3].chip_idx[i] = i;
		tinfo[4].chip_num = 3;
		tinfo[4].gc_threshold =  reclaim_page*tinfo[4].chip_num/tinfo[4].num_op;
		tinfo[4].chip_idx = (int*)malloc(sizeof(int)*tinfo[4].chip_num);
		for(int i=0;i<tinfo[4].chip_num;i++)
			tinfo[4].chip_idx[i] = i;
		tinfo[5].chip_num = 3;
		tinfo[5].gc_threshold =  reclaim_page*tinfo[5].chip_num/tinfo[5].num_op;
		tinfo[5].chip_idx = (int*)malloc(sizeof(int)*tinfo[5].chip_num);
		for(int i=0;i<tinfo[5].chip_num;i++)
			tinfo[5].chip_idx[i] = i;
	}

	//TBS
	tinfo[6].bench_idx = 6;
	tinfo[6].type = TBS;
	tinfo[6].num_op = 128;
	tinfo[6].period = 1000;
	tinfo[6].gc_threshold = 1;
	tinfo[6].chip_num = 1;
	tinfo[6].chip_idx = (int*)malloc(sizeof(int)*tinfo[6].chip_num);
	for(int i=0;i<tinfo[6].chip_num;i++)
		tinfo[6].chip_idx[i] = i;
	//task end.

	inf_init(0,0,argc,argv);
	bench_init();
	bench_vectored_configure();
	//dummy write workload.
	bench_add(VECTOREDSSET,0         ,RANGE*1/16-1, RANGE*1/16/4*3);
	bench_add(VECTOREDSSET,RANGE*1/16,RANGE*2/16-1, RANGE*1/16/4*3);
	bench_add(VECTOREDSSET,RANGE*2/16,RANGE*3/16-1, RANGE*1/16/4*3);
	
	//test workload.
	if(type == good_a){
		bench_add(VECTOREDRSET,0		 ,RANGE*2/16-1, RANGE/8);
		bench_add(VECTOREDRSET,0		 ,RANGE*2/16-1, RANGE/8);
		bench_add(VECTOREDRSET,RANGE*2/16,RANGE*3/16-1, RANGE/8);
	}
	if(type == good_b){
		bench_add(VECTOREDRSET,0		 ,RANGE*1/16-1, RANGE/8);
		bench_add(VECTOREDRSET,0		 ,RANGE*1/16-1, RANGE/8);
		bench_add(VECTOREDRSET,RANGE*1/16,RANGE*3/16-1, RANGE/4);
	}
	if(type == isol){
		bench_add(VECTOREDRSET,0		 ,RANGE*1/16-1, RANGE/8);
		bench_add(VECTOREDRSET,RANGE*1/16,RANGE*2/16-1, RANGE/8);
		bench_add(VECTOREDRSET,RANGE*2/16,RANGE*3/16-1, RANGE/8);
	}
	if(type == glob){
		bench_add(VECTOREDRSET,0		 ,RANGE*3/16-1, RANGE/8);
		bench_add(VECTOREDRSET,0		 ,RANGE*3/16-1, RANGE/8);
		bench_add(VECTOREDRSET,0		 ,RANGE*3/16-1, RANGE/4);
	}

	//bench_add(VECTOREDRGET,0		 ,RANGE*1/16 - 1, RANGE/4 * 24);
	//bench_add(VECTOREDRGET,RANGE*1/16,RANGE*2/16 - 1, RANGE/4 * 24);
	//bench_add(VECTOREDRGET,RANGE*2/16,RANGE*3/16 - 1, RANGE/4 * 12);
	//bench_add(VECTOREDRGET,RANGE*2/16, RANGE*3/16 - 1, RANGE/4 * 12);

	//dummy.
	tid1 = pthread_create(&pth[0],NULL,inf_main,(void*)&tinfo[0]);
	tid2 = pthread_create(&pth[1],NULL,inf_main,(void*)&tinfo[1]);
	tid3 = pthread_create(&pth[2],NULL,inf_main,(void*)&tinfo[2]);
	pthread_join(pth[0],(void**)&status);
	pthread_join(pth[1],(void**)&status);
	pthread_join(pth[2],(void**)&status);
	
	tid4 = pthread_create(&pth[3],NULL,inf_main,(void*)&tinfo[3]);
	tid5 = pthread_create(&pth[4],NULL,inf_main,(void*)&tinfo[4]);
	tid6 = pthread_create(&pth[5],NULL,inf_main,(void*)&tinfo[5]);
	//tid7 = pthread_create(&pth[6],NULL,inf_main,(void*)&tinfo[6]);
	
	
	
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
#endif
