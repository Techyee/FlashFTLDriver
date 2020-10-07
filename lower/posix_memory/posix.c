#define _LARGEFILE64_SOURCE
#include "posix.h"
#include "pipe_lower.h"
#include "../../blockmanager/bb_checker.h"
#include "../../include/settings.h"
#include "../../bench/bench.h"
#include "../../bench/measurement.h"
#include "../../interface/queue.h"
#include "../../interface/bb_checker.h"
#include "../../include/utils/cond_lock.h"
#include "../../include/data_struct/heap.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <pthread.h>
#include <limits.h>
//#include <readline/readline.h>
//#include <readline/history.h>
#define LASYNC 1
#define LATENCY

pthread_mutex_t fd_lock;
mem_seg *seg_table;
#if (LASYNC==1)
queue *p_q;
pthread_t t_id;

bool stopflag;
#endif
#define PPA_LIST_SIZE (240*1024)
cl_lock *lower_flying;
cl_lock *latency_flying[2];
char *invalidate_ppa_ptr;
char *result_addr;

//my data
queue* req_station[16]; //!!hard coded for 4way 4chip.
minh* req_minheap[16]; //!!hard coded form 4way 4chip.
int req_station_init = 0;
pthread_mutex_t cnt_lock;
pthread_mutex_t profile_lock[4]; // cur task = 4. change if necessary
FILE* pfs[4];
int _g_req_cnt = 0;
int flying_num[16] = {0, };
//!my data

lower_info my_posix={

	.create=posix_create,
	.destroy=posix_destroy,
#if (LASYNC==1)
	.write=posix_make_push,
	.read=posix_make_pull,
#elif (LASYNC==0)
	.write=posix_push_data,
	.read=posix_pull_data,
#endif
	.device_badblock_checker=NULL,
#if (LASYNC==1)
	.trim_block=posix_make_trim,
	.trim_a_block=posix_make_trim,
#elif (LASYNC==0)
	.trim_block=posix_trim_block,
	.trim_a_block=posix_trim_a_block,
#endif
	.refresh=posix_refresh,
	.stop=posix_stop,
	.lower_alloc=NULL,
	.lower_free=NULL,
	.lower_flying_req_wait=posix_flying_req_wait,
	.lower_show_info=NULL,
	.lower_tag_num=NULL,
#ifdef Lsmtree
	.read_hw=posix_read_hw,
	.hw_do_merge=posix_hw_do_merge,
	.hw_get_kt=posix_hw_get_kt,
	.hw_get_inv=posix_hw_get_inv
#endif

//my function
#if (LASYNC==1)
	.copyback=posix_make_copyback,
#elif (LASYNC==0)
	.copyback=posix_copyback
#endif
};

 uint32_t d_write_cnt, m_write_cnt, gcd_write_cnt, gcm_write_cnt;


//minheap fuctions for EDF-style request picking.
void req_mh_swap_hptr(void *a, void *b){
	posix_request *aa=(posix_request*)a;
	posix_request *bb=(posix_request*)b;

	void *temp=aa->hptr;
	aa->hptr=bb->hptr;
	bb->hptr=temp;
}

void req_mh_assign_hptr(void *a, void *hn){
	posix_request *aa=(posix_request*)a;
	aa->hptr=hn;
}

int req_get_cnt(void *a){
	posix_request *aa=(posix_request*)a;
	return aa->deadline;
}
//!minheap functions.

