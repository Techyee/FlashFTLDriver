#include "gc.h"
#include "map.h"
#include "../../include/data_struct/list.h"
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
extern algorithm page_ftl;

//pseudo-deadline for GC-IO serving.
//hard coded for 16 chip.
uint32_t pseudo_dl[16] = {1, };
int _g_cur_targ[32] = {0, };
void invalidate_ppa(uint32_t t_ppa){
	if(t_ppa<32768){
		//abort();
	}
	/*when the ppa is invalidated this function must be called*/
	page_ftl.bm->unpopulate_bit(page_ftl.bm, t_ppa);
}

void validate_ppa(uint32_t ppa, KEYT *lbas){
	/*when the ppa is validated this function must be called*/
	for(uint32_t i=0; i<L2PGAP; i++){
		page_ftl.bm->populate_bit(page_ftl.bm,ppa * L2PGAP+i);
	}

	/*this function is used for write some data to OOB(spare area) for reverse mapping*/
	page_ftl.bm->set_oob(page_ftl.bm,(char*)lbas,sizeof(KEYT)*L2PGAP,ppa);
}

gc_value* send_req(uint32_t ppa, uint8_t type, value_set *value){
	algo_req *my_req=(algo_req*)malloc(sizeof(algo_req));
	my_req->parents=NULL;
	my_req->end_req=page_gc_end_req;//call back function for GC
	my_req->type=type;
	//assign pseudo_dl for first serve.
	gettimeofday(&(my_req->algo_init_t),NULL);
	int mark = ppa / (BPC * _PPB);
	my_req->mark = mark;
	my_req->deadline = pseudo_dl[mark];
	pseudo_dl[mark]++;
	/*for gc, you should assign free space for reading valid data*/
	gc_value *res=NULL;
	switch(type){
		case GCDR:
			res=(gc_value*)malloc(sizeof(gc_value));
			res->isdone=false;
			res->ppa=ppa;
			my_req->params=(void *)res;
			my_req->type_lower=0;
			/*when read a value, you can assign free value by this function*/
			res->value=inf_get_valueset(NULL,FS_MALLOC_R,PAGESIZE);
			page_ftl.li->read(ppa,PAGESIZE,res->value,ASYNC,my_req);
			break;
		case GCDW:
			res=(gc_value*)malloc(sizeof(gc_value));
			res->value=value;
			my_req->params=(void *)res;
			page_ftl.li->write(ppa,PAGESIZE,res->value,ASYNC,my_req);
			break;
	}
	return res;
}

