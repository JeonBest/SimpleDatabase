#ifndef __DISK_MANAGER_H__
#define __DISK_MANAGER_H__

#include "type.h"

/* File manager API */

// Allocate an on-disk page from the free page list
pagenum_t file_alloc_page (int tid);

// Free an on-disk page from the free page list
void file_free_page (int tid, pagenum_t pagenum);

// Read an on-disk page into the in-memory page structure(dest)
void file_read_page (int tid, pagenum_t pagenum, page_t* dest);

// Write an in-memory page(src) to the on-disk page
void file_write_page (int tid, pagenum_t pagenum, const page_t* src);

/* header page handler functions */

pagenum_t get_freepgnum (int tid);
pagenum_t get_rootpgnum (int tid);
pagenum_t get_numofpages (int tid);
void set_freepgnum (int tid, pagenum_t src);
void set_rootpgnum (int tid, pagenum_t src);
void set_numofpages (int tid, pagenum_t src);

#endif /* __DISK_MANAGER_H__*/
