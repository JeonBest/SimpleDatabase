#include "index_manager.h"
#include "disk_manager.h"

// Finding functions

pagenum_t find_leaf_page (int tid, int64_t key) {
	int i;
	pagenum_t rootpgnum, target_pgnum;
	page_t* target_pg;

	rootpgnum = get_rootpgnum (tid);

	// Case1: root not exist.
	if (rootpgnum == 0) {
		return rootpgnum;
	}

	target_pgnum = rootpgnum;
	target_pg = PAGE_ALLOC();
	file_read_page (tid, target_pgnum, target_pg);

	// internal page case :
	while (target_pg->p_header.is_leaf == 0) {
		// if key is in the most left child page
		if (key < target_pg->internal_record[0].key) {
			target_pgnum = target_pg->p_header.left_pgnum;
		} else {
			i = 1;
			while (i < target_pg->p_header.numofkeys) {
				if (key >= target_pg->internal_record[i].key) i++;
				else break;
			}
			target_pgnum = target_pg->internal_record[i - 1].child_pgnum;
		}
		//after get child page number, go to child page
		file_read_page (tid, target_pgnum, target_pg);
	}
	free (target_pg);
	return target_pgnum;
}

char* find_value (page_t* page, int64_t key) {
	int i;
	// sequencially search for key
	for (i = 0; i < page->p_header.numofkeys; i++) {
		if (page->leaf_record[i].key == key) break;
	}
	if (i == page->p_header.numofkeys) {
		return NULL;
	} else {
		return page->leaf_record[i].value;
	}
}

// Insertion functions

page_t* make_page (void) {
	page_t* new_page;
	new_page = PAGE_ALLOC();
	if (new_page == NULL) {
		perror ("Page creation.");
		exit (EXIT_FAILURE);
	}
	return new_page;
}

page_t* make_header_page (void) {
	page_t* header = make_page();
	header->m_data.freepgnum = 0;
	header->m_data.rootpgnum = 0;
	header->m_data.numofpages = 1;
	return header;
}

page_t* make_leaf_page (void) {
	page_t* leaf = make_page();
	leaf->p_header.numofkeys = 0;
	leaf->p_header.is_leaf = 1;
	leaf->p_header.parent_pgnum = 0;
	leaf->p_header.next_pgnum = 0;
	return leaf;
}

page_t* make_internal_page (void) {
	page_t* internal = make_page();
	internal->p_header.numofkeys = 0;
	internal->p_header.is_leaf = 0;
	internal->p_header.parent_pgnum = 0;
	internal->p_header.left_pgnum = 0;
	return internal;
}

page_t* start_new_tree (int64_t key, char* value) {
	page_t* root = make_leaf_page();
	root->leaf_record[0].key = key;
	strcpy (root->leaf_record[0].value, value);
	root->p_header.parent_pgnum = 0;
	root->p_header.numofkeys = 1;
	return root;
}

page_t* insert_into_leaf (page_t* page, int64_t key, char* value) {
	int i, insertion_point;
	insertion_point = 0;

	while (insertion_point < page->p_header.numofkeys &&
			page->leaf_record[insertion_point].key < key) {
		insertion_point++;
	}
	for (i = page->p_header.numofkeys; i > insertion_point; i--) {
		page->leaf_record[i] = page->leaf_record[i - 1];
	}
	page->leaf_record[insertion_point].key = key;
	strcpy (page->leaf_record[insertion_point].value, value);
	page->p_header.numofkeys++;
	return page;
}

page_t* insert_into_node (page_t* page, int right_index, int64_t key, pagenum_t right) {
	int i;

	for (i = page->p_header.numofkeys; i > right_index; i--) {
		page->internal_record[i] = page->internal_record[i - 1];
	}
	page->internal_record[right_index].child_pgnum = right;
	page->internal_record[right_index].key = key;
	page->p_header.numofkeys++;
	return page;
}

int get_right_index (page_t* parent, pagenum_t left) {
	int left_index = 0;
	while (left_index < parent->p_header.numofkeys &&
			parent->internal_record[left_index].child_pgnum != left)
		left_index++;
	left_index++;
	if (left_index > parent->p_header.numofkeys)
		return 0;
	else
		return left_index;
}

