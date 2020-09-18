#ifndef __S_BM_HEADER
#define __S_BM_HEADER
#include "../../include/container.h"
#include "../../interface/queue.h"
#include "../../include/data_struct/heap.h"
#include "../bb_checker.h"
#include <stdint.h>
#include <unistd.h>
typedef struct block_set{
	uint32_t total_invalid_number;
	__block *blocks[BPS];
	void *hptr;
}block_set;

typedef struct chip_block_set{
	uint32_t total_invalid_number;
	__block *blocks[BPC];
	void *hptr;
}cblock_set;

typedef struct seq_bm_private{
	__block *seq_block;
	block_set *logical_segment;
	queue *free_logical_segment_q;
	mh *max_heap;
	uint32_t assigned_block;
	uint32_t free_block;
	
	//my structures
	cblock_set *logical_chip;
	queue *free_logical_chip_q;
	mh *max_heap_chip;
	uint32_t assigned_block_per_chip;
	int32_t free_block_per_chip;
}sbm_pri;

uint32_t seq_create (struct blockmanager*, lower_info *li);
uint32_t seq_destroy (struct blockmanager*);
__block* seq_get_block (struct blockmanager*, __segment *);
__block *seq_pick_block(struct blockmanager *, uint32_t page_num);
__segment* seq_get_segment (struct blockmanager*, bool isreserve);
bool seq_check_full(struct blockmanager *,__segment *active, uint8_t type);
bool seq_is_gc_needed (struct blockmanager*);
__gsegment* seq_get_gc_target (struct blockmanager*);
void seq_trim_segment (struct blockmanager*, __gsegment*, struct lower_info*);
int seq_populate_bit (struct blockmanager*, uint32_t ppa);
int seq_unpopulate_bit (struct blockmanager*, uint32_t ppa);
int seq_erase_bit (struct blockmanager*, uint32_t ppa);
bool seq_is_valid_page (struct blockmanager*, uint32_t ppa);
bool seq_is_invalid_page (struct blockmanager*, uint32_t ppa);
void seq_set_oob(struct blockmanager*, char *data, int len, uint32_t ppa);
char* seq_get_oob(struct blockmanager*, uint32_t ppa);
void seq_release_segment(struct blockmanager*, __segment *);
__segment* seq_change_reserve(struct blockmanager* ,__segment *reserve);
int seq_get_page_num(struct blockmanager* ,__segment *);
int seq_pick_page_num(struct blockmanager* ,__segment *);

uint32_t seq_map_ppa(struct blockmanager* , uint32_t lpa);
void seq_free_segment(struct blockmanager *, __segment *);

//my new functions.
__chip* seq_get_chip (struct blockmanager* bm, bool isreserve);
int seq_get_page_num_pinned(struct blockmanager* bm, __chip *c, int mark);

#endif
