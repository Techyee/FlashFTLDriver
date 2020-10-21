#include "seq_block_manager.h"
#include <stdlib.h>
#include <stdio.h>
extern bb_checker checker;
extern algorithm page_ftl;
struct blockmanager seq_bm={
	.create=seq_create,
	.destroy=seq_destroy,
	.get_block=seq_get_block,
	.pick_block=seq_pick_block,
	.get_segment=seq_get_segment,
	.get_page_num=seq_get_page_num,
	.pick_page_num=seq_pick_page_num,
	.check_full=seq_check_full,
	.is_gc_needed=seq_is_gc_needed, 
	.get_gc_target=seq_get_gc_target,
	.trim_segment=seq_trim_segment,
	.free_segment=seq_free_segment,
	.populate_bit=seq_populate_bit,
	.unpopulate_bit=seq_unpopulate_bit,
	.erase_bit=seq_erase_bit,
	.is_valid_page=seq_is_valid_page,
	.is_invalid_page=seq_is_invalid_page,
	.set_oob=seq_set_oob,
	.get_oob=seq_get_oob,
	.change_reserve=seq_change_reserve,

	.pt_create=NULL,
	.pt_destroy=NULL,
	.pt_get_segment=NULL,
	.pt_get_gc_target=NULL,
	.pt_trim_segment=NULL,

	//registering my own function.
	.get_chip = seq_get_chip,
	.get_page_num_pinned = seq_get_page_num_pinned,
	.get_page_num_gc = seq_get_page_num_gc
};

static uint32_t age = UINT_MAX;

//masterblock pointers for block in seq_get_page_num_pinned
static __block *n[CHN*WAY] = {NULL, };
static __block *reserved[CHN*WAY] = {NULL, };
//may change here afterward(according to task num)

void seq_mh_swap_hptr(void *a, void *b){
	block_set *aa=(block_set*)a;
	block_set *bb=(block_set*)b;

	void *temp=aa->hptr;
	aa->hptr=bb->hptr;
	bb->hptr=temp;
}

void seq_mh_assign_hptr(void *a, void *hn){
	block_set *aa=(block_set*)a;
	aa->hptr=hn;
}

int seq_get_cnt(void *a){
	block_set *aa=(block_set*)a;
	/*
	if(aa->total_invalid_number==UINT_MAX){
		for(uint32_t i=0; i<BPS; i++){
			res+=aa->blocks[i]->invalid_number;
		}
		aa->total_invalid_number=res;
	}
	else res=aa->total_invalid_number;*/

	return aa->total_invalid_number;
}

void block_mh_swap_hptr(void *a, void *b){
	__block *aa=(__block*)a;
	__block *bb=(__block*)b;

	void *temp=aa->hptr;
	aa->hptr=bb->hptr;
	bb->hptr=temp;
}

void block_mh_assign_hptr(void *a, void *hn){
	__block *aa=(__block*)a;
	aa->hptr=hn;
}

int block_get_cnt(void *a){
	__block *aa=(__block*)a;
	return aa->invalid_number;
}

uint32_t seq_create (struct blockmanager* bm, lower_info *li){
	bm->li=li;
	//bb_checker_start(bm->li);/*check if the block is badblock*/
#ifdef AMF
	printf("NOC :%d _NOS:%ld\n", NOC,_NOS);
#endif

	sbm_pri *p=(sbm_pri*)malloc(sizeof(sbm_pri));
	p->seq_block=(__block*)calloc(sizeof(__block), _NOB);
	p->logical_segment=(block_set*)calloc(sizeof(block_set), _NOS);
	p->logical_chip = (cblock_set*)calloc(sizeof(cblock_set), _NOC);
	p->assigned_block=p->free_block=0;
	p->assigned_block_per_chip=0;
	p->free_block_per_chip=0;

	int glob_block_idx=0;
	//initializing segment based data structure.
	for(int i=0; i<_NOS; i++){
		for(int j=0; j<BPS; j++){
			__block *b=&p->seq_block[glob_block_idx];
			b->block_num=i*BPS+j;		
			b->bitset=(uint8_t*)calloc(_PPB*L2PGAP/8,1);
			p->logical_segment[i].blocks[j]=&p->seq_block[glob_block_idx];
			glob_block_idx++;
		}
		p->logical_segment[i].total_invalid_number=0;
	}
	//!init

	//initializing chip based data structure.
	glob_block_idx = 0;
	printf("blocks per chip : %d, number of chip : %d\n",BPC,_NOC);
	for(int i=0;i<_NOC;i++){
		for(int j=0;j<BPC;j++){
			p->logical_chip[i].blocks[j] = &p->seq_block[glob_block_idx];
			glob_block_idx++;
		}
		p->logical_chip[i].total_invalid_number = 0;
	}
	//!init

	mh_init(&p->max_heap, _NOS, seq_mh_swap_hptr, seq_mh_assign_hptr, seq_get_cnt);
	q_init(&p->free_logical_segment_q, _NOS);
		
	
	q_init(&p->free_logical_chip_q, _NOC);
	//!initing my queue

	for(uint32_t i=0; i<_NOS; i++){
		q_enqueue((void*)&p->logical_segment[i], p->free_logical_segment_q);
		p->free_block++;
	}

	for(uint32_t i=0;i<_NOC;i++){
		q_enqueue((void*)&p->logical_chip[i],p->free_logical_chip_q);
		p->free_block_per_chip++;
	}

	bm->private_data=(void*)p;
	return 1;
}

