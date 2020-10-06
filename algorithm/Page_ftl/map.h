#include "../../include/container.h"
#include "../../interface/interface.h"

typedef struct page_map_body{
	uint32_t *mapping;
	bool isfull;
	uint32_t assign_page;

	/*segment is a kind of Physical Block*/
	__segment *reserve; //for gc
	__segment *active; //for gc
	__chip *chip_reserves;
	__chip *chip_actives;
	__chip *chip_actives_arr[_NOC];//an array for currently used chip.
}pm_body;


void page_map_create();
uint32_t page_map_assign(KEYT *lba);
uint32_t page_map_assign_pinned(KEYT *lba, int mark, int chip_num, int* chip_idx);
uint32_t page_map_pick(uint32_t lba);
uint32_t page_map_trim(uint32_t lba);
uint32_t page_map_gc_update(KEYT* lba, uint32_t idx);
uint32_t page_map_gc_update_chip(KEYT *lba, uint32_t idx, int chip_num);
void page_map_free();
