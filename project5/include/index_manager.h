#ifndef __INDEX_MANAGER_H__
#define __INDEX_MANAGER_H__

#include "type.h"

// Finding

pagenum_t find_leaf_page (int tid, int64_t key);
char * find_value (page_t* page, int64_t key);

// Insertion.

page_t * make_page (void);
page_t * make_header_page (void);
page_t * make_leaf_page (void);
page_t * make_internal_page (void);
page_t * start_new_tree (int64_t key, char* value);
page_t * insert_into_leaf (page_t* page, int64_t key, char* value);
page_t * insert_into_node (page_t* page, int right_index, int64_t key, pagenum_t right);
int get_right_index (page_t* parent, pagenum_t right);
int insert_into_leaf_after_splitting (int tid, pagenum_t pagenum, int64_t key, char* value);
int insert_into_parent (int tid, pagenum_t pgnum, pagenum_t left, int64_t key, pagenum_t right);
int insert_into_new_root (int tid, pagenum_t left, int64_t key, pagenum_t right);
int insert_into_node_after_splitting (int tid, pagenum_t oldpgnum, int right_index, int64_t key, pagenum_t right);

// Deletion.
int delete_entry (int tid, pagenum_t leafnum, int64_t key);
page_t * remove_entry_from_page (page_t* page, int64_t key);
int adjust_root (int tid, pagenum_t rootnum);
int coalesce_pages (int tid, pagenum_t pgnum, pagenum_t neighbor_num, int neighbor_index);
int redistribute_pages (int tid, pagenum_t pgnum, pagenum_t neighbor_num, int neighbor_index);
int get_neighbor_index (page_t* parent, pagenum_t pagenum);

// Join.
int64_t find_least_key (int tid);
int64_t get_next_record (int tid, int64_t key);
int get_table_value (int tid, int64_t key, char* ret_val);

#endif /* __INDEX_MANAGER_H__*/