int insert_into_leaf_after_splitting (int tid, pagenum_t pgnum, int64_t key, char* value) {
	page_t* page, * new_page;
	pagenum_t new_pgnum, parent_pgnum;
	leaf_record_t* temp_record;
	int insertion_index, split, new_key, i, j;

	page = PAGE_ALLOC();
	file_read_page (tid, pgnum, page);
	new_pgnum = file_alloc_page (tid);
	new_page = make_leaf_page();

	temp_record = (leaf_record_t*)calloc(MAX_LEAF_KEY + 1, sizeof(leaf_record_t));
	if (temp_record == NULL) {
		perror("temporary records array.");
		exit(EXIT_FAILURE);
	}

	insertion_index = 0;
	while (insertion_index < MAX_LEAF_KEY &&
			page->leaf_record[insertion_index].key < key)
		insertion_index++;
	
	for (i = 0, j = 0; i < page->p_header.numofkeys; i++, j++) {
		if (j == insertion_index) j++;
		temp_record[j] = page->leaf_record[i];
	}

	temp_record[insertion_index].key = key;
	strcpy (temp_record[insertion_index].value, value);

	page->p_header.numofkeys = 0;

	split = MAX_LEAF_KEY / 2;

	for (i = 0; i< split; i++) {
		page->leaf_record[i] = temp_record[i];
		page->p_header.numofkeys++;
	}
	for (i = split, j = 0; i < MAX_LEAF_KEY + 1; i++, j++) {
		new_page->leaf_record[j] = temp_record[i];
		new_page->p_header.numofkeys++;
	}

	free (temp_record);

	// leaf page link update
	new_page->p_header.next_pgnum = page->p_header.next_pgnum;
	page->p_header.next_pgnum = new_pgnum;

	parent_pgnum = page->p_header.parent_pgnum;
	new_page->p_header.parent_pgnum = parent_pgnum;
	new_key = new_page->leaf_record[0].key;
	
	file_write_page (tid, pgnum, page);
	file_write_page (tid, new_pgnum, new_page);
	free (page);
	free (new_page);
	insert_into_parent (tid, parent_pgnum, pgnum, new_key, new_pgnum);
	return 0;
}

int insert_into_parent (int tid, pagenum_t pgnum,
		pagenum_t left, int64_t key, pagenum_t right) {
	
	page_t* page;
	int right_index;

	/* Case: new root. */

	if (pgnum == 0) {
		insert_into_new_root (tid, left, key, right);
		return 0;
	}

	/* Case: leaf or node. */

	// Find the parent's index between left, right page.

	page = PAGE_ALLOC();
	file_read_page (tid, pgnum, page);

	right_index = get_right_index (page, left);

	/* Simple case: the new key fits into the node. */

	if (page->p_header.numofkeys < MAX_INTERNAL_KEY) {
		insert_into_node (page, right_index, key, right);
		file_write_page (tid, pgnum, page);
		free (page);
		return 0;
	}

	/* Harder case: split a node. */

	free (page);
	insert_into_node_after_splitting (tid, pgnum, right_index, key, right);
	return 0;
}

int insert_into_new_root (int tid, pagenum_t left, int64_t key, pagenum_t right) {
	page_t* root, * child;
	root = make_internal_page();
	pagenum_t rootpgnum = file_alloc_page(tid);
	
	// set new root.
	root->internal_record[0].key = key;
	root->p_header.left_pgnum = left;
	root->internal_record[0].child_pgnum = right;
	root->p_header.numofkeys++;

	file_write_page (tid, rootpgnum, root);
	free (root);

	// update child's parent
	child = PAGE_ALLOC();
	file_read_page (tid, left, child);
	child->p_header.parent_pgnum = rootpgnum;
	file_write_page (tid, left, child);

	file_read_page (tid, right, child);
	child->p_header.parent_pgnum = rootpgnum;
	file_write_page (tid, right, child);

	free (child);
	
	// update header page's root page number
	set_rootpgnum (tid, rootpgnum);
	return 0;
}
	
