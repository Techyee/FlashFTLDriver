#ifndef POSIX_HEADER
#define POSIX_HEADER
#include "../../include/container.h"
#include "../../include/utils/cond_lock.h"
#define FS_LOWER_W 1
#define FS_LOWER_R 2
#define FS_LOWER_T 3
#define FS_LOWER_C 4

uint32_t posix_create(lower_info*,blockmanager *);
void *posix_destroy(lower_info*);
void* posix_push_data(uint32_t ppa, uint32_t size, value_set *value,bool async, algo_req * const req);
void* posix_pull_data(uint32_t ppa, uint32_t size, value_set* value,bool async,algo_req * const req);
void* posix_make_push(uint32_t ppa, uint32_t size, value_set *value,bool async, algo_req * const req);
void* posix_make_pull(uint32_t ppa, uint32_t size, value_set *value,bool async, algo_req * const req);

void* posix_badblock_checker(uint32_t ppa, uint32_t size, void*(*process)(uint64_t,uint8_t));
void* posix_trim_block(uint32_t ppa, bool async);
void* posix_trim_a_block(uint32_t ppa, bool async);
void *posix_make_trim(uint32_t ppa, bool async);
void *posix_refresh(lower_info*);
void posix_stop();
void posix_flying_req_wait();
void* posix_read_hw(uint32_t ppa, char *key,uint32_t key_len, value_set *value,bool async,algo_req * const req);
uint32_t posix_hw_do_merge(uint32_t lp_num, ppa_t *lp_array, uint32_t hp_num,ppa_t *hp_array,ppa_t *tp_array, uint32_t* ktable_num, uint32_t *invliadate_num);
char * posix_hw_get_kt();
char *posix_hw_get_inv();
uint32_t convert_ppa(uint32_t);

//my functions
void *new_latency_main(void *__input);
void* posix_make_copyback(uint32_t ppa, uint32_t ppa2, uint32_t size, bool async);
void* posix_copyback(uint32_t ppa, uint32_t ppa2, uint32_t size, bool async);

typedef struct posix_request {
	void * hptr;
	uint32_t deadline;
	uint32_t trim_mark;
	struct timeval algo_init_t;
	struct timeval l_init_t;
	struct timeval dev_init_t;
	struct timeval dev_end_t;
	FSTYPE type;
	uint32_t key;
	uint32_t key2;
	value_set *value;
	algo_req *upper_req;
	bool isAsync;
	uint32_t size;
}posix_request;

typedef struct mem_seg {
	PTR storage;
} mem_seg;

typedef struct _chip_info{
	pthread_t chip_pid;
	cl_lock* latency;
	int mark;
}chip_info;
#endif