uint32_t seq_destroy (struct blockmanager* bm){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	free(p->seq_block);
	free(p->logical_segment);
	mh_free(p->max_heap);
	q_free(p->free_logical_segment_q);
	free(p);

	//destroying my data structures
	free(p->logical_chip);
	q_free(p->free_logical_chip_q);
	mh_free(p->max_heap_chip);

	return 1;
}

__block* seq_get_block (struct blockmanager* bm, __segment* s){
	if(s->now+1>s->max) abort();
	return s->blocks[s->now++];
}

__segment* seq_get_segment (struct blockmanager* bm, bool isreserve){
	__segment* res=(__segment*)malloc(sizeof(__segment));
	sbm_pri *p=(sbm_pri*)bm->private_data;
	
	block_set *free_block_set=(block_set*)q_dequeue(p->free_logical_segment_q);
	
	if(!free_block_set){
		printf("new block is null!\n");
		abort();
	}


	if(isreserve){

	}
	else{
		free_block_set->total_invalid_number=age--;
		mh_insert_append(p->max_heap, (void*)free_block_set);
	}

	memcpy(res->blocks, free_block_set->blocks, sizeof(__block*)*BPS);

	res->now=0;
	res->max=BPS;
	res->invalid_blocks=0;
	res->used_page_num=0;
	
	p->assigned_block++;
	p->free_block--;

	if(p->assigned_block+p->free_block!=_NOS){
		printf("missing segment error\n");
		abort();
	}

	return res;
}
__chip* seq_get_chip (struct blockmanager* bm, bool isreserve){
	__chip* res = (__chip*)malloc(sizeof(__chip));
	sbm_pri *p=(sbm_pri*)bm->private_data;
	cblock_set *free_block_set =(cblock_set*)q_dequeue(p->free_logical_chip_q);
	if(!free_block_set){
		printf("new block is null!");
		abort();
	}
	
	if(isreserve){}
	else{
		free_block_set->total_invalid_number=age--;
	//	mh_insert_append(p->max_heap_chip,(void*)free_block_set);
	}
	
	//initialize and save available block queue.
	q_init(&res->free_block_queue,BPC);
	q_init(&res->full_block_queue,BPC);
	q_init(&res->rsv_block_queue,BPC);
	for(int i=0;i<BPC;i++){
			q_enqueue(free_block_set->blocks[i],res->free_block_queue);
	}
	
	//!finish saving available block queue.
	
	//initialize the max_heap queue
	mh_init(&res->free_block_maxheap,BPC,block_mh_swap_hptr,block_mh_assign_hptr,block_get_cnt);
	//move block pointer information to chip structure.
	memcpy(res->blocks, free_block_set->blocks,sizeof(__block*)*BPC);

	//reserve some blocks for GC(multiple BGC support).
	for(int i=0;i<NUM_RSV;i++){
		res->reserved_blocks[i] = (__block*)q_dequeue(res->free_block_queue);
		//q_enqueue((void*)(res->reserved_blocks[i]),&res->rsv_block_queue);
		printf("dequeued block num is %d\n",res->reserved_blocks[i]->block_num);
	}
	
	res->now = 0;
	res->max = BPC;
	res->invalid_blocks = 0;
	res->used_page_num = 0;
	p->assigned_block++;
	p->free_block--;
	
	return res;
}
__segment* seq_change_reserve(struct blockmanager* bm,__segment *reserve){

	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t segment_start_block_number=reserve->blocks[0]->block_num;
	uint32_t segment_idx=segment_start_block_number/BPS;
	block_set *bs=&p->logical_segment[segment_idx];
	bs->total_invalid_number=age--;

	mh_insert_append(p->max_heap, (void*)bs);

	return seq_get_segment(bm,true);
}