void send_req_cb(uint32_t ppa, uint32_t ppa2, uint8_t type){
	algo_req *my_req = (algo_req*)malloc(sizeof(algo_req));
	my_req->parents = NULL;
	my_req->type = type;
	my_req->mark = ppa / (BPC * _PPB);

	my_req->end_req=page_gc_end_req;
	//assing pseudo deadline.
	my_req->deadline = pseudo_dl[my_req->mark];
	pseudo_dl[my_req->mark]++;
	//!finish asssigning pseudo dl.
	switch(type){
	case GCCB:
		page_ftl.li->copyback(ppa,ppa2,PAGESIZE,ASYNC);
		break;
	default:
		printf("send_req_cb only supports copyback!");
		abort();
		break;
	}
}
void chip_gc(int mark){ //!!hard coded for (PAGESIZE == LPAGESIZE) case!!
	//find victim block
	pm_body *p = (pm_body*)page_ftl.algo_body;
	
	mh_construct(p->chip_actives_arr[mark]->free_block_maxheap);
	__block* target = (__block*)mh_get_max(p->chip_actives_arr[mark]->free_block_maxheap);
	//static int targ_block = 1;
	//__block* target = p->chip_actives_arr[mark]->blocks[targ_block];
	//targ_block = (targ_block + 1) % BPC;
	printf("target block grabbed %d\n",target->block_num);
	//params
	__block* reserved;
	gc_value **gv_array = (gc_value**)malloc(sizeof(gc_value*)*_PPB);
	blockmanager *bm=page_ftl.bm;
	align_gc_buffer g_buffer;
	gc_value *gv;
	uint32_t page;
	uint32_t pidx;
	uint32_t res;
	uint32_t gv_idx = 0;
	page = target->block_num * _PPB;
	printf("target : %d, cur res: %d\n",target->block_num,
	p->chip_actives_arr[mark]->reserved_block->block_num);

	int valid_cnt = 0;
	int read_cnt = 0;
	for(pidx=0;pidx!=_PPB;pidx++){
		if(bm->is_valid_page(bm,page)){
			gv=send_req(page,GCDR,NULL);
			gv_array[gv_idx++] = gv;
			valid_cnt++;
		}
		page++;
		read_cnt++;
	}
	printf("valid : %d, read : %d\n",valid_cnt,read_cnt);
	//successfully sent req for gc_read.

	
	uint32_t cur_gv_idx = 0;
	uint32_t done_cnt = 0;
	KEYT *lbas;
	while(done_cnt != gv_idx){
		
		//copy value and address on buffer.
		gv = gv_array[cur_gv_idx];
		if(!gv->isdone){
			goto next_idx;
		}
		lbas = (KEYT*)bm->get_oob(bm,gv->ppa);
		memcpy(&g_buffer.value,gv->value->value,PAGESIZE);
		g_buffer.key[0] = lbas[0];
		//!copy
		res = page_map_gc_update_chip(g_buffer.key,L2PGAP,mark);
		validate_ppa(res,g_buffer.key);
		send_req(res,GCDW,inf_get_valueset(g_buffer.value,FS_MALLOC_W,PAGESIZE));
		done_cnt++;
		//send gc write req.
		
		//inf_free_valueset(gv->value,FS_MALLOC_R);
		//free(gv);
		//return memory.
next_idx:
		cur_gv_idx = (cur_gv_idx+1) % gv_idx;
	}
	
	//successfully sent req for gc_read.
	//trim target block.
	target->invalid_number = 0;
	target->now = 0;
	memset(target->bitset,0,_PPB/8);
	memset(target->oob_list,0,sizeof(target->oob_list));
	page_ftl.li->trim_a_block(target->block_num*_PPB,ASYNC);
	//queue manipulation.
	reserved = p->chip_actives_arr[mark]->reserved_block;
	p->chip_actives_arr[mark]->reserved_block = target;
	q_enqueue(reserved,p->chip_actives_arr[mark]->free_block_queue);
	mh_insert_append(p->chip_actives_arr[mark]->free_block_maxheap,(void*)target);
	//reset pseudo_dl;
	pseudo_dl[mark] = 1;	
}

void chip_gc_cb(int mark){
    pm_body *p = (pm_body*)page_ftl.algo_body;

    //use maxheap to get gc target.
    mh_construct(p->chip_actives_arr[mark]->free_block_maxheap);
	__block* target = (__block*)mh_get_max(p->chip_actives_arr[mark]->free_block_maxheap);
    __block* reserved;

    blockmanager *bm=page_ftl.bm;
    uint32_t page = target->block_num * _PPB;
    uint32_t pidx;
    uint32_t source_ppa;
    uint32_t dest_ppa;
	int v_num = 0;
    KEYT* lbas;

    //for each page in target block, try to send copyback operation.
    for(pidx=0;pidx != _PPB; pidx++){
        if(bm->is_valid_page(bm,page)){
            source_ppa = page;
            lbas = (KEYT*)bm->get_oob(bm,source_ppa);
            dest_ppa = page_map_gc_update_chip(lbas,L2PGAP,mark);
            validate_ppa(dest_ppa,lbas);
            send_req_cb(source_ppa,dest_ppa,GCCB);
			v_num++;
        }
        pidx++;
        page++;
    }
	
	//reset info.
    target->invalid_number = 0;
    target->now = 0;
 	memset(target->bitset,0,_PPB/8);
	memset(target->oob_list,0,sizeof(target->oob_list));
    page_ftl.li->trim_a_block(target->block_num*_PPB,ASYNC);

    //queue manipulation.(swap reserved with target block)
    reserved = p->chip_actives_arr[mark]->reserved_block;
	p->chip_actives_arr[mark]->reserved_block = target;
	q_enqueue(reserved,p->chip_actives_arr[mark]->free_block_queue);
	mh_insert_append(p->chip_actives_arr[mark]->free_block_maxheap,(void*)target);
	pseudo_dl[mark] = 1;
}

