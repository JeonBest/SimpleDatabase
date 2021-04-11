#include "disk_manager.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// direct disk managing functions.

pagenum_t file_alloc_page (int tid) {
	pagenum_t freepgnum, nextnum, lastpgnum, i;
	
	freepgnum = get_freepgnum (tid);

	/* Case: free page exist.
	 * update header page's first free page number
	 * to second free page number
	 */

	if (freepgnum != 0) {
		pread (tid, &nextnum, sizeof(pagenum_t), PAGE_SIZE_ * freepgnum);
		set_freepgnum (tid, nextnum);
		return freepgnum;
	}

	/* Case: free page don't exist.
	 * grow file and return the very first free page number.
	 * update header page free page number.
	 */

	else {
		lastpgnum = get_numofpages (tid);
		
		for (i = lastpgnum + 1; i < lastpgnum + FREEPAGE_GROW_RATE; i++) {
			nextnum = i + 1;
			pwrite (tid, &nextnum, sizeof(pagenum_t), PAGE_SIZE_ * i);
		}
		nextnum = 0;
		pwrite (tid, &nextnum, sizeof(pagenum_t), PAGE_SIZE_ * (i - 1));
		char _end = '0'; //guarantee last freepage size.
		pwrite (tid, &_end, sizeof(char), PAGE_SIZE_ * i - sizeof(char));
		
		set_freepgnum (tid, lastpgnum + 1);
		set_numofpages (tid, lastpgnum + FREEPAGE_GROW_RATE);

		return lastpgnum;
	}
}

void file_free_page (int tid, pagenum_t pgnum) {
	pagenum_t freepgnum;

	freepgnum = get_freepgnum (tid);

	set_freepgnum (tid, pgnum);
	
	pwrite (tid, &freepgnum, sizeof(pagenum_t), PAGE_SIZE_ * pgnum);
}

void file_read_page (int tid, pagenum_t pgnum, page_t* dest) {
	if (pread (tid, dest, PAGE_SIZE_, PAGE_SIZE_ * pgnum) == -1) {
		perror("page read failed.");
	}
}

void file_write_page (int tid, pagenum_t pgnum, const page_t* src) {
	if (pwrite (tid, src, PAGE_SIZE_, PAGE_SIZE_ * pgnum) == -1) {
		perror("page write failed.");
	}
}

// header info managing functions.

pagenum_t get_freepgnum (int tid) {
	page_t* header;
	pagenum_t freepgnum;

	header = PAGE_ALLOC();
	file_read_page (tid, HEADER_PAGENUM, header);
	freepgnum = header->m_data.freepgnum;
	
	free (header);
	return freepgnum;
}

pagenum_t get_rootpgnum (int tid) {
	page_t* header;
	pagenum_t rootpgnum;

	header = PAGE_ALLOC();
	file_read_page (tid, HEADER_PAGENUM, header);
	rootpgnum = header->m_data.rootpgnum;
	
	free (header);
	return rootpgnum;
}

pagenum_t get_numofpages (int tid) {
	page_t* header;
	pagenum_t numofpages;

	header = PAGE_ALLOC();
	file_read_page (tid, HEADER_PAGENUM, header);
	numofpages = header->m_data.numofpages;
	
	free (header);
	return numofpages;
}

void set_freepgnum (int tid, pagenum_t src) {
	page_t* header;
	header = PAGE_ALLOC();

	file_read_page (tid, HEADER_PAGENUM, header);
	header->m_data.freepgnum = src;
	
	file_write_page (tid, HEADER_PAGENUM, header);
	free (header);
}

void set_rootpgnum (int tid, pagenum_t src) {
	page_t* header;
	header = PAGE_ALLOC();
	
	file_read_page (tid, HEADER_PAGENUM, header);
	header->m_data.rootpgnum = src;
	
	file_write_page (tid, HEADER_PAGENUM, header);
	free (header);
}

void set_numofpages (int tid, pagenum_t src) {
	page_t* header;
	header = PAGE_ALLOC();
	
	file_read_page (tid, HEADER_PAGENUM, header);
	header->m_data.numofpages = src;
	
	file_write_page (tid, HEADER_PAGENUM, header);
	free (header);
}

