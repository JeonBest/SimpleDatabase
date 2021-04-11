#include "bpt.h"

void usage_1 (void) {
	printf ("Enter any of the following commands after the prompt > :\n"
	"\to <p>         -- open file which is in pathname <p>.\n"
	"\ti <t> <k> <v> -- Insert <k> (an 64-bit integer) as key and <v> as value.\n"
	"\tf <t> <k>     -- Find the value under key <k> and print value.\n"
	"\td <t> <k>     -- Delete key <k> and its associated value.\n"
	"\tp <t> <p>     -- Shows page which has page number <p>.\n"
	"\tq             -- Quit. (Or use Ctrl-D.)\n"
	"\t?             -- Print this help message.\n");
}

/* debuging functions */

void print_page (int tid, pagenum_t pgnum) {
	page_t* page;
	page = PAGE_ALLOC();
	file_read_page (tid, pgnum, page);
	int i;
	printf ("parent number : %lu\n", page->p_header.parent_pgnum);
	printf ("number of keys : %d\n", page->p_header.numofkeys);
	if (page->p_header.is_leaf == 1) {
		printf ("next leaf page number : %lu\n", page->p_header.next_pgnum);
		for (i = 0; i < page->p_header.numofkeys; i++) {
			printf("%d | key : %ld , value : %s\n", i, page->leaf_record[i].key, page->leaf_record[i].value);
		}
	} else {
		printf ("most left child page number : %lu\n", page->p_header.left_pgnum);
		for (i = 0; i < page->p_header.numofkeys; i++) {
			printf ("%d | key : %ld , child : %lu\n", i, page->internal_record[i].key, page->internal_record[i].child_pgnum);
		}
	}
	free (page);
}

void print_header (int tid) {
	page_t* header;
	header = PAGE_ALLOC();
	file_read_page (tid, HEADER_PAGENUM, header);
	printf ("free page number: %lu\nroot page number: %lu\nnumber of pages: %lu\n",
			header->m_data.freepgnum, header->m_data.rootpgnum, header->m_data.numofpages);
	free (header);
}

/* API services */

int open_table (char* pathname) {
	int fd;
	page_t* header;

	// If file already exist, just open.
	if (access (pathname, F_OK) == 0) {
		fd = open (pathname, O_RDWR);
		if (fd == 0) {
			perror("file open failed");
		}
	}

	// If file doesn't exist, create one.
	else {
		fd = open (pathname, O_RDWR | O_CREAT, S_IRWXU);
		if (fd == 0) {
			perror("file open failed");
		}
		// make header page, and 25000 free pages)
		header = make_header_page();
		file_write_page (fd, HEADER_PAGENUM, header);
	}

	// return unique_id of table file.
	return fd;
}

int db_insert (int tid, int64_t key, char* value) {
	page_t* target_pg;
	pagenum_t rootpgnum, target_pgnum;

	rootpgnum = get_rootpgnum (tid);

	/* Case1: root not exist.
	 * make new db.
	 */

	if (rootpgnum == 0) {
		target_pg = start_new_tree (key, value);
		target_pgnum = file_alloc_page(tid);

		set_rootpgnum (tid, target_pgnum);
		file_write_page (tid, target_pgnum, target_pg);

		free (target_pg);
		printf ("new db made.\n");
		return 0;
	}

	// find leaf page where key should be.

	target_pgnum = find_leaf_page (tid, key);
	target_pg = PAGE_ALLOC();
	file_read_page (tid, target_pgnum, target_pg);

	/* Case2: key already in db.
	 * The current implementation ignores duplicates.
	 */
	
	if (find_value (target_pg, key) != NULL) {
		printf("key already in db.\n");
		free (target_pg);
		return 0;
	}

	/* Case3: leaf has room for key. */

	if (target_pg->p_header.numofkeys < MAX_LEAF_KEY) {
		target_pg = insert_into_leaf (target_pg, key, value);
		file_write_page (tid, target_pgnum, target_pg);
		free (target_pg);
		return 0;
	}

	/* Case4: leaf must split.
	 * heavy operation.
	 */
	
	free (target_pg);
	insert_into_leaf_after_splitting (tid, target_pgnum, key, value);
	return 0;
}

int db_find (int tid, int64_t key, char* ret_val) {
	page_t* target_pg;
	pagenum_t rootpgnum, target_pgnum;

	rootpgnum = get_rootpgnum(tid);

	target_pgnum = find_leaf_page (tid, key);
	target_pg = PAGE_ALLOC();
	file_read_page (tid, target_pgnum, target_pg);

	if (find_value (target_pg, key) == NULL){
		free (target_pg);
		return -1;
	} else {
		strcpy (ret_val, find_value (target_pg, key));
		free (target_pg);
		return 0;
	}
}

int db_delete (int tid, int64_t key) {
	page_t* target_pg;
	pagenum_t rootpgnum, target_pgnum;

	rootpgnum = get_rootpgnum (tid);
	
	target_pgnum = find_leaf_page (tid, key);
	target_pg = PAGE_ALLOC();
	file_read_page (tid, target_pgnum, target_pg);

	// if key not exist
	if (find_value (target_pg, key) == NULL) {
		free (target_pg);
		return -1;
	} else {
		free (target_pg);
		delete_entry (tid, target_pgnum, key);
		return 0;
	}
}


int join_table (int table_id_1, int table_id_2, char * pathname) {
	printf ("join_table called.\n");

	int64_t key_1, key_2;
	FILE* fp;
	char value_1[120], value_2[120];

	key_1 = find_least_key (table_id_1);
	key_2 = find_least_key (table_id_2);

	fp = fopen (pathname, "w");
	printf ("pathname file created\n");

	while (1) {
		if (key_1 == MAX_KEY_VALUE && key_2 == MAX_KEY_VALUE) {
			if (get_table_value (table_id_1, key_1, value_1)) {
				perror ("No value1 found: Join");
				exit(EXIT_FAILURE);
			}
			if (get_table_value (table_id_2, key_1, value_2)) {
				perror ("No value2 found: Join");
				exit(EXIT_FAILURE);
			}
			printf ("111\n");
			fprintf (fp, "%ld,%s,%ld,%s\n", key_1, value_1,	key_2, value_2);
			printf ("222\n");
		}
		printf ("333\t%ld %ld\n",key_1,key_2);
		while (key_1 < key_2) {
			key_1 = get_next_record (table_id_1, key_1);
		}
		while (key_1 > key_2) {
			key_2 = get_next_record (table_id_2, key_2);
		}
		// assert r == s
		// Done when key_1 or key_2 gets to the last key.
		if (key_1 == MAX_KEY_VALUE || key_2 == MAX_KEY_VALUE)
			break;

		printf ("444\t%ld %ld\n",key_1,key_2);
		if (get_table_value (table_id_1, key_1, value_1)) {
			perror ("No value1 found: Join");
			exit(EXIT_FAILURE);
		}
		if (get_table_value (table_id_2, key_1, value_2)) {
			perror ("No value2 found: Join");
			exit(EXIT_FAILURE);
		}
		printf ("555\n");
		fprintf (fp, "%ld,%s,%ld,%s\n", key_1, value_1,	key_2, value_2);
		printf ("666\n");
		key_2 = get_next_record (table_id_2, key_2);
		printf ("777\n");
	}
	fclose (fp);

	return 0;
}

