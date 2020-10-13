#ifndef VECTORED_INTERFACE
#define VECTORED_INTERFACE
#include "interface.h"
#include "../include/settings.h"

uint32_t inf_vector_make_req(char *buf, void * (*end_req)(void*), uint32_t mark, uint32_t deadline, uint32_t gc_deadline, int chip_num, int* chip_idx);
void *vectored_main(void *);
void *inf_main(void* arg);
void assign_vectored_req(vec_request *txn);
void release_each_req(request *req);
#endif