#if (LASYNC==1)
void *l_main(void *__input){

	posix_request *inf_req;
	int target_chip;
	int req_num = 0;

	//make a queue for low-levl latency generation.
	for(int i=0;i<16;i++){
		q_init(&req_station[i],10240);
		minh_init(&req_minheap[i],10240,req_mh_swap_hptr,req_mh_assign_hptr,req_get_cnt);
	}

	//cinfo & profile_lock init.(hardcoded)
	chip_info** cinfo = (chip_info**)malloc(sizeof(chip_info*) * 4);
	
	for(int i=0;i<4;i++){
		cinfo[i] = (chip_info*)malloc(sizeof(chip_info));
		cinfo[i]->latency = cl_init(QDEPTH,true);
		pthread_mutex_init(&(cinfo[i]->chip_heap_lock),NULL);
		cinfo[i]->mark = i;
	}
	//prof lock.
	
	for(int i=0;i<4;i++)
		pthread_mutex_init(&(profile_lock[i]),NULL);
	//!inited.

	//open a profile csv.
	
	for(int i=0;i<4;i++){
		char name[100];
		sprintf(name,"[bench %d]profile.csv",i);
		pfs[i] = fopen(name,"w");
	}

	//init chip_mains.
	pthread_mutex_init(&(cnt_lock),NULL);
	pthread_create(&(cinfo[0]->chip_pid),NULL,&new_latency_main,(void*)cinfo[0]);
	pthread_create(&(cinfo[1]->chip_pid),NULL,&new_latency_main,(void*)cinfo[1]);
	pthread_create(&(cinfo[2]->chip_pid),NULL,&new_latency_main,(void*)cinfo[2]);
	pthread_create(&(cinfo[3]->chip_pid),NULL,&new_latency_main,(void*)cinfo[3]);

	//l_main while loop
	while(1){
		if(stopflag){
			printf("posix bye bye! processed req num : %d\n",req_num);
			for(int i=0;i<4;i++)
				fclose(pfs[i]);	
			pthread_exit(NULL);
			break;
		}
		cl_grap(lower_flying);
		
		if(!(inf_req=(posix_request*)q_dequeue(p_q))){
			continue;
		} 
#ifdef LATENCY //latency mode. l_main serves as a splitter for each chip.
		target_chip = inf_req->trim_mark;
		
		switch(inf_req->type){
			case FS_LOWER_W:
				pthread_mutex_lock(&(cinfo[target_chip]->chip_heap_lock));
				minh_insert_append(req_minheap[inf_req->upper_req->mark],(void*)inf_req);
				pthread_mutex_unlock(&(cinfo[target_chip]->chip_heap_lock));
				gettimeofday(&(inf_req->dev_init_t),NULL);
				//q_enqueue((void*)inf_req,req_station[target_chip]);
				cl_release(cinfo[target_chip]->latency);
				
				break;

			case FS_LOWER_R:
				pthread_mutex_lock(&(cinfo[target_chip]->chip_heap_lock));
				minh_insert_append(req_minheap[inf_req->upper_req->mark],(void*)inf_req);
				pthread_mutex_unlock(&(cinfo[target_chip]->chip_heap_lock));
				gettimeofday(&(inf_req->dev_init_t),NULL);
				//q_enqueue((void*)inf_req,req_station[target_chip]);
				cl_release(cinfo[target_chip]->latency);
				
				break;

			case FS_LOWER_T:
				pthread_mutex_lock(&(cinfo[target_chip]->chip_heap_lock));
				minh_insert_append(req_minheap[inf_req->trim_mark],(void*)inf_req);
				pthread_mutex_unlock(&(cinfo[target_chip]->chip_heap_lock));
				gettimeofday(&(inf_req->dev_init_t),NULL);
				//q_enqueue((void*)inf_req,req_station[target_chip]);
				cl_release(cinfo[target_chip]->latency);
				
				break;

			case FS_LOWER_C:
				pthread_mutex_lock(&(cinfo[target_chip]->chip_heap_lock));
				minh_insert_append(req_minheap[inf_req->trim_mark],(void*)inf_req);
				pthread_mutex_unlock(&(cinfo[target_chip]->chip_heap_lock));
				gettimeofday(&(inf_req->dev_init_t),NULL);
				//q_enqueue((void*)inf_req,req_station[target_chip]);
				cl_release(cinfo[target_chip]->latency);
				
				break;
		} //dealing with high-level queue.

#else //non latency mode. initiate operation immediately in single-queue manner.
		switch(inf_req->type){
			case FS_LOWER_W:
			
				posix_push_data(inf_req->key, inf_req->size, inf_req->value, inf_req->isAsync, inf_req->upper_req);
				break;
			case FS_LOWER_R:
				
				posix_pull_data(inf_req->key, inf_req->size, inf_req->value, inf_req->isAsync, inf_req->upper_req);
				break;
			case FS_LOWER_T:
				
				posix_trim_a_block(inf_req->key, inf_req->isAsync);
				break;

			case FS_LOWER_C:

				posix_copyback(inf_req->key,inf_req->key2,inf_req->size,inf_req->isAsync);
				break;
		} //dealing with high-level queue.
#endif
		
#ifndef LATENCY
		free(inf_req);
#endif	
	}//end of while loop
	return NULL;
}

