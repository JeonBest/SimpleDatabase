#ifndef __TYPE_H__
#define __TYPE_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>


#define PAGE_SIZE_ 4096
#define FREEPAGE_GROW_RATE 25000
#define HEADER_PAGENUM 0
#define MAX_LEAF_KEY 31
#define MAX_INTERNAL_KEY 248
#define VALUE_SIZE 120

#define MAX_KEY_VALUE 9223372036854775807
typedef uint64_t pagenum_t;

// macros

#define PAGE_ALLOC() (page_t*)malloc(sizeof(page_t))
#define FRAME_ALLOC() (frame_t*)malloc(sizeof(frame_t))

/* In-memory structure */

/* Internal record has key and child page number.
 * child page number points it's child page number.
 */
struct internal_record_t {
	int64_t key;
	pagenum_t child_pgnum;
}__attribute__((aligned(16), packed));
typedef struct internal_record_t internal_record_t;

/* Leaf record has key and it's value.
 */
struct leaf_record_t {
	int64_t key;
	char value[VALUE_SIZE];
}__attribute__((aligned(128), packed));
typedef struct leaf_record_t leaf_record_t;

/* Page header contains page number, is leaf, number of keys and
 * right sibling page number. right sibling page number is saved
 * in front of the first key. If page is leaf, it points next leaf
 * If page is internal, it points the most left child page number
 * If the leaf page is right most leaf page, number will be 0.
 */
struct page_header {
	pagenum_t parent_pgnum;
	int is_leaf;
	int numofkeys;
	char reserved[104];
	union {
		pagenum_t next_pgnum;
		pagenum_t left_pgnum;
	}; // if internal page : most left childpage number
}__attribute__((aligned(128), packed));
typedef struct page_header page_header;

/* meta_data structure is made for header page
 */
struct meta_data {
	pagenum_t freepgnum;
	pagenum_t rootpgnum;
	pagenum_t numofpages;
	char reserved[104];
}__attribute__((aligned(128), packed));
typedef struct meta_data meta_data;

/* Page type can hold both leaf page and internal page.
 * If the page is interal its record would be internal_record
 * Otherwise, record would be leaf_record.
 */
struct page_t {
	union {
		page_header p_header;
		meta_data m_data;
	};
	union {
		internal_record_t internal_record[MAX_INTERNAL_KEY];
		leaf_record_t leaf_record[MAX_LEAF_KEY];
	};
}__attribute__((aligned(PAGE_SIZE_), packed));
typedef struct page_t page_t;

struct frame_t {
	int table_id;
	pagenum_t page_num;
	bool is_dirty;
	bool is_pinned;
	bool ref_bit;
	page_t payload;
};
typedef struct frame_t frame_t;

#endif /* __TYPE_H__*/