bool seq_is_gc_needed (struct blockmanager* bm){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	if(p->free_logical_segment_q->size==0) return true;
	return false;
}

__gsegment* seq_get_gc_target (struct blockmanager* bm){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	__gsegment* res=(__gsegment*)malloc(sizeof(__gsegment));
	res->invalidate_number=0;


	/*
	for(uint32_t i=0; i<_NOS; i++){
		p->logical_segment[i].total_invalid_number=UINT_MAX;
	}*/

	mh_construct(p->max_heap);
	block_set* target=(block_set*)mh_get_max(p->max_heap);

	memcpy(res->blocks, target->blocks, sizeof(__block*)*BPS);
	res->invalidate_number=target->total_invalid_number;


	if(res->invalidate_number==0){
		printf("invalid!\n");	
		mh_construct(p->max_heap);
		abort();
	}
	res->now=res->max=0;
	return res;
}

void seq_trim_segment (struct blockmanager* bm, __gsegment* gs, struct lower_info* li){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t segment_startblock_number=gs->blocks[0]->block_num;

	for(int i=0; i<BPS; i++){
		__block *b=gs->blocks[i];
		b->invalid_number=0;
		b->now=0;
		memset(b->bitset,0,_PPB/8);
		memset(b->oob_list,0,sizeof(b->oob_list));
	}

	uint32_t segment_idx=segment_startblock_number/BPS;
	block_set *bs=&p->logical_segment[segment_idx];
	bs->total_invalid_number=0;

	q_enqueue((void*)bs, p->free_logical_segment_q);
	
	p->assigned_block--;
	p->free_block++;

	if(p->assigned_block+p->free_block!=_NOS){
		printf("missing segment error\n");
		abort();
	}

	li->trim_block(segment_startblock_number*_PPB, ASYNC);
	free(gs);
}

int seq_populate_bit (struct blockmanager* bm, uint32_t ppa){
	int res=1;
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t bn=ppa/(_PPB * L2PGAP);
	uint32_t pn=ppa%(_PPB * L2PGAP);
	uint32_t bt=pn/8;
	uint32_t of=pn%8;

	if(p->seq_block[bn].bitset[bt]&(1<<of)){
		res=0;
	}
	p->seq_block[bn].bitset[bt]|=(1<<of);
	return res;
}

int seq_unpopulate_bit (struct blockmanager* bm, uint32_t ppa){
	int res=1;
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t bn=ppa/(_PPB * L2PGAP);
	uint32_t pn=ppa%(_PPB * L2PGAP);
	uint32_t bt=pn/8;
	uint32_t of=pn%8;
	__block *b=&p->seq_block[bn];

	if(!(p->seq_block[bn].bitset[bt]&(1<<of))){
		res=0;
	}
	b->bitset[bt]&=~(1<<of);
	b->invalid_number++;

/*
	uint32_t segment_idx=b->block_num/BPS;
	block_set *seg=&p->logical_segment[segment_idx];
	seg->total_invalid_number++;
*/	
	if(b->invalid_number > _PPB * L2PGAP){
		printf("[abort]block num %d, invalid %d\n",b->block_num, b->invalid_number);
	}
	return res;
}

int seq_erase_bit (struct blockmanager* bm, uint32_t ppa){
	int res=1;
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t bn=ppa/(_PPB * L2PGAP);
	uint32_t pn=ppa%(_PPB * L2PGAP );
	uint32_t bt=pn/8;
	uint32_t of=pn%8;
	__block *b=&p->seq_block[bn];

	if(!(p->seq_block[bn].bitset[bt]&(1<<of))){
		res=0;
	}
	b->bitset[bt]&=~(1<<of);
	if(0<=ppa && ppa< _PPS*MAPPART_SEGS){
		if(b->invalid_number>_PPB){
			abort();
		}
	}
	return res;
}

bool seq_is_valid_page (struct blockmanager* bm, uint32_t ppa){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t bn=ppa/(_PPB*L2PGAP);
	uint32_t pn=ppa%(_PPB * L2PGAP);
	uint32_t bt=pn/8;
	uint32_t of=pn%8;

	return p->seq_block[bn].bitset[bt]&(1<<of);
}

bool seq_is_invalid_page (struct blockmanager* bm, uint32_t ppa){
	return !seq_is_valid_page(bm,ppa);
}