int insert_into_node_after_splitting (int tid, pagenum_t oldpgnum,
		int right_index, int64_t key, pagenum_t right) {

	int i, j, split, k_prime;
	page_t* old_page, * new_page, * child;
	internal_record_t* temp_record;
	pagenum_t new_pgnum, parent_pgnum;

	/* First, create a temporary set of records to hold everything
	 * in order, including new key and child number, inserted in
	 * their correc palces.
	 * Then, create a new node and copy half of the records to
	 * the old node and the other half to the new.
	 */

	temp_record = calloc (MAX_INTERNAL_KEY + 1, sizeof(internal_record_t));
	if (temp_record == NULL) {
		perror ("Temporary record array for splitting nodes.");
		exit(EXIT_FAILURE);
	}
	old_page = PAGE_ALLOC();
	file_read_page (tid, oldpgnum, old_page);

	for (i = 0, j = 0; i < old_page->p_header.numofkeys; i++, j++) {
		if (j== right_index) j++;
		temp_record[j] = old_page->internal_record[i];
	}
	temp_record[right_index].key = key;
	temp_record[right_index].child_pgnum = right;

	/* Create the new node and copy half to old and new
	 * (left half : old, right half : new, split_point : parent)
	 * (in MAX ORDER 248 case, old: 124, new: 123)
	 */

	split = MAX_INTERNAL_KEY / 2;

	new_page = make_internal_page();
	old_page->p_header.numofkeys = 0;
	
	for (i = 0; i < split; i++) {
		old_page->internal_record[i] = temp_record[i];
		old_page->p_header.numofkeys++;
	}
	k_prime = temp_record[split].key;
	for (++i, j = 0; i < MAX_INTERNAL_KEY + 1; i++, j++) {
		new_page->internal_record[j] = temp_record[i];
		new_page->p_header.numofkeys++;
	}
	new_page->p_header.left_pgnum = temp_record[split].child_pgnum;
	free (temp_record);
	parent_pgnum = old_page->p_header.parent_pgnum;
	new_page->p_header.parent_pgnum = parent_pgnum;

	file_write_page (tid, oldpgnum, old_page);
	free (old_page);

	// update new node's child's parent page number.
	new_pgnum = file_alloc_page(tid);
	child = PAGE_ALLOC();
	for (i = 0; i < new_page->p_header.numofkeys; i++) {
		file_read_page (tid, new_page->internal_record[i].child_pgnum, child);
		child->p_header.parent_pgnum = new_pgnum;
		file_write_page (tid, new_page->internal_record[i].child_pgnum, child);
	}
	// also update most left child page.
	file_read_page (tid, new_page->p_header.left_pgnum, child);
	child->p_header.parent_pgnum = new_pgnum;
	file_write_page (tid, new_page->p_header.left_pgnum, child);
	free (child);

	// write new page
	file_write_page (tid, new_pgnum, new_page);
	free (new_page);
	
	/* Insert a new key into the parent of the two nodes resulting from
	 * the split, with the old page to the left and the new to the right.
	 */

	insert_into_parent (tid, parent_pgnum, oldpgnum, k_prime, new_pgnum);
	return 0;
}

// Deletion functions.