void *new_latency_main(void *arg){ //latency generater main code.
	chip_info* recv = (chip_info*)arg;
	posix_request* active = NULL;
	bool show_latency = true;
	uint32_t algo_elapse;
	uint32_t lower_elapse;
	uint32_t dev_elapse;
	int lat_req = 0;

	while(1){
		if(stopflag){	
			pthread_exit(NULL);
			break;
		}
		cl_grap(recv->latency);
		pthread_mutex_lock(&(recv->chip_heap_lock));
		//active = (posix_request*)q_dequeue(req_station[recv->mark]);
		minh_construct(req_minheap[recv->mark]);
		active = (posix_request*)minh_get_min(req_minheap[recv->mark]);
		pthread_mutex_unlock(&(recv->chip_heap_lock));
		
		if(active != NULL){
			switch(active->type){
				case FS_LOWER_W:
					usleep(500);
					posix_push_data(active->key, active->size, active->value, active->isAsync, active->upper_req);
					gettimeofday(&(active->dev_end_t),NULL);
					if(show_latency == true){
						algo_elapse = (active->l_init_t.tv_sec - active->algo_init_t.tv_sec)*1000000 + active->l_init_t.tv_usec - active->algo_init_t.tv_usec;
						lower_elapse = (active->dev_init_t.tv_sec - active->l_init_t.tv_sec)*1000000 + active->dev_init_t.tv_usec - active->l_init_t.tv_usec;
						dev_elapse = (active->dev_end_t.tv_sec - active->dev_init_t.tv_sec)*1000000 + active->dev_end_t.tv_usec - active->dev_init_t.tv_usec;
						if(dev_elapse > 1000000000){
							printf("what happen? end : %ld, init : %ld\n",active->dev_end_t.tv_usec,active->dev_init_t.tv_usec);
						}
						pthread_mutex_lock(&(profile_lock[active->bench_idx]));
						fprintf(pfs[active->bench_idx],"%ld, %ld, %ld, %ld, %ld\n",recv->mark, active->bench_idx, algo_elapse, lower_elapse, dev_elapse);
						pthread_mutex_unlock(&(profile_lock[active->bench_idx]));
					}
					free(active);
					active = NULL;
					break;

				case FS_LOWER_R:
					usleep(50);
					posix_pull_data(active->key, active->size, active->value, active->isAsync, active->upper_req);
					gettimeofday(&(active->dev_end_t),NULL);
					if(show_latency == true){
						algo_elapse = active->l_init_t.tv_sec - active->algo_init_t.tv_sec + active->l_init_t.tv_usec - active->algo_init_t.tv_usec;
						lower_elapse = active->dev_init_t.tv_sec - active->l_init_t.tv_sec + active->dev_init_t.tv_usec - active->l_init_t.tv_usec;
						dev_elapse = active->dev_end_t.tv_sec - active->dev_init_t.tv_sec + active->dev_end_t.tv_usec - active->dev_init_t.tv_usec;
						pthread_mutex_lock(&(profile_lock[active->bench_idx]));
						fprintf(pfs[active->bench_idx],"%ld, %ld, %ld, %ld, %ld\n",recv->mark, active->bench_idx, algo_elapse, lower_elapse, dev_elapse);
						pthread_mutex_unlock(&(profile_lock[active->bench_idx]));
					}
					free(active);
					active = NULL;
					break;

				case FS_LOWER_T:
					
					usleep(5000);
					posix_trim_a_block(active->key, active->isAsync);
					gettimeofday(&(active->dev_end_t),NULL);
					free(active);
					active = NULL;
					break;

				case FS_LOWER_C:
				
					usleep(550);
					posix_copyback(active->key,active->key2,active->size,active->isAsync);
					gettimeofday(&(active->dev_end_t),NULL);
					free(active);
					active = NULL;
					break;
			}//generate artificial latency to current I/O operation.
		}
	}
}

