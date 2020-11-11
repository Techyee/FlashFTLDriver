#ifndef POSIX_HEADER
#define POSIX_HEADER
#include "../../include/container.h"
#include "../../include/utils/cond_lock.h"
#define FS_LOWER_W 1
#define FS_LOWER_R 2
#define FS_LOWER_T 3
#define FS_LOWER_C 4
#define FS_LOWER_MISS 5

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
void *pipe_main(void *arg);
void* posix_make_copyback(uint32_t ppa, uint32_t ppa2, uint32_t size, bool async,algo_req * const req);
void* posix_copyback(uint32_t ppa, uint32_t ppa2, uint32_t size, bool async,algo_req * const req);
void* posix_make_req_trim(uint32_t ppa, bool async, uint32_t gc_deadline, int bench_idx);

typedef struct posix_request {
	void * hptr;
	uint32_t deadline;
	uint32_t trim_mark;
	uint8_t bench_idx;
	struct timeval algo_init_t;
	struct timeval l_init_t;
	struct timeval dev_init_t;
	struct timeval dev_end_t;
	uint32_t dev_req_t; //record how much user req blocked.
	uint32_t dev_gc_t; //record how much gc blocked.
	uint32_t dev_intGC_t; //record how much interference is occured(from other request)
	uint32_t dev_intIO_t;
	FSTYPE type;
	FSTYPE old_type;
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
	pthread_mutex_t chip_heap_lock;
	pthread_mutex_t chip_busy;
	cl_lock* latency;
	int mark;
}chip_info;
#endif