void new_do_gc(){
	/*this function return a block which have the most number of invalidated page*/
	__gsegment *target=page_ftl.bm->get_gc_target(page_ftl.bm);
	uint32_t page;
	uint32_t bidx, pidx;
	blockmanager *bm=page_ftl.bm;
	pm_body *p=(pm_body*)page_ftl.algo_body;
	//list *temp_list=list_init();
	gc_value **gv_array=(gc_value**)malloc(sizeof(gc_value*)*_PPS);
	align_gc_buffer g_buffer;
	gc_value *gv;
	uint32_t gv_idx=0;

	/*by using this for loop, you can traversal all page in block*/
	for_each_page_in_seg(target,page,bidx,pidx){
		//this function check the page is valid or not
		gv=send_req(page,GCDR,NULL);
		
		gv_array[gv_idx++]=gv;
	}

	g_buffer.idx=0;
	KEYT *lbas;
	gv_idx=0;
	uint32_t done_cnt=0; 
	while(done_cnt!=_PPS){
		gv=gv_array[gv_idx];
		if(!gv->isdone){
			goto next;
		}
		lbas=(KEYT*)bm->get_oob(bm, gv->ppa);
		for(uint32_t i=0; i<L2PGAP; i++){
			if(page_map_pick(lbas[i])!=gv->ppa*L2PGAP+i) continue;
			memcpy(&g_buffer.value[g_buffer.idx*4096],&gv->value->value[i*4096],4096);
			g_buffer.key[g_buffer.idx]=lbas[i];

			g_buffer.idx++;

			if(g_buffer.idx==L2PGAP){
				uint32_t res=page_map_gc_update(g_buffer.key, L2PGAP);
				validate_ppa(res, g_buffer.key);
				send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE));
				g_buffer.idx=0;
			}
		}

		done_cnt++;
		inf_free_valueset(gv->value, FS_MALLOC_R);
		free(gv);
next:
		gv_idx=(gv_idx+1)%_PPS;
	}	

	if(g_buffer.idx!=0){
		uint32_t res=page_map_gc_update(g_buffer.key, g_buffer.idx);
		validate_ppa(res, g_buffer.key);
		send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE));
		g_buffer.idx=0;	
	}

	bm->trim_segment(bm,target,page_ftl.li); //erase a block

	bm->free_segment(bm, p->active);

	p->active=p->reserve;//make reserved to active block
	p->reserve=bm->change_reserve(bm,p->reserve); //get new reserve block from block_manager
}

static int gc_cnt=0;
void do_gc(){
	/*this function return a block which have the most number of invalidated page*/
	if(gc_cnt < 2){
		printf("gc start\n");
	}
	__gsegment *target=page_ftl.bm->get_gc_target(page_ftl.bm);
	uint32_t page;
	uint32_t bidx, pidx;
	blockmanager *bm=page_ftl.bm;
	pm_body *p=(pm_body*)page_ftl.algo_body;
	list *temp_list=list_init();
	align_gc_buffer g_buffer;
	gc_value *gv;

	/*by using this for loop, you can traversal all page in block*/
	for_each_page_in_seg(target,page,bidx,pidx){
		//this function check the page is valid or not
		bool should_read=false;
		for(uint32_t i=0; i<L2PGAP; i++){
			if(bm->is_invalid_page(bm,page*L2PGAP+i)) continue;
			else{
				should_read=true;
				break;
			}
		}
		if(should_read){
			gv=send_req(page,GCDR,NULL);
			list_insert(temp_list,(void*)gv);
		}
	}

	li_node *now,*nxt;
	g_buffer.idx=0;
	KEYT *lbas;
	while(temp_list->size){
		for_each_list_node_safe(temp_list,now,nxt){

			gv=(gc_value*)now->data;
			if(!gv->isdone) continue;
			lbas=(KEYT*)bm->get_oob(bm, gv->ppa);
			for(uint32_t i=0; i<L2PGAP; i++){
				if(bm->is_invalid_page(bm,gv->ppa*L2PGAP+i)) continue;
				memcpy(&g_buffer.value[g_buffer.idx*4096],&gv->value->value[i*4096],4096);
				g_buffer.key[g_buffer.idx]=lbas[i];

				g_buffer.idx++;

				if(g_buffer.idx==L2PGAP){
					uint32_t res=page_map_gc_update(g_buffer.key, L2PGAP);
					validate_ppa(res, g_buffer.key);
					send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE));
					g_buffer.idx=0;
				}
			}

			inf_free_valueset(gv->value, FS_MALLOC_R);
			free(gv);
			//you can get lba from OOB(spare area) in physicall page
			list_delete_node(temp_list,now);
		}
	}

	if(g_buffer.idx!=0){
		uint32_t res=page_map_gc_update(g_buffer.key, g_buffer.idx);
		validate_ppa(res, g_buffer.key);
		send_req(res, GCDW, inf_get_valueset(g_buffer.value, FS_MALLOC_W, PAGESIZE));
		g_buffer.idx=0;	
	}

	bm->trim_segment(bm,target,page_ftl.li); //erase a block

	bm->free_segment(bm, p->active);

	p->active=p->reserve;//make reserved to active block
	p->reserve=bm->change_reserve(bm,p->reserve); //get new reserve block from block_manager

	list_free(temp_list);
	if(gc_cnt < 2){
		printf("gc end\n");
	}
}