/*
void *latency_main(void *__input){
	//latency enforcer for each request. hardcoded for 16 chips.
	//when time reaches right latency, end it & free it.
	struct timeval cur_time;
	posix_request* active[16] = {NULL, };
	int elapsed;
	int chip_idle = 0;
	bool count_flag;

	while(1){
		if(req_station_init != 1){
			printf("latency generator halted\n");
			continue;
		}
		
		if(stopflag){
			//printf("posix bye bye!\n");
			pthread_exit(NULL);
			break;
		}
		//communicating with l_main. now we have to consider all 16 chips.
		chip_idle = 0;
		for(int i=0;i<16;i++){//iterate through all chips
			if(flying_num[i] == 0){//no flying l_main.
				if(active[i] == NULL){//no active request
					chip_idle++;
					continue;
				}
				else{//active request exists
					count_flag = true;
				}
			}	
			else{//flying req exists.
				if(active[i] == NULL){//cur active gone.
					
					//pthread_mutex_lock(&latency_lock);
					//minh_construct(req_minheap[i]);
					//active[i] = (posix_request*)minh_get_min(req_minheap[i]);
					active[i] = (posix_request*)q_dequeue(req_station[i]);
					flying_num[i]--;
					//pthread_mutex_unlock(&latency_lock);
					gettimeofday(&(active[i]->dev_init_t),NULL);
					count_flag = true;
				}
				else{//cur active still exists.
					count_flag = true;
				}
			}
			if (chip_idle == 16){ count_flag = false;}
		}
		//!communication done. actives for certain chip is selected.
		
		gettimeofday(&cur_time,NULL); //set current time.
		if(count_flag == true){
			for(int i=0;i<16;i++){
				if(active[i] != NULL){//when i-th chip has non-null active request,
					elapsed = (cur_time.tv_sec - active[i]->dev_init_t.tv_sec ) * 1000000 + (cur_time.tv_usec - active[i]->dev_init_t.tv_usec);
	
					if((elapsed >= 0) && (active[i]->type == FS_LOWER_W)){
						posix_push_data(active[i]->key, active[i]->size, active[i]->value, active[i]->isAsync, active[i]->upper_req);
						//free(active[i]);
						active[i] = NULL;
					}
					else if((elapsed >= 0) && (active[i]->type == FS_LOWER_R)){
						posix_pull_data(active[i]->key, active[i]->size, active[i]->value, active[i]->isAsync, active[i]->upper_req);
						//free(active[i]);
						active[i] = NULL;
					}
					else if((elapsed >= 0) && (active[i]->type == FS_LOWER_T)){
						posix_trim_a_block(active[i]->key, active[i]->isAsync);
						//free(active[i]);
						active[i] = NULL;
					}
				}
			}
		}
	}
	return NULL;
}
*/

