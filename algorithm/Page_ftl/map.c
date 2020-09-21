#include "map.h"
#include "gc.h"
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
extern algorithm page_ftl;

void page_map_create(){
	pm_body *p=(pm_body*)calloc(sizeof(pm_body),1);

	//initialize l-to-p mapping with unit32_t values.
	p->mapping=(uint32_t*)malloc(sizeof(uint32_t)*_NOP*L2PGAP);
	for(int i=0;i<_NOP*L2PGAP; i++){
		p->mapping[i]=UINT_MAX;
	}
	//!finished creation.
 
	//alloc ptr to target segments.
	p->reserve=page_ftl.bm->get_segment(page_ftl.bm,true); //reserve for GC
	p->active=page_ftl.bm->get_segment(page_ftl.bm,false); //now active block for inserted request.
	//!alloc
	
	//p->chip_actives=page_ftl.bm->get_chip(page_ftl.bm,false);
	for(int i=0;i<_NOC;i++){
		p->chip_actives_arr[i] = page_ftl.bm->get_chip(page_ftl.bm,false);
	}
		
	page_ftl.algo_body=(void*)p; //you can assign your data structure in algorithm structure
}

uint32_t page_map_assign(KEYT* lba){
	uint32_t res=0;

	res=get_ppa(lba);
	pm_body *p=(pm_body*)page_ftl.algo_body;
	for(uint32_t i=0; i<L2PGAP; i++){
		KEYT t_lba=lba[i];
		if(p->mapping[t_lba]!=UINT_MAX){
			/*when mapping was updated, the old one is checked as a inavlid*/
			invalidate_ppa(p->mapping[t_lba]);
		}
		/*mapping update*/
		p->mapping[t_lba]=res*L2PGAP+i;
		DPRINTF("\tmap set : %u->%u\n", t_lba, p->mapping[t_lba]);
	}

	return res;
}

uint32_t page_map_assign_pinned(KEYT* lba, int mark){
	//pinned version for page_map_assign(nearly same)
	uint32_t res = 0;
	//use get_ppa_pinned to differentiate page allocation.
	res = get_ppa_pinned(lba, mark);
	pm_body *p = (pm_body*)page_ftl.algo_body;
	for(uint32_t i=0; i<L2PGAP; i++){
		KEYT t_lba=lba[i];
		if(p->mapping[t_lba]!=UINT_MAX){
			/*when mapping was updated, the old one is checked as a inavlid*/
			invalidate_ppa(p->mapping[t_lba]);
		}
		/*mapping update*/
		p->mapping[t_lba]=res*L2PGAP+i;
		validate_ppa(res, lba);
		DPRINTF("\tmap set : %u->%u\n", t_lba, p->mapping[t_lba]);
	}
	return res;
}
uint32_t page_map_pick(uint32_t lba){
	uint32_t res=0;
	pm_body *p=(pm_body*)page_ftl.algo_body;
	res=p->mapping[lba];
	return res;
}


uint32_t page_map_trim(uint32_t lba){
	uint32_t res=0;
	pm_body *p=(pm_body*)page_ftl.algo_body;
	res=p->mapping[lba];
	if(res==UINT32_MAX){
		return 0;
	}
	else{
		invalidate_ppa(res);
		p->mapping[lba]=UINT32_MAX;
		return 1;
	}
}

uint32_t page_map_gc_update(KEYT *lba, uint32_t idx){
	uint32_t res=0;
	pm_body *p=(pm_body*)page_ftl.algo_body;

	/*when the gc phase, It should get a page from the reserved block*/
	res=page_ftl.bm->get_page_num(page_ftl.bm,p->reserve);
	uint32_t old_ppa, new_ppa;
	for(uint32_t i=0; i<idx; i++){
		KEYT t_lba=lba[i];
		if(p->mapping[t_lba]!=UINT_MAX){
			/*when mapping was updated, the old one is checked as a inavlid*/
		//	invalidate_ppa(p->mapping[t_lba]);
		}
		if(t_lba==1409711){
			old_ppa=p->mapping[t_lba];
		}
		/*mapping update*/
		p->mapping[t_lba]=res*L2PGAP+i;
		if(t_lba==1409711){
			new_ppa=p->mapping[t_lba];
			printf("%d change %d to %d\n", t_lba, old_ppa, new_ppa);
		}
	}

	return res;
}

uint32_t page_map_gc_update_chip(KEYT *lba, uint32_t idx, int chip_num){
	//while updating a page_map, pickup a reserved block
	//!!hardcoded for (PAGESIZE == LPAGESIZE) case!!
	uint32_t res = 0;
	KEYT t_lba = lba[0];
	pm_body *p = (pm_body*)page_ftl.algo_body;
	res = page_ftl.bm->get_page_num_pinned(page_ftl.bm,p->chip_actives_arr[chip_num],chip_num,true);
	p->mapping[t_lba] = res*L2PGAP;
	return res;

}

void page_map_free(){
	pm_body *p=(pm_body*)page_ftl.algo_body;
	free(p->mapping);
}


