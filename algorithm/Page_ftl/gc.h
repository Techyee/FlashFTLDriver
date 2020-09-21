#include "../../include/container.h"
#include "../../interface/interface.h"

typedef struct align_gc_buffer{
	uint8_t idx;
	char value[PAGESIZE];
	KEYT key[L2PGAP];
}align_gc_buffer;

typedef struct gc_value{
	uint32_t ppa;
	value_set *value;
	bool isdone;
}gc_value;

void invalidate_ppa(uint32_t t_ppa);
void validate_ppa(uint32_t t_ppa, KEYT *lbas);
ppa_t get_ppa(KEYT* lba);
ppa_t get_ppa_pinned(KEYT* lba, int mark);
void do_gc();
void chip_gc(int mark);
void *page_gc_end_req(algo_req *input);