void *posix_make_push(uint32_t PPA, uint32_t size, value_set* value, bool async, algo_req *const req){

	bool flag=false;
	posix_request *p_req=(posix_request*)malloc(sizeof(posix_request));
	
	p_req->type=FS_LOWER_W;
	p_req->key=PPA;
	p_req->value=value;
	p_req->upper_req=req;
	p_req->isAsync=async;
	p_req->size=size;
	p_req->bench_idx = req->bench_idx;
	//my data.
	p_req->deadline = req->deadline;
	p_req->trim_mark = req->mark;
	p_req->algo_init_t.tv_sec = req->algo_init_t.tv_sec;
	p_req->algo_init_t.tv_usec = req->algo_init_t.tv_usec;
	while(!flag){
		if(q_enqueue((void*)p_req,p_q)){
			gettimeofday(&(p_req->l_init_t), NULL);
			cl_release(lower_flying);
			flag=true;
		}
	}
	return NULL;
}

void *posix_make_pull(uint32_t PPA, uint32_t size, value_set* value, bool async, algo_req *const req){
	bool flag=false;
	posix_request *p_req=(posix_request*)malloc(sizeof(posix_request));
	
	p_req->type=FS_LOWER_R;
	p_req->key=PPA;
	p_req->value=value;
	p_req->upper_req=req;
	p_req->isAsync=async;
	p_req->size=size;
	p_req->bench_idx = req->bench_idx;
	req->type_lower=0;
	bool once=true;
	//my data.
	p_req->deadline = req->deadline;
	p_req->trim_mark = req->mark;
	p_req->algo_init_t.tv_sec = req->algo_init_t.tv_sec;
	p_req->algo_init_t.tv_usec = req->algo_init_t.tv_usec;

	while(!flag){
		if(q_enqueue((void*)p_req,p_q)){
			gettimeofday(&(p_req->l_init_t), NULL);
			cl_release(lower_flying);
			flag=true;
		}	
		if(!flag && once){
			req->type_lower=1;
			once=false;
		}
	}
	return NULL;
}

void *posix_make_trim(uint32_t PPA, bool async){
	bool flag=false;
	posix_request *p_req=(posix_request*)malloc(sizeof(posix_request));
	p_req->type=FS_LOWER_T;
	p_req->key=PPA;
	p_req->isAsync=async;
	p_req->deadline = _PPB + 1;
	p_req->trim_mark = PPA / (_PPB * BPC);
	printf("trim enqueued.\n");
	while(!flag){
		if(q_enqueue((void*)p_req,p_q)){
			gettimeofday(&(p_req->l_init_t), NULL);
			cl_release(lower_flying);
			flag=true;
		}
	}
	return NULL;
}


void *posix_make_copyback(uint32_t ppa, uint32_t ppa2, uint32_t size, bool async){
	
	bool flag = false;
	posix_request *p_req = (posix_request*)malloc(sizeof(posix_request));
	p_req->type = FS_LOWER_C;
	p_req->key = ppa;
	p_req->key2 = ppa2;
	p_req->isAsync=async;
	p_req->size = size;
	p_req->trim_mark = ppa / (_PPB * BPC);
	p_req->deadline = 1;
	while(!flag){
		if(q_enqueue((void*)p_req,p_q)){
			gettimeofday(&(p_req->l_init_t), NULL);
			cl_release(lower_flying);
			flag=true;
		}
	}
	return NULL;
}
#endif

