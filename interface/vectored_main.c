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

int GLOB_BG = 1;
int ISOL_BG = 0;
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
	cluster_num = 2;
	cluster_def = (cluster_info*)malloc(sizeof(cluster_info)*cluster_num);
	cluster_stat = (cluster_status*)malloc(sizeof(cluster_status));
	//taskinfo
	tinfo = (task_info*)malloc(sizeof(task_info)*10);
	pthread_t pth[10];
	int tid1, tid2, tid3, tid4, tid5, tid6, tid7, tid8, tid9, tid10, status;

	//set cluseter information.
	//hardcode chipnum.
	cluster_def[0].chip_num = 3;
	cluster_def[1].chip_num = 1;
	//cluster_def[2].chip_num = 1;
	cluster_def[0].cluster_util_left = 1.0 - 0.955052;
	cluster_def[1].cluster_util_left = 1.0 - 0.902315;
	//cluster_def[2].cluster_util_left = 1.0 - 0.9;

	//cluster malloc and def)
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
	//DUMMY
	tinfo[0].bench_idx = 0;
	tinfo[0].type = RT;
	tinfo[0].num_op = 1;
	tinfo[0].period = 500;
	tinfo[0].chip_num = 4;
	tinfo[0].gc_threshold = reclaim_page*tinfo[0].chip_num/tinfo[0].num_op;
	tinfo[0].chip_idx = (int*)malloc(sizeof(int)*tinfo[0].chip_num);
	for(int i=0;i<tinfo[0].chip_num;i++)
		tinfo[0].chip_idx[i] = i;
	//!DUMMY

	tinfo[1].bench_idx = 1;
	tinfo[1].type = RT;
	tinfo[1].num_op = 24;
	tinfo[1].period = 37028;
	tinfo[1].chip_num = 3;
	tinfo[1].gc_threshold = reclaim_page*tinfo[1].chip_num/tinfo[1].num_op;
	tinfo[1].chip_idx = (int*)malloc(sizeof(int)*tinfo[1].chip_num);
	for(int i=0;i<tinfo[1].chip_num;i++)
		tinfo[1].chip_idx[i] = i;

	tinfo[2].bench_idx = 2;
	tinfo[2].type = RT;
	tinfo[2].num_op = 12;
	tinfo[2].period = 129600;
	tinfo[2].chip_num = 1;
	tinfo[2].gc_threshold = reclaim_page*tinfo[2].chip_num/tinfo[2].num_op;
	tinfo[2].chip_idx = (int*)malloc(sizeof(int)*tinfo[2].chip_num);
	for(int i=0;i<tinfo[2].chip_num;i++)
		tinfo[2].chip_idx[i] = i+3;

	tinfo[3].bench_idx = 3;
	tinfo[3].type = RT;
	tinfo[3].num_op = 12;
	tinfo[3].period = 129600;
	tinfo[3].chip_num = 1;
	tinfo[3].gc_threshold = reclaim_page*tinfo[3].chip_num/tinfo[3].num_op;
	tinfo[3].chip_idx = (int*)malloc(sizeof(int)*tinfo[3].chip_num);
	for(int i=0;i<tinfo[3].chip_num;i++)
		tinfo[3].chip_idx[i] = i+3;
	
	
	tinfo[4].bench_idx = 4;
	tinfo[4].type = RT;
	tinfo[4].num_op = 12;
	tinfo[4].period = 129600;
	tinfo[4].chip_num = 1;
	tinfo[4].gc_threshold = reclaim_page*tinfo[4].chip_num/tinfo[4].num_op;
	tinfo[4].chip_idx = (int*)malloc(sizeof(int)*tinfo[4].chip_num);
	for(int i=0;i<tinfo[4].chip_num;i++)
		tinfo[4].chip_idx[i] = i+3;

	//best-effort write(sched BG for each chip)
	tinfo[5].bench_idx = 5;
	tinfo[5].type = TBS;
	tinfo[5].num_op = 128;
	tinfo[5].period = 125000;
	tinfo[5].chip_num = 4;
	tinfo[5].gc_threshold = reclaim_page*tinfo[5].chip_num/tinfo[5].num_op;
	tinfo[5].chip_idx = (int*)malloc(sizeof(int)*tinfo[5].chip_num);
	for(int i=0;i<tinfo[5].chip_num;i++)
		tinfo[5].chip_idx[i] = i;

	//RT reads
	tinfo[6].bench_idx = 6;
	tinfo[6].type = RT;
	tinfo[6].num_op = 40;
	tinfo[6].period = 18000;
	tinfo[6].chip_num = 3;
	tinfo[6].gc_threshold = reclaim_page*tinfo[6].chip_num/tinfo[6].num_op;
	tinfo[6].chip_idx = (int*)malloc(sizeof(int)*tinfo[6].chip_num);
	for(int i=0;i<tinfo[6].chip_num;i++)
		tinfo[6].chip_idx[i] = i;
	
	tinfo[7].bench_idx = 7;
	tinfo[7].type = RT;
	tinfo[7].num_op = 80;
	tinfo[7].period = 72000;
	tinfo[7].chip_num = 1;
	tinfo[7].gc_threshold = reclaim_page*tinfo[7].chip_num/tinfo[7].num_op;
	tinfo[7].chip_idx = (int*)malloc(sizeof(int)*tinfo[7].chip_num);
	for(int i=0;i<tinfo[7].chip_num;i++)
		tinfo[7].chip_idx[i] = i+3;
	
	tinfo[8].bench_idx = 8;
	tinfo[8].type = RT;
	tinfo[8].num_op = 80;
	tinfo[8].period = 72000;
	tinfo[8].chip_num = 1;
	tinfo[8].gc_threshold = reclaim_page*tinfo[8].chip_num/tinfo[8].num_op;
	tinfo[8].chip_idx = (int*)malloc(sizeof(int)*tinfo[8].chip_num);
	for(int i=0;i<tinfo[8].chip_num;i++)
		tinfo[8].chip_idx[i] = i+3;
	
	tinfo[9].bench_idx = 9;
	tinfo[9].type = RT;
	tinfo[9].num_op = 80;
	tinfo[9].period = 72000;
	tinfo[9].chip_num = 1;
	tinfo[9].gc_threshold = reclaim_page*tinfo[9].chip_num/tinfo[9].num_op;
	tinfo[9].chip_idx = (int*)malloc(sizeof(int)*tinfo[9].chip_num);
	for(int i=0;i<tinfo[9].chip_num;i++)
		tinfo[9].chip_idx[i] = i+3;
	//!RT read end
	/*
	//TBS
	tinfo[4].bench_idx = 4;
	tinfo[4].type = TBS;
	tinfo[4].num_op = 128;
	tinfo[4].period = 125000;
	tinfo[4].gc_threshold = reclaim_page*tinfo[4].chip_num/tinfo[4].num_op;
	tinfo[4].chip_num = 4;
	tinfo[4].chip_idx = (int*)malloc(sizeof(int)*tinfo[4].chip_num);
	for(int i=0;i<tinfo[4].chip_num;i++)
		tinfo[4].chip_idx[i] = i;
	*/
	inf_init(0,0,argc,argv);
	bench_init();	
	bench_vectored_configure();
	//DUMMYRAND
	bench_add(VECTOREDRSET,0		 ,RANGE*4/16-1, RANGE/4);
	//RTwrite
	bench_add(VECTOREDRSET,0		 ,RANGE*3/16-1, RANGE/2);
	bench_add(VECTOREDRSET,RANGE*3/16,RANGE*4/16-1, RANGE/16);
	bench_add(VECTOREDRSET,RANGE*3/16,RANGE*4/16-1, RANGE/16);
	bench_add(VECTOREDRSET,RANGE*3/16,RANGE*4/16-1, RANGE/16);
	//BGwrite
	bench_add(VECTOREDRSET,RANGE*3/16,RANGE*4/16-1, RANGE/16);
	//RTread
	bench_add(VECTOREDRGET,0		 ,RANGE*1/16-1, RANGE);
	bench_add(VECTOREDRGET,RANGE*3/16,RANGE*4/16-1, RANGE/8);
	bench_add(VECTOREDRGET,RANGE*3/16,RANGE*4/16-1, RANGE/8);
	bench_add(VECTOREDRGET,RANGE*3/16,RANGE*4/16-1, RANGE/8);

	//BGread
	/////

	//DUMMY first
	tid1 = pthread_create(&pth[0],NULL,inf_main,(void*)&tinfo[0]);
	pthread_join(pth[0],(void**)&status);

	tid2 = pthread_create(&pth[1],NULL,inf_main,(void*)&tinfo[1]);
	tid3 = pthread_create(&pth[2],NULL,inf_main,(void*)&tinfo[2]);
	tid4 = pthread_create(&pth[3],NULL,inf_main,(void*)&tinfo[3]);
	tid5 = pthread_create(&pth[4],NULL,inf_main,(void*)&tinfo[4]);
	tid6 = pthread_create(&pth[5],NULL,inf_main,(void*)&tinfo[5]);
	tid7 = pthread_create(&pth[6],NULL,inf_main,(void*)&tinfo[6]);
	tid8 = pthread_create(&pth[7],NULL,inf_main,(void*)&tinfo[7]);
	tid9 = pthread_create(&pth[8],NULL,inf_main,(void*)&tinfo[8]);
	tid10 = pthread_create(&pth[9],NULL,inf_main,(void*)&tinfo[9]);
	pthread_join(pth[1],(void**)&status);
	pthread_join(pth[2],(void**)&status);
	pthread_join(pth[3],(void**)&status);
	pthread_join(pth[4],(void**)&status);
	pthread_join(pth[5],(void**)&status);
	pthread_join(pth[6],(void**)&status);
	pthread_join(pth[7],(void**)&status);
	pthread_join(pth[8],(void**)&status);
	pthread_join(pth[9],(void**)&status);
	inf_free();
	
	return 0;
}
#endif

