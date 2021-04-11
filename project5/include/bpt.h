#ifndef __BPT_H__
#define __BPT_H__

#include "type.h"
#include "index_manager.h"
#include "buffer_manager.h"
#include "disk_manager.h"

/* debuging functions */

void print_page (int tid, pagenum_t pagenum);
void print_header (int tid);

/* API services */

/* Allocate the buffer pool (array) with the given number of entries.
 * Initialize other fields (state info, LRU info..) with your own design.
 * If success, return 0. Otherwise, return non-zero value.
 */
int init_db (int buf_num);

/* Open existing data file using 'pathname' or create one if not existed.
 * If success, return the unique table id, which represents the own table in this database.
 * (Return negative value if error occurs)
 * You have to maintain a table id once open_talbe() is called, which is matching file descriptor
 * of file pointer depending on your previous implementation. (table id >= 1 and maximum
 * allocated id is set to 10)
 */
int open_table (char* pathname);

/* Insert input 'key/value' (record) to data file at the right place.
 * If success, return 0. Otherwise, return non-zero value.
 */
int db_insert (int tid, int64_t key, char* value);


/* Find the record containing input 'key'.
 * If found matching 'key', store matched 'value' string int ret_val and return 0.
 * Otherwise, return non-zero value.
 * Memory allocation for record structure(ret_val) should occer in caller function.
 */
int db_find (int tid, int64_t key, char* ret_val);

/* Find the matching record and delete it if found.
 * If success, return 0. Otherwise, return non-zero value.
 */
int db_delete (int tid, int64_t key);

// Your existing APIs (insert, delete, find) must work with implemented buffer
// manager first before accessing to disk.

/* If the page is not in buffer pool (cache-miss), read page from disk and maintain this page
 * in buffer block.
 * Page modification only occurs in-memory buffer. If the page frame in buffer is updated,
 * mark the buffer block as dirty.
 * According to LRU policy, least recently used buffer is the victim for page eviction.
 * Writing page to disk occurs during LRU page eviction.
 */

/* Write all pages of this table from buffer to disk and discard the table id.
 * If success, return 0. Otherwise, return non-zero value.
 */
int close_table (int table_id);

/* Flush all data from buffer and destroy allocated buffer.
 * If success, return 0. Otherwise, return non-zero value.
 */
int shutdown_db (void);

/* Do natural join with given two tables and write result table to the file using given pathname.
 * Return 0 if success, otherwise return non-zero value.
 * Two tables should have been opened earlier.
 */
int join_table (int table_id_1, int table_id_2, char * pathname);

#endif /* __BPT_H__*/