uint32_t posix_create(lower_info *li, blockmanager *b){
	li->NOB=_NOS;
	li->NOP=_NOP;
	li->SOB=BLOCKSIZE*BPS;
	li->SOP=PAGESIZE;
	li->SOK=sizeof(uint32_t);
	li->PPB=_PPB;
	li->PPS=_PPS;
	li->TS=TOTALSIZE;
	lower_flying=cl_init(QDEPTH,true);//large queue
	
	invalidate_ppa_ptr=(char*)malloc(sizeof(uint32_t)*PPA_LIST_SIZE*20);
	result_addr=(char*)malloc(8192*(PPA_LIST_SIZE));

	printf("!!! posix memory LASYNC: %d NOP:%d!!!\n", LASYNC,li->NOP);
	li->write_op=li->read_op=li->trim_op=0;
	seg_table = (mem_seg*)malloc(sizeof(mem_seg)*li->NOP);
	for(uint32_t i = 0; i < li->NOP; i++){
		seg_table[i].storage = NULL;
	}
	pthread_mutex_init(&fd_lock,NULL);
#if (LASYNC==1)
	stopflag = false;
	q_init(&p_q, 10240);
	pthread_create(&t_id,NULL,&l_main,NULL);
#endif

	memset(li->req_type_cnt,0,sizeof(li->req_type_cnt));

	return 1;
}

void *posix_refresh(lower_info *li){
	li->write_op=li->read_op=li->trim_op=0;
	return NULL;
}

void *posix_destroy(lower_info *li){
	for(int i=0; i<LREQ_TYPE_NUM;i++){
		fprintf(stderr,"%s %lu\n",bench_lower_type(i),li->req_type_cnt[i]);
	}
	for(uint32_t i = 0; i < li->NOP; i++){
		free(seg_table[i].storage);
	}
	free(seg_table);
	pthread_mutex_destroy(&fd_lock);
	free(invalidate_ppa_ptr);
	free(result_addr);
#if (LASYNC==1)
	stopflag = true;
	q_free(p_q);
#endif

	return NULL;
}

static uint8_t convert_type(uint8_t type) {
	return (type & (0xff>>1));
}
extern bb_checker checker;
inline uint32_t convert_ppa(uint32_t PPA){
	return PPA;
}
void *posix_copyback(uint32_t _PPA, uint32_t _PPA2, uint32_t size, bool async){
	//this operation mimics copyback operation.
	//don't have to consider external data I/O
	uint32_t PPA=convert_ppa(_PPA);
	uint32_t PPA2=convert_ppa(_PPA2);
	//printf("source : %d, dest : %d\n",PPA,PPA2);
	if((PPA>_NOP) || (PPA2>_NOP)){
		printf("address error!\n");
		abort();
	}
	value_set* value = (value_set*)malloc(sizeof(value_set));
	value->value = (PTR)malloc(PAGESIZE);
	//pull data.
	if(!seg_table[PPA].storage){
		printf("%u not populated!\n",PPA);
		abort();
	} else {
		memcpy(value->value,seg_table[PPA].storage,PAGESIZE);
	}
	//push data.
	if(!seg_table[PPA2].storage){
		seg_table[PPA2].storage = (PTR)malloc(PAGESIZE);
	}
	else{
		printf("storage is already malloc-ed...\n");
		abort();
	}
	
	my_posix.req_type_cnt[GCCB]++;
	memcpy(seg_table[PPA2].storage,value->value,PAGESIZE);
	free(value);
	return NULL;
}

void *posix_push_data(uint32_t _PPA, uint32_t size, value_set* value, bool async,algo_req *const req){
	
	uint8_t test_type;
	uint32_t PPA=convert_ppa(_PPA);
	if(PPA==8192){
		printf("8192 populate!\n");
	}

	if(PPA>_NOP){
		printf("address error!\n");
		abort();
	}
	if(value->dmatag==-1){
		printf("dmatag -1 error!\n");
		abort();
	}

	if(my_posix.SOP*PPA >= my_posix.TS){
		printf("\nwrite error\n");
		abort();
	}

	test_type = convert_type(req->type);

	if(test_type < LREQ_TYPE_NUM){
		my_posix.req_type_cnt[test_type]++;
	}

	if(!seg_table[PPA].storage){
		seg_table[PPA].storage = (PTR)malloc(PAGESIZE);
	}
	else{
		abort();
	}
	memcpy(seg_table[PPA].storage,value->value,size);
	req->end_req(req);

	
	return NULL;
}