#ifdef motive
int main(int argc,char* argv[]){

	printf("checking params for TTC alloc, bpc: %d, _NOC %d\n",BPC, _NOC);
	//int temp_cnt=bench_set_params(argc,argv,temp_argv);
	printf("range: %lu!\n",RANGE);
	type = atoi(argv[1]);
	case_num = atoi(argv[2]);
	int glob = 1;
	int good_a = 2;
	int good_b = 3;
	int isol = 4;
	
	//taskinfo
	pthread_t pth[7];
	int tids[7];
	int tid1, tid2, tid3, tid4, tid5, tid6, tid7, tid8, status;
	tinfo = (task_info*)malloc(sizeof(task_info)*7);

	//cluster info init.
	cluster_num = 3;
	cluster_def = (cluster_info*)malloc(sizeof(cluster_info)*cluster_num);
	cluster_stat = (cluster_status*)malloc(sizeof(cluster_status));
	
	

	//set cluseter information.
	//hardcode chipnum.
	cluster_def[0].chip_num = 1;
	cluster_def[1].chip_num = 2;
	cluster_def[2].chip_num = 1;
	cluster_def[0].cluster_util_left = 1.0 - 0.95;
	cluster_def[1].cluster_util_left = 1.0 - 0.92;
	cluster_def[2].cluster_util_left = 1.0 - 0.0;

	//cluster mallic(def  at vectored_interface.c)
	int cur_chip = 0;
	for(int i=0;i<cluster_num;i++){
		cluster_def[i].chip_idx = (int*)malloc(sizeof(int)*cluster_def[i].chip_num);
	}
	//////////////////////////////////////////////////////////////////DUMMY START
	tinfo[0].bench_idx = 0;
	tinfo[0].type = DUMMY;
	tinfo[0].num_op = 1;
	tinfo[0].period = 500;
	tinfo[0].chip_num = 3;
	tinfo[0].gc_threshold = reclaim_page*tinfo[0].chip_num/tinfo[0].num_op;
	tinfo[0].chip_idx = (int*)malloc(sizeof(int)*tinfo[0].chip_num);
	for(int i=0;i<tinfo[0].chip_num;i++)
		tinfo[0].chip_idx[i] = i;

	/////////////////////////////////////////////////////////////////DUMMY END
	int s_tid = 1;
	int e_tid = 6;
	for(int i=s_tid;i<e_tid+1;i++){
		tinfo[i].bench_idx = i;
		tinfo[i].type = RT;
	}

	//HARDCODED task infos.
	tinfo[1].num_op = 24;
	tinfo[1].period = 30000;

	tinfo[2].num_op = 12;
	tinfo[2].period = 130000;
	
	tinfo[3].num_op = 12;
	tinfo[3].period = 130000;
	
	tinfo[4].num_op = 40;
	tinfo[4].period = 36000;

	tinfo[5].num_op = 80;
	tinfo[5].period = 25000;

	tinfo[6].num_op = 80;
	tinfo[6].period = 25000;

	if(type == isol){
		for(int i=s_tid;i<e_tid+1;i++){
			tinfo[i].chip_num = 1;
			tinfo[i].gc_threshold = reclaim_page*tinfo[i].chip_num/tinfo[i].num_op;
			tinfo[i].chip_idx = (int*)malloc(sizeof(int)*tinfo[i].chip_num);
			for(int j=0;j<tinfo[i].chip_num;j++)
				tinfo[i].chip_idx[j] = (i-1) % 3;
		}
	}
	if(type == good_a){
		for(int i=s_tid;i<e_tid+1;i++){
			if((i-1)%3 == 0){
				tinfo[i].chip_num = 2;
				tinfo[i].gc_threshold = reclaim_page*tinfo[i].chip_num/tinfo[i].num_op;
				tinfo[i].chip_idx = (int*)malloc(sizeof(int)*tinfo[i].chip_num);
				for(int j=0;j<tinfo[i].chip_num;j++)
					tinfo[i].chip_idx[j] = j;
			}
			else{
				tinfo[i].chip_num = 1;
				tinfo[i].gc_threshold = reclaim_page*tinfo[i].chip_num/tinfo[i].num_op;
				tinfo[i].chip_idx = (int*)malloc(sizeof(int)*tinfo[i].chip_num);
				tinfo[i].chip_idx[0] = 2;
			} 
		}
	}
	if(type == glob){
		for(int i=s_tid;i<e_tid+1;i++){
			tinfo[i].chip_num = 3;
			tinfo[i].gc_threshold = reclaim_page*tinfo[i].chip_num/tinfo[i].num_op;
			tinfo[i].chip_idx = (int*)malloc(sizeof(int)*tinfo[i].chip_num);
			for(int j=0;j<tinfo[i].chip_num;j++)
				tinfo[i].chip_idx[j] = j;
		}
	}

	inf_init(0,0,argc,argv);
	bench_init();
	bench_vectored_configure();
	//dummy write workload.
	bench_add(VECTOREDRSET,0         ,RANGE*3/16-1, RANGE*3/16);
	//test workload.
	if(type == good_a){
		bench_add(VECTOREDRSET,0		 ,RANGE*2/16-1, RANGE/3);
		bench_add(VECTOREDRSET,RANGE*2/16,RANGE*3/16-1, RANGE/12);
		bench_add(VECTOREDRSET,RANGE*2/16,RANGE*3/16-1, RANGE/12);
		bench_add(VECTOREDRGET,0		 ,RANGE*1/16-1, RANGE);
		bench_add(VECTOREDRGET,RANGE*2/16,RANGE*3/16-1, RANGE*3);
		bench_add(VECTOREDRGET,RANGE*2/16,RANGE*3/16-1, RANGE*3);
	}

	if(type == isol){
		bench_add(VECTOREDRSET,0		 ,RANGE*1/16-1, RANGE/2);
		bench_add(VECTOREDRSET,RANGE*1/16,RANGE*2/16-1, RANGE/2);
		bench_add(VECTOREDRSET,RANGE*2/16,RANGE*3/16-1, RANGE/2);
		bench_add(VECTOREDRGET,0		 ,RANGE*1/16-1, RANGE);
		bench_add(VECTOREDRGET,RANGE*1/16,RANGE*2/16-1, RANGE);
		bench_add(VECTOREDRGET,RANGE*2/16,RANGE*3/16-1, RANGE);
	}
	if(type == glob){
		bench_add(VECTOREDRSET,0		 ,RANGE*3/16-1, RANGE/3);
		bench_add(VECTOREDRSET,0		 ,RANGE*3/16-1, RANGE/12);
		bench_add(VECTOREDRSET,0		 ,RANGE*3/16-1, RANGE/12);
		bench_add(VECTOREDRGET,0		 ,RANGE*1/16-1, RANGE);
		bench_add(VECTOREDRGET,0		 ,RANGE*1/16-1, RANGE*3);
		bench_add(VECTOREDRGET,0		 ,RANGE*1/16-1, RANGE*3);
	}

	//dummy.
	tid1 = pthread_create(&pth[0],NULL,inf_main,(void*)&tinfo[0]);
	pthread_join(pth[0],(void**)&status);
	
	tid2 = pthread_create(&pth[1],NULL,inf_main,(void*)&tinfo[1]);
	tid3 = pthread_create(&pth[2],NULL,inf_main,(void*)&tinfo[2]);
	tid4 = pthread_create(&pth[3],NULL,inf_main,(void*)&tinfo[3]);
	tid5 = pthread_create(&pth[4],NULL,inf_main,(void*)&tinfo[4]);
	tid6 = pthread_create(&pth[5],NULL,inf_main,(void*)&tinfo[5]);
	tid7 = pthread_create(&pth[6],NULL,inf_main,(void*)&tinfo[6]);
	
	
	pthread_join(pth[1],(void**)&status);
	pthread_join(pth[2],(void**)&status);
	pthread_join(pth[3],(void**)&status);
	pthread_join(pth[4],(void**)&status);
	pthread_join(pth[5],(void**)&status);
	pthread_join(pth[6],(void**)&status);
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