void seq_set_oob(struct blockmanager* bm, char *data,int len, uint32_t ppa){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	__block *b=&p->seq_block[ppa/_PPB];
	memcpy(b->oob_list[ppa%_PPB].d,data,len);
}

char *seq_get_oob(struct blockmanager*bm,  uint32_t ppa){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	uint32_t bidx=ppa/_PPB;
	__block *b=&p->seq_block[bidx];
	return b->oob_list[ppa%_PPB].d;
}

void seq_release_segment(struct blockmanager* bm, __segment *s){
	free(s);
}

int seq_get_page_num(struct blockmanager* bm,__segment *s){
	if(s->now==BPS-1){
		if(s->blocks[BPS-1]->now==_PPB) return -1;
	}
	__block *b=s->blocks[s->now];
	if(b->now==_PPB){
		s->now++;
	}
	int blocknumber=s->now;
	b=s->blocks[blocknumber];
	uint32_t page=b->now++;
	int res=b->block_num*_PPB+page;
	if(page>_PPB) abort();
	s->used_page_num++;
	bm->assigned_page++;
	return res;
}
int seq_get_page_num_pinned(struct blockmanager* bm, __chip *c, int mark, bool isreserve){
	//!!n[mark] is a global masterblock pointer declared in sbm.c!!
	
	__block *b = c->blocks[c->now];
	int reset = 0;
	if(!isreserve){
		if((b->now == _PPB) || (n[mark] == NULL)) { //cur block full or init.

			printf("cur maxheap is %d\n",c->free_block_maxheap->size);
			n[mark] = (__block*)q_dequeue(c->free_block_queue);
			if(n[mark] == NULL){//cannot reclaim (chip full)
				printf("[bench %d] chip is full\n",mark);
				n[mark] = NULL;
				return -1;
			}
			else{//if reclaimed, 
				if(b->now == _PPB){//if cur block is full block,
					mh_insert_append(c->free_block_maxheap,(void*)b);
				}
			}

			c->now = n[mark]->block_num % BPC;
		}//update c->now.
	}

	int blocknumber = c->now;
	b=c->blocks[blocknumber];
	uint32_t page = b->now++;
	int res=b->block_num*_PPB + page;
	//printf("[IO]checking dequeued block, num : %d, page : %u\n",b->block_num , page);
	
	if(page>_PPB){
		abort();
	}
	c->used_page_num++;
	bm->assigned_page++;
	//printf("result page num is %d\n",res);
	return res;
}

int seq_get_page_num_gc(struct blockmanager* bm, __chip *c, int mark, int chip_num, int gc_init){
	
	__block *b = c->blocks[c->gc_now];
	//init gc :: page_num from new reserve block.
	if(gc_init == 1){
		//each task(indexed as mark) has their own reserved blocks.
		reserved[chip_num] = c->reserved_blocks[mark];
		c->gc_now = reserved[chip_num]->block_num % BPC;
	}

	int blocknumber = c->gc_now;
	b=c->blocks[blocknumber];
	//printf("[GC]checking dequeued block, num : %d, page : %u\n",b->block_num,b->now);
	uint32_t page = b->now++;
	int res=b->block_num*_PPB + page;
	if(page>_PPB){
		abort();
	}
	return res;
}
int seq_pick_page_num(struct blockmanager* bm,__segment *s){
	if(s->now==0){
		if(s->blocks[BPS-1]->now==_PPB) return -1;
	}

	int blocknumber=s->now;
	if(s->now==BPS) s->now=0;
	__block *b=s->blocks[blocknumber];
	uint32_t page=b->now;
	int res=b->block_num*_PPB+page;

	if(page>_PPB) abort();
	return res;
}

bool seq_check_full(struct blockmanager *bm,__segment *active, uint8_t type){
	bool res=false;
//	__block *b=active->blocks[active->now];
	switch(type){
		case MASTER_SEGMENT:
			break;
		case MASTER_PAGE:
		case MASTER_BLOCK:
			if(active->blocks[BPS-1]->now==_PPB){
				res=true;
			}
			break;
			/*
			if(active->now >= active->max){
				res=true;
				abort();
			}
			break;*/
	}
	return res;
}

__block *seq_pick_block(struct blockmanager *bm, uint32_t page_num){
	sbm_pri *p=(sbm_pri*)bm->private_data;
	return &p->seq_block[page_num/_PPB];
}

void seq_free_segment(struct blockmanager *, __segment *seg){
	free(seg);
}