void *posix_pull_data(uint32_t _PPA, uint32_t size, value_set* value, bool async,algo_req *const req){
	struct timeval t_init;
	gettimeofday(&t_init,NULL);
	uint8_t test_type;
	uint32_t PPA=convert_ppa(_PPA);
	if(PPA>_NOP){
		printf("address error!\n");
		abort();
	}

	if(req->type_lower!=1 && req->type_lower!=0){
		req->type_lower=0;
	}
	if(value->dmatag==-1){
		printf("dmatag -1 error!\n");
		abort();
	}


	if(my_posix.SOP*PPA >= my_posix.TS){
		printf("\nread error\n");
		abort();
	}

	test_type = convert_type(req->type);
	if(test_type < LREQ_TYPE_NUM){
		my_posix.req_type_cnt[test_type]++;
	}
	if(!seg_table[PPA].storage){
		printf("%u not populated!\n",PPA);
		abort();
	} else {
		memcpy(value->value,seg_table[PPA].storage,size);
	}
	req->type_lower=1;
	req->end_req(req);
	return NULL;
}

void *posix_trim_block(uint32_t _PPA, bool async){
	struct timeval trim_init;
	gettimeofday(&trim_init,NULL);
	
	uint32_t PPA=convert_ppa(_PPA);
	if(my_posix.SOP*PPA >= my_posix.TS || PPA%my_posix.PPS != 0){
		printf("\ntrim error\n");
		abort();
	}
	
	my_posix.req_type_cnt[TRIM]++;
	for(uint32_t i=PPA; i<PPA+my_posix.PPS; i++){
		free(seg_table[i].storage);
		seg_table[i].storage=NULL;
	}
	
	return NULL;
}

void posix_stop(){}

void posix_flying_req_wait(){
#if (LASYNC==1)
	while(p_q->size!=0){}
#endif
}

void* posix_trim_a_block(uint32_t _PPA, bool async){
	struct timeval t_init;
	struct timeval t_end;
	gettimeofday(&t_init, NULL);
	uint32_t PPA=convert_ppa(_PPA);
	if(PPA>_NOP){
		printf("address error!\n");
		abort();
	}
	my_posix.req_type_cnt[TRIM]++;
	static int cnt=0;
	for(int i=0; i<_PPB; i++){
		//uint32_t t=PPA+i*PUNIT;
		uint32_t t=PPA+i;
		if(!seg_table[t].storage){
			//abort();
		}
		free(seg_table[t].storage);
		seg_table[t].storage=NULL;
	}
	
	gettimeofday(&t_end, NULL);
	//printf("trim time : %d us \n",(t_end.tv_sec - t_init.tv_sec) * 1000000 + t_end.tv_usec - t_init.tv_usec);
	return NULL;
}