int delete_entry (int tid, pagenum_t pgnum, int64_t key) {

	int min_keys, neighbor_index, capacity;
	pagenum_t parent_num, neighbor_num;
	page_t* page, * parent, * neighbor;

	page = PAGE_ALLOC();
	file_read_page (tid, pgnum, page);

	// Remove key and pointer from node.

	page = remove_entry_from_page (page, key);

	/* Case: deletion from the root. */

	if (pgnum == get_rootpgnum(tid)) {
		file_write_page (tid, pgnum, page);
		free (page);
		adjust_root (tid, pgnum);
		return 0;
	}

	/* Determin minimum allowable size of node,
	 * to be preserved after deletion.
	 */

	/* Delayed Merge */

	min_keys = page->p_header.is_leaf == 1 ?
		MAX_LEAF_KEY / 8 : MAX_INTERNAL_KEY / 8;

	/* Case: node stays at or above minimum. */

	if (page->p_header.numofkeys > min_keys) {
		file_write_page (tid, pgnum, page);
		free (page);
		return 0;
	}

	/* Case: page key number falls below minimum.
	 * Either coalescence or redistribution is needed.
	 * In this case, leaf : under 3, internal : under 31
	 */
	
	/* Find the appropriate neighbor node with which
	 * to coalesce.
	 * Also find the key (k_prime) in the parent
	 * between node and neighbor.
	 */

	parent_num = page->p_header.parent_pgnum;
	parent = PAGE_ALLOC();
	file_read_page (tid, parent_num, parent);

	neighbor_index = get_neighbor_index (parent, pgnum);

	neighbor_num = neighbor_index == -1 ?
		parent->internal_record[parent->p_header.numofkeys - 2].child_pgnum :
		parent->internal_record[neighbor_index].child_pgnum;

	free (parent);

	/* When page and neighbor have less than capacity,
	 * they coalesce.
	 * when they have same or more than capacity,
	 * they redistribute.
	 */

	capacity = page->p_header.is_leaf == 1 ?
		MAX_LEAF_KEY / 2 : MAX_INTERNAL_KEY / 2;

	neighbor = PAGE_ALLOC();
	file_read_page (tid, neighbor_num, neighbor);

	/* Coalescence. */

	if (neighbor->p_header.numofkeys + page->p_header.numofkeys
			< capacity) {
		file_write_page (tid, pgnum, page);
		free (page);
		free (neighbor);
		coalesce_pages (tid, pgnum, neighbor_num, neighbor_index);

		return 0;
	}

	/* Redistribution. */

	else {
		file_write_page (tid, pgnum, page);
		free (page);
		free (neighbor);
		redistribute_pages (tid, pgnum, neighbor_num, neighbor_index);

		return 0;
	}
}

page_t* remove_entry_from_page (page_t* page, int64_t key) {	
	int i;

	/* Case: leaf page */

	if (page->p_header.is_leaf == 1) {

		// Remove the record and shift other records accordingly.
		i = 0;
		while (page->leaf_record[i].key != key)
			i++;
		for (++i; i < page->p_header.numofkeys; i++) {
			page->leaf_record[i - 1] = page->leaf_record[i];
		}
	}

	/* Case: internal page */
	else {

		// Remove the record and shift other records accordingly.
		i = 0;
		while (page->internal_record[i].key != key)
			i++;
		for (++i; i <page->p_header.numofkeys; i++) {
			page->internal_record[i - 1] = page->internal_record[i];
		}
	}
	// One key fewer.
	page->p_header.numofkeys--;

	return page;
}

int adjust_root (int tid, pagenum_t rootnum) {
	
	pagenum_t new_rootnum;
	page_t* root, * new_root;

	root = PAGE_ALLOC();
	file_read_page (tid, rootnum, root);

	/* Case: nonempty root. 
	 * Record have already been deleted,
	 * so nothing to be done.
	 */
	if (root->p_header.numofkeys > 0) {
		free (root);
		return 0;
	}

	/* Case: empty root. */

	/* If it has a child, promote the first child
	 * as the new root.
	 */

	if (root->p_header.is_leaf == 0) {
		new_rootnum = root->p_header.left_pgnum;
		free (root);
		set_rootpgnum (tid, new_rootnum);
		// new root's parent number is 0.
		new_root = PAGE_ALLOC();
		file_read_page (tid, new_rootnum, new_root);
		new_root->p_header.parent_pgnum = 0;
		file_write_page (tid, new_rootnum, new_root);
		free (new_root);
		// old root page becomes free page.
		file_free_page (tid, rootnum);
	}
	
	/* If it is a leaf (has no children),
	 * then the whole tree is empty.
	 */

	else {
		free (root);
		file_free_page (tid, rootnum);
		set_rootpgnum (tid, 0);
	}

	return 0;
}

