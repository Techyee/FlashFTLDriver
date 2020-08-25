#ifndef __MY_CACHE_H__
#define __MY_CACHE_H__
#include <stdint.h>

typedef enum {
	COARSE, FINE,
} CACHE_TYPE;

typedef struct my_cache{
	uint32_t (*init)(struct my_cache *, uint32_t total_caching_physical_pages);
	uint32_t (*free)(struct my_cache *);
	bool (*is_needed_eviction)(struct my_cache *);
	uint32_t (*update_entry)(struct my_cache *, struct GTD_entry *, uint32_t lba, uint32_t ppa);
	uint32_t (*update_entry_gc)(struct my_cache *, struct GTD_entry *, uint32_t lba, uint32_t ppa);
	uint32_t (*insert_entry_from_translation)(struct my_cache *, GTD_entry *, uint32_t lba, char *data);
	uint32_t (*update_from_translation_gc)(struct my_cache *, char *data, uint32_t lba, uint32_t ppa);
	uint32_t (*get_mapping)(struct my_cache *, uint32_t lba);
	struct GTD_entry *(*get_eviction_GTD_entry)(struct my_cache *);//if return value is NULL, it is clean eviction.
	struct mapping_entry *(*get_eviction_mapping_entry)(struct my_cache *);//if return value is NULL, it is clean eviction.
	bool (*update_eviction_target_translation)(struct my_cache* , GTD_entry *etr, char *data);

	bool (*exist)(struct my_cache *, uint32_t lba);
	CACHE_TYPE type;
	void *private_data;
}my_cache;

#endif