ppa_t get_ppa(KEYT *lbas){
	uint32_t res;
	static uint32_t cnt=0;
	if(cnt++==2302004){
		printf("break!\n");
	}
	pm_body *p=(pm_body*)page_ftl.algo_body;
	/*you can check if the gc is needed or not, using this condition*/
	if(page_ftl.bm->check_full(page_ftl.bm, p->active,MASTER_PAGE) && page_ftl.bm->is_gc_needed(page_ftl.bm)){
		new_do_gc();//call gc
	}

retry:
	/*get a page by bm->get_page_num, when the active segment doesn't have block, return UINT_MAX*/
	res=page_ftl.bm->get_page_num(page_ftl.bm,p->active);
	usleep(50000);
	if(res==UINT32_MAX){
		page_ftl.bm->free_segment(page_ftl.bm, p->active);
		p->active=page_ftl.bm->get_segment(page_ftl.bm,false); //get a new block
		goto retry;
	}

	/*validate a page*/
	validate_ppa(res,lbas);
	//printf("assigned %u\n",res);
	return res;
}

ppa_t get_ppa_pinned(KEYT *lbas, int mark, int chip_num, int* chip_idx){
	//imagine a task is pinned on chip 1~4.
	uint32_t res;
	int target_idx;
	int target_chip;
	
	static uint32_t cnt = 0;
	cnt++;
	pm_body *p = (pm_body*)page_ftl.algo_body;
	struct timeval gc_init;
	struct timeval gc_end;
retry:
	//selection of target chip. use _g_cur_targ[mark] to track current target.
	target_idx = _g_cur_targ[mark];
	_g_cur_targ[mark] = (_g_cur_targ[mark]+1) % chip_num;
	target_chip = chip_idx[target_idx];
	//!end of selection.
	res = page_ftl.bm->get_page_num_pinned(page_ftl.bm,p->chip_actives_arr[target_chip],target_chip,false);
	//old logic. hard coded partitioning.
	/*
	if(mark == 0) res = page_ftl.bm->get_page_num_pinned(page_ftl.bm,p->chip_actives_arr[target], target, false);
	else if(mark == 1) res = page_ftl.bm->get_page_num_pinned(page_ftl.bm,p->chip_actives_arr[1], mark, false);
	else if(mark == 2) res = page_ftl.bm->get_page_num_pinned(page_ftl.bm,p->chip_actives_arr[2], mark, false);
	else{
		printf("A mark of this bench is not registered!\n");
		abort();
	}*/
	//get a page by newly created get_page_num_pinned.


	//if a page is not available, go through garbage collection.
	if(res == UINT32_MAX){
		gettimeofday(&(gc_init),NULL);
		//chip_gc(mark);
		chip_gc_cb(target_chip);
		gettimeofday(&(gc_end),NULL);
		goto retry;
	}
	validate_ppa(res,lbas);
	return res;
}

void *page_gc_end_req(algo_req *input){
	gc_value *gv=(gc_value*)input->params;
	switch(input->type){
		case GCDR:
			gv->isdone=true;
			break;
		case GCDW:
			/*free value which is assigned by inf_get_valueset*/
			inf_free_valueset(gv->value,FS_MALLOC_R);
			free(gv);
			break;
	}
	free(input);
	return NULL;
}