int coalesce_pages (int tid, pagenum_t pgnum,
		pagenum_t neighbor_num, int neighbor_index) {

	pagenum_t parent_num;
	page_t* page, * neighbor, * parent, * child;
	int64_t k_prime;
	int balance_num, i, j;

	page = PAGE_ALLOC();
	file_read_page (tid, pgnum, page);

	neighbor = PAGE_ALLOC();
	file_read_page (tid, neighbor_num, neighbor);

	parent_num = page->p_header.parent_pgnum;
	parent = PAGE_ALLOC();
	file_read_page (tid, parent_num, parent);

	// number of keys that page will have after coalesce.	
	balance_num = page->p_header.is_leaf == 1 ?
		page->p_header.numofkeys + neighbor->p_header.numofkeys :
		page->p_header.numofkeys + neighbor->p_header.numofkeys + 1;
		

	// Case: page has a neighbor to the right.
	if (neighbor_index != -1) {
		
		// Case: leaf pages coalesce.
		if (page->p_header.is_leaf == 1) {
			
			/* Move neighbor's records to page.
			 */
			for (i = 0, j = page->p_header.numofkeys;
					i < neighbor->p_header.numofkeys; i++, j++) {
				page->leaf_record[j] = neighbor->leaf_record[i];
			}
			page->p_header.next_pgnum = neighbor->p_header.next_pgnum;
			
			page->p_header.numofkeys = balance_num;
		
		}

		// Case: internal pages coalesce.
		else {

			/* Parent's k_prime key and neighbor's most left child pointer
			 * becomes a record between page, neighbor's records.
			 */
			page->internal_record[page->p_header.numofkeys].key
				= parent->internal_record[neighbor_index].key;
			page->internal_record[page->p_header.numofkeys].child_pgnum
				= neighbor->p_header.left_pgnum;

			child = PAGE_ALLOC();

			// update most left child's parent number.
			file_read_page (tid, neighbor->p_header.left_pgnum, child);
			child->p_header.parent_pgnum = pgnum;
			file_write_page (tid, neighbor->p_header.left_pgnum, child);

			for (i = 0, j = page->p_header.numofkeys + 1;
					i < neighbor->p_header.numofkeys; i++,j++) {
				page->internal_record[j] = neighbor->internal_record[i];
				
				// update child's parent number.
				file_read_page (tid, neighbor->internal_record[i].child_pgnum, child);
				child->p_header.parent_pgnum = pgnum;
				file_write_page (tid, neighbor->internal_record[i].child_pgnum, child);
			}
			free (child);

			page->p_header.numofkeys = balance_num;
		}

		file_write_page (tid, pgnum, page);
		free (page);
		file_free_page (tid, neighbor_num);
		free (neighbor);

		/* Record between page pointer and neighbor pointer
		 * shoud be deleted recursively.
		 */
		k_prime = parent->internal_record[neighbor_index].key;
		free (parent);
		delete_entry (tid, parent_num, k_prime);
	}

	// Case: page has a neighbor to the left.
	else {
		
		/* Almost similar to right neighbor case,
		 * but in this case, all records moves to neighbor.
		 */
	
		// Case: leaf pages coalesce.
		if (page->p_header.is_leaf == 1) {
			
			for (i = 0, j = neighbor->p_header.numofkeys;
					i < page->p_header.numofkeys; i++, j++) {
				neighbor->leaf_record[j] = page->leaf_record[i];
			}
			neighbor->p_header.next_pgnum = page->p_header.next_pgnum;
			
			neighbor->p_header.numofkeys = balance_num;

		}
		
		// Case : internal page coalesce.
		else {

			neighbor->internal_record[neighbor->p_header.numofkeys].key
				= parent->internal_record[parent->p_header.numofkeys - 1].key;
			neighbor->internal_record[neighbor->p_header.numofkeys].child_pgnum
				= page->p_header.left_pgnum;

			child = PAGE_ALLOC();
			// update left child's parent number.
			file_read_page (tid, page->p_header.left_pgnum, child);
			child->p_header.parent_pgnum = neighbor_num;
			file_write_page (tid, page->p_header.left_pgnum, child);

			for (i = 0, j = neighbor->p_header.numofkeys + 1;
					i < page->p_header.numofkeys; i++, j++) {
				neighbor->internal_record[j] = page->internal_record[i];
				
				// update child's parent page number.
				file_read_page (tid, page->internal_record[i].child_pgnum, child);
				child->p_header.parent_pgnum = neighbor_num;
				file_write_page (tid, page->internal_record[i].child_pgnum, child);
			}
			free (child);

			neighbor->p_header.numofkeys = balance_num;
		}

		file_write_page (tid, neighbor_num, neighbor);
		free (neighbor);
		file_free_page (tid, pgnum);
		free (page);

		k_prime = parent->internal_record[parent->p_header.numofkeys - 1].key;
		free (parent);
		delete_entry (tid, parent_num, k_prime);

	}

	return 0;
}