void print_array(uint32_t *arr, int num){
	printf("target:");
	for(int i=0; i<num; i++) printf("%d, ",arr[i]);
	printf("\n");
}
#ifdef Lsmtree
uint32_t posix_hw_do_merge(uint32_t lp_num, ppa_t *lp_array, uint32_t hp_num,ppa_t *hp_array,ppa_t *tp_array, uint32_t* ktable_num, uint32_t *invliadate_num){
	if(lp_num==0 || hp_num==0){
		fprintf(stderr,"l:%d h:%d\n",lp_num,hp_num);
		abort();
	}
	pl_body *lp, *hp, *rp;
	lp=plbody_init(seg_table,lp_array,lp_num);
	hp=plbody_init(seg_table,hp_array,hp_num);
	rp=plbody_init(seg_table,tp_array,lp_num+hp_num);
	
	uint32_t lppa, hppa, rppa;
	KEYT lp_key=plbody_get_next_key(lp,&lppa);
	KEYT hp_key=plbody_get_next_key(hp,&hppa);
	KEYT insert_key;
	int next_pop=0;
	int result_cnt=0;
	int invalid_cnt=0;
	char *res_data;
	while(!(lp_key.len==UINT8_MAX && hp_key.len==UINT8_MAX)){
		if(lp_key.len==UINT8_MAX){
			insert_key=hp_key;
			rppa=hppa;
			next_pop=1;
		}
		else if(hp_key.len==UINT8_MAX){
			insert_key=lp_key;
			rppa=lppa;
			next_pop=-1;
		}
		else{
			if(!KEYVALCHECK(lp_key)){
				printf("%.*s\n",KEYFORMAT(lp_key));
				abort();
			}
			if(!KEYVALCHECK(hp_key)){
				printf("%.*s\n",KEYFORMAT(hp_key));
				abort();
			}

			next_pop=KEYCMP(lp_key,hp_key);
			if(next_pop<0){
				insert_key=lp_key;
				rppa=lppa;
			}
			else if(next_pop>0){
				insert_key=hp_key;
				rppa=hppa;
			}
			else{
				memcpy(&invalidate_ppa_ptr[invalid_cnt++*sizeof(uint32_t)],&lppa,sizeof(lppa));
				rppa=hppa;
				insert_key=hp_key;
			}
		}
	
		if((res_data=plbody_insert_new_key(rp,insert_key,rppa,false))){
			if(result_cnt>=hp_num+lp_num){
				printf("%d %d %d\n",result_cnt, hp_num, lp_num);
			}
			memcpy(&result_addr[result_cnt*PAGESIZE],res_data,PAGESIZE);
		//	plbody_data_print(&result_addr[result_cnt*PAGESIZE]);		
			result_cnt++;
		}
		
		if(next_pop<0) lp_key=plbody_get_next_key(lp,&lppa);
		else if(next_pop>0) hp_key=plbody_get_next_key(hp,&hppa);
		else{
			lp_key=plbody_get_next_key(lp,&lppa);
			hp_key=plbody_get_next_key(hp,&hppa);
		}
	}

	if((res_data=plbody_insert_new_key(rp,insert_key,0,true))){
		if(result_cnt>PPA_LIST_SIZE*2){
			printf("too many result!\n");
			abort();
		}
		memcpy(&result_addr[result_cnt*PAGESIZE],res_data,PAGESIZE);
		//	plbody_data_print(&result_addr[result_cnt*PAGESIZE]);		
		result_cnt++;
	}
	*ktable_num=result_cnt;
	*invliadate_num=invalid_cnt;
	free(rp);
	free(lp);
	free(hp);
	return 1;
}

char * posix_hw_get_kt(){
	return result_addr;
}

char *posix_hw_get_inv(){
	return invalidate_ppa_ptr;
}

void* posix_read_hw(uint32_t _PPA, char *key,uint32_t key_len, value_set *value,bool async,algo_req * const req){
	uint32_t PPA=convert_ppa(_PPA);
	if(PPA>_NOP){
		printf("address error!\n");
		abort();
	}

	if(req->type_lower!=1 && req->type_lower!=0){
		req->type_lower=0;
	}
	if(value->dmatag==-1){
		printf("dmatag -1 error!\n");
		abort();
	}

	pthread_mutex_lock(&fd_lock);

	my_posix.req_type_cnt[MAPPINGR]++;
	if(my_posix.SOP*PPA >= my_posix.TS){
		printf("\nread error\n");
		abort();
	}

	if(!seg_table[PPA].storage){
		printf("%u not populated!\n",PPA);
		abort();
	}
	pthread_mutex_unlock(&fd_lock);
	KEYT temp;
	temp.key=key;
	temp.len=key_len;

	uint32_t res=find_ppa_from(seg_table[PPA].storage,key,key_len);
	if(res==UINT32_MAX){
		req->type=UINT8_MAX;
	}
	else{
		req->ppa=res;
	}
	req->end_req(req);
	//memcpy(value->value,seg_table[PPA].storage,size);
	//req->type_lower=1;
	return NULL;
}
#endif