int redistribute_pages (int tid, pagenum_t pgnum,
		pagenum_t neighbor_num, int neighbor_index) {
	
	pagenum_t parent_num;
	page_t* page, * parent, * neighbor, * child;
	int balance_num, i, j;

	page = PAGE_ALLOC();
	file_read_page (tid, pgnum, page);

	neighbor = PAGE_ALLOC();
	file_read_page (tid, neighbor_num, neighbor);

	parent_num = page->p_header.parent_pgnum;
	parent = PAGE_ALLOC();
	file_read_page (tid, parent_num, parent);

	// number of keys that page will have after redistribution.
	balance_num = (page->p_header.numofkeys + neighbor->p_header.numofkeys) / 2;

	/* Case: page has a neighbor to the right. */
	if (neighbor_index != -1) {

		// Leaf case
		if (page->p_header.is_leaf == 1) {
			/* Move records from neighbor to page.
			 */
			for (j = page->p_header.numofkeys, i = 0; j < balance_num; i++, j++) {
				page->leaf_record[j] = neighbor->leaf_record[i];
			}
			// update parent k_prime. (i is now index of first not-taken record)
			parent->internal_record[neighbor_index].key = neighbor->leaf_record[i].key;
			
			// shift neighbor's records.
			for (j = 0, i; i < neighbor->p_header.numofkeys; i++, j++) {
				neighbor->leaf_record[j] = neighbor->leaf_record[i];
			}		
		}

		// Internal case
		else {
			/* Move records from neighbor to page.
			 * Also update child page's parents.
			 */
			page->internal_record[page->p_header.numofkeys].key
				= parent->internal_record[neighbor_index].key;
			page->internal_record[page->p_header.numofkeys].child_pgnum
				= neighbor->p_header.left_pgnum;
			
			child = PAGE_ALLOC();
			// update left child's parent number.
			file_read_page (tid, neighbor->p_header.left_pgnum, child);
			child->p_header.parent_pgnum = pgnum;
			file_write_page (tid, neighbor->p_header.left_pgnum, child);

			for (i = 0, j = page->p_header.numofkeys + 1; j < balance_num; i++, j++) {
				
				page->internal_record[j] = neighbor->internal_record[i];
				
				// update child's parent number.
				file_read_page (tid, neighbor->internal_record[i].child_pgnum, child);
				child->p_header.parent_pgnum = pgnum;
				file_write_page (tid, neighbor->internal_record[i].child_pgnum, child);
			}
			parent->internal_record[neighbor_index].key
				= neighbor->internal_record[i].key;
			// shift neighbor's records.
			neighbor->p_header.left_pgnum = neighbor->internal_record[i].child_pgnum;
			for (j = 0, ++i; i < neighbor->p_header.numofkeys; i++, j++) {
				neighbor->internal_record[j] = neighbor->internal_record[i];
			}
			free (child);
		}
	}
	/* Case: page has a neighbor to the left. */
	else {

		// Leaf case:
		if (page->p_header.is_leaf == 1) {
			/* shift page's records first.
			 */
			for (j = balance_num - 1, i = page->p_header.numofkeys - 1; i >= 0; i--, j--) {
				page->leaf_record[j] = page->leaf_record[i];
			}
			// Move records from neighbor to page.
			for (j, i = neighbor->p_header.numofkeys - 1; j >= 0; i--, j--) {
				page->leaf_record[j] = neighbor->leaf_record[i];
			}
			// update parent.
			parent->internal_record[parent->p_header.numofkeys - 1].key = page->leaf_record[0].key;
		}

		// Internal case:
		else {
			child = PAGE_ALLOC();

			for (i = page->p_header.numofkeys - 1, j = balance_num; i >= 0; i--, j--) {
				page->internal_record[j] = page->internal_record[i];
			}
			page->internal_record[j].key = parent->internal_record[parent->p_header.numofkeys - 1].key;
			page->internal_record[j].child_pgnum = page->p_header.left_pgnum;

			for (i = balance_num + 1, j = 0; i < neighbor->p_header.numofkeys; i++, j++) {

				page->internal_record[j] = neighbor->internal_record[i];
			
				// update child's parent number.
				file_read_page (tid, neighbor->internal_record[i].child_pgnum, child);
				child->p_header.parent_pgnum = pgnum;
				file_write_page (tid, neighbor->internal_record[i].child_pgnum, child);
			}
			// update parent's key.
			parent->internal_record[parent->p_header.numofkeys - 1].key 
				= neighbor->internal_record[balance_num].key;
			
			page->p_header.left_pgnum = neighbor->internal_record[balance_num].child_pgnum;
			file_read_page (tid, neighbor->internal_record[balance_num].child_pgnum, child);
			child->p_header.parent_pgnum = pgnum;
			file_write_page (tid, neighbor->internal_record[balance_num].child_pgnum, child);

			free (child);
		}
	}

	file_write_page (tid, parent_num, parent);
	free (parent);

	/* page now has balance number of keys.
	 * neighbor lost keys, so fewer its number of keys
	 * at the amount of page gets.
	 */

	neighbor->p_header.numofkeys -= balance_num - page->p_header.numofkeys;
	page->p_header.numofkeys = balance_num;

	file_write_page (tid, pgnum, page);
	file_write_page (tid, neighbor_num, neighbor);

	free (page);
	free (neighbor);

	return 0;
}

			


int get_neighbor_index (page_t* parent, pagenum_t pagenum) {
	int i;

	/* Return the index of the right neighbor.
	 */

	if (parent->p_header.left_pgnum == pagenum)
		return 0;

	for (i = 0; i < parent->p_header.numofkeys; i++) {
		if (parent->internal_record[i].child_pgnum == pagenum)
			break;
	}

	/* If page is the right most page,
	 * return -1
	 */
	if (i + 1 == parent->p_header.numofkeys)
		return -1;
	else if (i + 1 > parent->p_header.numofkeys) {
		perror ("No page number in parent.");
		exit(EXIT_FAILURE);
	}
	else
		return i + 1;
}

// Join.
int64_t find_least_key (int tid) {
	
	pagenum_t pgnum;
	page_t* page;
	int64_t key;

	pgnum = get_rootpgnum (tid);

	if (pgnum == 0)
		return MAX_KEY_VALUE;

	page = PAGE_ALLOC();

	file_read_page (tid, pgnum, page);

	if (page->p_header.is_leaf == 0) {

		pgnum = page->p_header.left_pgnum;
		file_read_page (tid, pgnum, page);
	}
	key = page->leaf_record[0].key;
	free (page);
	return key;	
}

int64_t get_next_record (int tid, int64_t key) {

	pagenum_t pgnum;
	page_t* page;
	int index;
	int64_t next_key;

	pgnum = find_leaf_page (tid, key);
	
	page = PAGE_ALLOC();
	file_read_page (tid, pgnum, page);

	index = 0;
	while (page->leaf_record[index].key != key) {
		index++;
	}
	
	index++;
	if (index >= page->p_header.numofkeys) {

		if (page->p_header.next_pgnum == 0) {
			return MAX_KEY_VALUE;
		}
		pgnum = page->p_header.next_pgnum;
		file_read_page (tid, pgnum, page);
		
		next_key = page->leaf_record[0].key;
	}
	else {
		next_key = page->leaf_record[index].key;
	}

	return next_key;
}

int get_table_value (int tid, int64_t key, char* ret_val) {

	pagenum_t pgnum;
	page_t* page;

	pgnum = find_leaf_page (tid, key);
	page = PAGE_ALLOC();
	file_read_page (tid, pgnum, page);

	strcpy (ret_val, find_value (page, key));
	free (page);
	if (ret_val == NULL)
		return -1;
	else return 0;
}

