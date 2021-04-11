#include "bpt.h"

// GlOBALS.

int fd;

void usage_1( void ) {
	printf ("Enter any of the following commands after the prompt > :\n"
	"\to <p>     -- open file which is in pathname <p>.\n"
	"\ti <k> <v> -- Insert <k> (an 64-bit integer) as key and <v> as value.\n"
	"\tf <k>     -- Find the value under key <k> and print value.\n"
	"\td <k>     -- Delete key <k> and its associated value.\n"
	"\tq         -- Quit. (Or use Ctrl-D.)\n"
	"\t?         -- Print this help message.\n");
}

/* debuging functions */

void print_page (pagenum_t pagenum) {
	Page_t* page;
	page = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (pagenum, page);
	int i;
	printf("number of keys : %d\n", page->p_header.number_of_keys);
	if (page->p_header.is_leaf == 1) {
		for (i = 0; i < page->p_header.number_of_keys; i++) {
			printf("%d | key : %ld , value : %s\n", i, page->leaf_record[i].key, page->leaf_record[i].value);
		}
	} else {
		printf("most left child pagenumber : %lu\n", page->p_header.right_sibling_page_num);
		for (i = 0; i < page->p_header.number_of_keys; i++) {
			printf("%d | key : %ld , child : %lu\n", i, page->internal_record[i].key, page->internal_record[i].child_pagenum);
		}
	}
	free(page);
}

void print_header (void) {
	Page_t * header;
	header = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (0, header);
	printf("free page number: %lu\nroot page number: %lu\nnumber of pages: %lu\n",
			header->m_data.freepagenum, header->m_data.rootpagenum, header->m_data.numofpages);
	free (header);
}


/* File Space Manager */

typedef uint64_t pagenum_t;

// Allocate an on-disk page from the free page list
pagenum_t file_alloc_page () {
	printf("file_alloc_page called.\n");
	pagenum_t _nextnum;
	Page_t* _headerpage = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (0, _headerpage);
	pagenum_t _freepagenum = _headerpage->m_data.freepagenum;

	/* Case: free page exist.
	 * update headerpage's first freepagenum to second freepagenum.
	 */
	if (_freepagenum != 0) {
		pread (fd, &_nextnum, sizeof(pagenum_t), PAGE_SIZE_ * _freepagenum);
		printf("freepagenum : %lu\n", _freepagenum);
		_headerpage->m_data.freepagenum = _nextnum;
		file_write_page (0, _headerpage);
		free (_headerpage);
		return _freepagenum;
	}
	/* Case: free page don't exist.
	 * expand file and return the very first freepage.
	 * update header page freepagenum to very first page.
	 */
	else {
		pagenum_t _last_page = _headerpage->m_data.numofpages;
		pagenum_t i;
		// expand file size +102.4MB
		for (i = _last_page; i < _last_page + 25000; i++) {
			_nextnum = i + 1;
			pwrite (fd, &_nextnum, sizeof(pagenum_t), PAGE_SIZE_ * i);
		}
		// last free page points 0.
		_nextnum = 0;
		pwrite (fd, &_nextnum, sizeof(pagenum_t), PAGE_SIZE_ * (i - 1));
		char _end = '0'; // guarantee last freepage size.
		pwrite (fd, &_end, sizeof(char), PAGE_SIZE_ * i - sizeof(char));
		_headerpage->m_data.freepagenum = _last_page;
		_headerpage->m_data.numofpages = _last_page + 25000;
		print_header();
		file_write_page (0, _headerpage);
		free (_headerpage);
		return _last_page;
	}
}

void file_free_page (pagenum_t pagenum) {
	Page_t* _header = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (0, _header);
	pagenum_t _freepagenum = _header->m_data.freepagenum;

	// update first free page number to pagenum.
	_header->m_data.freepagenum = pagenum;

	file_write_page (0, _header);
	free (_header);

	// new free page header will point previous free page number.
	pwrite (fd, &_freepagenum, sizeof(pagenum_t), PAGE_SIZE_ * pagenum);	
}

void file_read_page (pagenum_t pagenum, Page_t* dest) {
	// Read on-disk page and copy to dest
	if (pread (fd, dest, PAGE_SIZE_, PAGE_SIZE_ * pagenum) == -1) {
		printf("page read failed.\n");
	}
}

void file_write_page (pagenum_t pagenum, const Page_t* src) {
	// Write in-memory page src to on-disk page
	if (pwrite (fd, src, PAGE_SIZE_, PAGE_SIZE_ * pagenum) == -1) {
		printf("page write failed.\n");
	}
}

/* Index Manager */

int open_table (char* pathname) {
	// File already exist, just open.	
	if (access (pathname, F_OK) == 0) {
		fd = open (pathname, O_RDWR | O_CREAT, S_IRWXU);
		if (fd == 0) {
			printf("file open failed\n");
		}
	}

	// Case: make new DB. (create file)
	else {
		fd = open (pathname, O_RDWR | O_CREAT, S_IRWXU);
		if (fd == 0) {
			printf("file open failed\n");
		}
		// make 1 header page, 25000 free pages)
		Page_t *_header_page = make_header_page();
		file_write_page (0, _header_page);
		file_alloc_page();
	}
	
	// create unique_id of table file.
	int unique_id = 0;
	int i;
	for (i = 0; i < strlen(pathname); i++) {
		unique_id += (int)pathname[i] * (i + 1);
	}
	return unique_id;
}

int db_insert (int64_t key, char* value) {	
	// get rootnumber.
	Page_t* _headerpage = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (0, _headerpage);
	pagenum_t _rootnum = _headerpage->m_data.rootpagenum;

	pagenum_t _target_pagenum;
	Page_t * _target_page;
	
	/* Case1: root not exist.
	 * make new db.
	 */
	if (_rootnum == 0) {
		_target_page = start_new_tree (key, value);
		// update to disk.
		_target_pagenum = file_alloc_page();
		file_read_page (0,  _headerpage);
		file_write_page (_target_pagenum, _target_page);
		free (_target_page);
		_headerpage->m_data.rootpagenum = _target_pagenum;
		file_write_page (0, _headerpage);
		free (_headerpage);
		printf("new db made.\n");
		return 0;
	}
	free (_headerpage);

	/* Case2: key already in db.
	 * The current implementation ignores duplicates.
	 */
	// find leaf page and check key exist in the page.

	_target_pagenum = find_leaf_page (_rootnum, key);
	_target_page = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (_target_pagenum, _target_page);
	if (find_value (_target_page, key) != NULL) {
		printf("key already in db.\n");
		free (_target_page);
		return 0;
	}
	printf("key not in db. inserting progress...\n");

	/* Case3~4: db already exists.
	 * (Rest of function body.)
	 */

	_target_pagenum = find_leaf_page (_rootnum, key);
	printf("target number : %lu\n", _target_pagenum);
	_target_page = (Page_t*)malloc(sizeof(Page_t));
	file_read_page(_target_pagenum, _target_page);

	/* Case3: leaf has room for key.
	 */

	if (_target_page->p_header.number_of_keys < 31) {
		_target_page = insert_into_leaf (_target_page, key, value);
		file_write_page (_target_pagenum, _target_page);
		free (_target_page);
		return 0;
	}
	
	/* Case4: leaf must be split.
	 * which is very heavy operation
	 */
	
	free (_target_page);
	insert_into_leaf_after_splitting (_target_pagenum, key, value);
	return 0;
}

int db_find (int64_t key, char* ret_val) {
	// get root page number.
	Page_t* _header_page = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (0, _header_page);
	pagenum_t rootnum = _header_page->m_data.rootpagenum;
	free (_header_page);
	
	// find page holds key.
	pagenum_t _pagenum = find_leaf_page(rootnum, key);
	Page_t* page = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (_pagenum, page);

	// copy value to ret_val.
	if (find_value (page, key) == NULL) {
		free (page);
		return 1;
	}
	else {
		strcpy (ret_val, find_value (page, key));
		free (page);
		return 0;
	}
}

int db_delete (int64_t key) {
	// get root.
	Page_t* _header = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (0, _header);
	pagenum_t _rootnum = _header->m_data.rootpagenum;
	free (_header);

	// get leaf holding key.
	pagenum_t _leafnum = find_leaf_page (_rootnum, key);
	Page_t* _leaf = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (_leafnum, _leaf);

	// if key not exist
	if (find_value (_leaf, key) == NULL) {
		free (_leaf);	
		return 1;
	}
	// if key exist
	else {
		free (_leaf);
		delete_entry (_leafnum, key);
		return 0;
	}
}

// Finding

pagenum_t find_leaf_page (pagenum_t rootnum, int64_t key) {
	int i = 0;
	pagenum_t c_pagenum = rootnum;
	// root not exist.
	if (c_pagenum == 0) {
		return c_pagenum;
	}

	Page_t* c_page = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (c_pagenum, c_page);
	// internal page case :
	while (c_page->p_header.is_leaf == 0) {
		// if key is in the most left child page
		if (key < c_page->internal_record[0].key) {
			c_pagenum = c_page->p_header.right_sibling_page_num;
		} else {
			i = 1;
			while (i < c_page->p_header.number_of_keys) {
				if (key >= c_page->internal_record[i].key) i++;
				else break;
			}
			c_pagenum = c_page->internal_record[i - 1].child_pagenum;
		}
		// after get child page number, free parent page and get child page
		file_read_page (c_pagenum, c_page);
	}
	free(c_page);
	return c_pagenum;
}

char * find_value (Page_t* page, int64_t key) {
	int i = 0;
	// sequencially search for key
	for (i = 0; i < page->p_header.number_of_keys; i++) {
		if (page->leaf_record[i].key == key) break;
	}
	if (i == page->p_header.number_of_keys) {
		printf("key is not in this leaf.\n");
		return NULL;
	}
	else
		return page->leaf_record[i].value;
}

// Insertion

/* make_page function makes in-memory page struct.
   default member values are all 0.
 */
Page_t * make_page (void) {
	printf("make_page called.\n");
	Page_t* new_page;
	new_page = malloc(sizeof(Page_t));
	if (new_page == NULL) {
		perror ("Page creation.");
		exit (EXIT_FAILURE);
	}
	return new_page;
}

Page_t * make_header_page (void) {
	printf("make_header_page called.\n");
	Page_t* _header_page = make_page();
	_header_page->m_data.freepagenum = 0;
	_header_page->m_data.rootpagenum = 0;
	_header_page->m_data.numofpages = 1;
	return _header_page;
}

Page_t * make_leaf_page (void) {
	printf("make_leaf_page called.\n");
	Page_t *leaf = make_page();
	leaf->p_header.number_of_keys = 0;
	leaf->p_header.is_leaf = 1;
	leaf->p_header.parent_page_number = 0;
	leaf->p_header.right_sibling_page_num = 0;
	return leaf;
}

Page_t * make_internal_page (void) {
	printf("make_internal_page called.\n");
	Page_t *internal = make_page();
	internal->p_header.number_of_keys = 0;
	internal->p_header.is_leaf = 0;
	internal->p_header.parent_page_number = 0;
	internal->p_header.right_sibling_page_num = 0;
	return internal;
}

Page_t * start_new_tree (int64_t key, char* value) {
	Page_t * root = make_leaf_page();
	root->leaf_record[0].key = key;
	strcpy (root->leaf_record[0].value, value);
	root->p_header.parent_page_number = 0;
	root->p_header.number_of_keys = 1;
	return root;
}

Page_t * insert_into_leaf (Page_t* page, int64_t key, char* value) {
	printf("insert_into_leaf called.\n");
	int i, insertion_point;
	insertion_point = 0;
	while (insertion_point < page->p_header.number_of_keys &&
			page->leaf_record[insertion_point].key < key) {
		insertion_point++;
	}
	for (i = page->p_header.number_of_keys; i > insertion_point; i--) {
		page->leaf_record[i] = page->leaf_record[i - 1];
	}
	page->leaf_record[insertion_point].key = key;
	strcpy (page->leaf_record[insertion_point].value, value);
	page->p_header.number_of_keys++;
	return page;
}

int insert_into_leaf_after_splitting
(pagenum_t pagenum, int64_t key, char* value) {
	printf("insert_into_leaf_after_splitting called.\n");
	Page_t* page, * new_page;
	pagenum_t new_pagenum = file_alloc_page();
	Leaf_record_t* temp_record;
	int insertion_index, split, new_key, i, j;

	page = (Page_t*)malloc(sizeof(Page_t));
	file_read_page(pagenum, page);
	new_page = make_leaf_page();

	temp_record = (Leaf_record_t*)calloc(32, sizeof(Leaf_record_t));
	if (temp_record == NULL) {
		perror("temporary records array.");
		exit(EXIT_FAILURE);
	}

	insertion_index = 0;
	while (insertion_index < 31 &&
			page->leaf_record[insertion_index].key < key) {
		insertion_index++;
	}

	for (i = 0, j = 0; i < page->p_header.number_of_keys; i++, j++) {
		if (j == insertion_index) j++;
		temp_record[j] = page->leaf_record[i];
	}

	temp_record[insertion_index].key = key;
	strcpy (temp_record[insertion_index].value, value);

	page->p_header.number_of_keys = 0;

	// order is 31, so splited left page has 15 record.
	split = 15;
	
	for (i = 0; i < split; i++) {
		page->leaf_record[i] = temp_record[i];
		page->p_header.number_of_keys++;
	}
	for (i = split, j = 0; i < 32; i++, j++) {
		new_page->leaf_record[j] = temp_record[i];
		new_page->p_header.number_of_keys++;
	}
	
	free (temp_record);
	// leaf page link update
	new_page->p_header.right_sibling_page_num = page->p_header.right_sibling_page_num;
	page->p_header.right_sibling_page_num = new_pagenum;

	new_page->p_header.parent_page_number = page->p_header.parent_page_number;
	new_key = new_page->leaf_record[0].key;

	file_write_page (pagenum, page);
	file_write_page (new_pagenum, new_page);
	free (page);
	free (new_page);
	insert_into_parent (pagenum, new_key, new_pagenum);
	return 0;
}

int insert_into_parent (pagenum_t left, int64_t key, pagenum_t right) {
	printf("insert_into_parent called.\n");
	int left_index;
	Page_t* left_page = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (left, left_page);

	pagenum_t _parentnum = left_page->p_header.parent_page_number;
	free (left_page);

	/* Case: new root. */

	if (_parentnum == 0) {
		insert_into_new_root (left, key, right);
		return 0;
	}

	/* Case: leaf or node. */

	/* Find the parent's pointer to the left node. */

	Page_t* parent_pg = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (_parentnum, parent_pg);

	// @@ if right index is 0, left would be most left child.
	left_index = get_left_index (parent_pg, left);
	
	/* Simple case: the new key fits into the node. */

	if (parent_pg->p_header.number_of_keys < 248) {
		free(parent_pg);
		insert_into_node (_parentnum, left_index, key, right);
		return 0;
	}

	/* Harder case: split a node in order to preserve the B+ tree properties. */
	
	free(parent_pg);
	insert_into_node_after_splitting (_parentnum, left_index, key, right);
	return 0;
}

int insert_into_new_root (pagenum_t left, int64_t key, pagenum_t right) {
	printf("insert_into_new_root called.\n");
	Page_t* root = make_internal_page();
	pagenum_t rootnum = file_alloc_page();

	printf("new rootnum : %lu\n", rootnum);
	
	root->internal_record[0].key = key;
	root->p_header.right_sibling_page_num = left;
	root->internal_record[0].child_pagenum = right;
	root->p_header.number_of_keys++;
	root->p_header.parent_page_number = 0;
	
	Page_t* child = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (left, child);
	child->p_header.parent_page_number = rootnum;
	file_write_page (left, child);
	
	file_read_page (right, child);
	child->p_header.parent_page_number = rootnum;
	file_write_page (right, child);

	Page_t* header_pg = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (0, header_pg);
	header_pg->m_data.rootpagenum = rootnum;
	file_write_page (0, header_pg);
	free (header_pg);

	file_write_page (rootnum, root);
	free (root);
	return 0;
}

int get_left_index (Page_t* parent, pagenum_t left) {
	printf("get_left_index called.\n");
	int left_index = 0;
	while (left_index <= parent->p_header.number_of_keys &&
			parent->internal_record[left_index].child_pagenum != left)
		left_index++;
	return left_index++;
}

int insert_into_node (pagenum_t pagenum, int left_index,
		int64_t key, pagenum_t right) {
	printf("insert_into_node called.\n");
	int i;

	Page_t* _page = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (pagenum, _page);

	for (i = _page->p_header.number_of_keys; i > left_index; i--) {
		_page->internal_record[i] = _page->internal_record[i - 1];
	}
	_page->internal_record[left_index].child_pagenum = right;
	_page->internal_record[left_index].key = key;
	_page->p_header.number_of_keys++;
	file_write_page (pagenum, _page);
	free(_page);
	return 0;
}

int insert_into_node_after_splitting (pagenum_t oldpagenum,
		int left_index, int64_t key, pagenum_t right) {
	printf("insert_into_node_after_splitting called.\n");
	int i, j, split, k_prime;
	Page_t* old_node, * new_node, * child;
	Internal_record_t* temp_record;

	/* First create a temporary set of records to hold everything
	 * in order, including new key and value, inserted in their
	 * correct places.
	 * Then create a new node and copy half of the records to
	 * the old node and the other half to the new.
	 */

	temp_record = calloc (249, sizeof(Internal_record_t));
	if (temp_record == NULL) {
		perror("Temporary record array for splitting nodes.");
		exit(EXIT_FAILURE);
	}
	old_node = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (oldpagenum, old_node);

	for (i = 0, j = 0; i < old_node->p_header.number_of_keys; i++, j++) {
		if (j == left_index) j++;
		temp_record[j] = old_node->internal_record[i];
	}

	temp_record[left_index].key = key;
	temp_record[left_index].child_pagenum = right;
	
	/* Create the new node and copy half the keys and pointers
	 * to the old and half to the new.
	 * (0~123 : old, 125~247 : new, 124 : parent)
	 */
	split = 124;
	new_node = make_internal_page();
	old_node->p_header.number_of_keys = 0;
	for (i = 0; i < split; i++) {
		old_node->internal_record[i] = temp_record[i];
		old_node->p_header.number_of_keys++;
	}
	k_prime = temp_record[split].key;
	for (++i, j = 0; i < 248; i++, j++) {
		new_node->internal_record[j] = temp_record[i];
		new_node->p_header.number_of_keys++;
	}
	new_node->p_header.right_sibling_page_num = temp_record[split].child_pagenum;
	free (temp_record);
	new_node->p_header.parent_page_number = old_node->p_header.parent_page_number;
	// change new node's child's parent to new node.
	pagenum_t new_node_num = file_alloc_page();
	for (i = 0; i <= new_node->p_header.number_of_keys; i++) {
		child = (Page_t*)malloc(sizeof(Page_t));
		file_read_page (new_node->internal_record[i].child_pagenum , child);
		child->p_header.parent_page_number = new_node_num;
		free(child);
	}

	/* Insert a new key into the parent of the two nodes
	 * resulting from the split, with the old node to
	 * the left and the new to the right.
	 */
	
	file_write_page (new_node_num, new_node);
	insert_into_parent (oldpagenum, k_prime, new_node_num);
	return 0;
}

// Deletion.

int delete_entry_from_leaf (pagenum_t pagenum, int64_t key) {
	
	Page_t* c_page = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (pagenum, c_page);
	c_page = remove_entry_from_leaf (c_page, key);
	
	// the happy case: just delete record.
	if (c_page->p_header.number_of_keys >= 1) {
		file_write_page (pagenum, c_page);
		free (c_page);
		return 0;
	}

	// leaf has no key, should be merged.
	else {
		// leafs are linked, so update link.
		update_link (pagenum);
		delete_entry_from_node (c_page->p_header.parent_page_number, key);
		file_free_page (pagenum);
		free (c_page);
		return 0;
	}
}

int delete_entry_from_node (pagenum_t pagenum, int64_t key) {
	int min_keys;
	pagenum_t neighbor;
	int neighbor_index;
	int k_prime_index, k_prime;
	int capacity;
	
	// Remove record from page
	
	Page_t* c_page = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (pagenum, c_page);
	// If leaf has no key, it becomes free page.
	c_page = remove_entry_from_leaf (pagenum, page, key);	

	/* Case1: deletion from the root.
	 */

	if (c_page->p_header.parent_page_number == 0) {
		c_page = adjust_root (c_page);
		file_write_page (pagenum, c_page);
		free (c_page);
		return 0;
	}

	/* Delayed merge operate.
	 * page don't merge until it has zero key.
	 */

	min_keys = 1;

	/* Case2: node stays at or above minimum.
	 * (the simple and most case.)
	 */

	if (page->p_header.number_of_keys >= min_keys) {
		free (page);
		return 0;
	}

	/* Case3~5: node falls below minimum.
	 * (rest of function body.)
	 */

	/* Case3~4: coalesce page and it's neighbor.
	 * Find the appropriate neighbor page with which
	 * to coalesce.
	 * Also find the key (k_prime) in the parent
	 * between the pointer to page _leaf and the pointer
	 * to the neighbor.
	 */

	/* Case3: leaf page can always be merged.
	 */

	/* Case4: internal page can be merged when its
	 * neighbor's number of keys aren't full.
	 */

	/* Case5: most unlikely case.
	 * in this case neighbor's number of key is full.
	 * internal page steals key from neighbor.
	 * so after stealing, each pages will have keys 
	 * half of order.(248, 0) -> (124, 124)
	 */



	
	return 0;
}

/* return key-deleted leaf page.
 */
Page_t * remove_entry_from_leaf (Page_t* leaf, int64_t key) {

	// Remove the key and shift other keys and values accordingly.
	int i = 0;
	// lookup key sequencially
	while (leaf->leaf_record[i].key != key)
		i++;
	for (++i; i < leaf->p_header.number_of_keys; i++) {
		leaf->leaf_record[i - 1] = leaf->leaf_record[i];
	}
	// One key fewer.
	leaf->p_header.number_of_keys--;
	
	return leaf;
}

Page_t * remove_entry_from_page (pagenum_t pagenum, Page_t* page, int64_t key) {
	
	// Remove the key and shift other keys and pointers accordingly.
	int i = 0;
	// lookup key sequencially
	while (page->internal_record[i].key != key)
		i++;
	for (++i; i < page->p_header.number_of_keys; i++) {
		page->internal_record[i - 1].key = page->internal_record[i].key;
		page->internal_record[i - 1].child_pagenum = page->internal_record[i].child_pagenum;
	}
	// One key fewer.
	page->p_header.number_of_keys--;

	if (page->p_header.number_of_keys == 0) {
		file_free_page (pagenum);
		return NULL;
	}
	return page;
}

// adjust root when root is leaf.
pagenum_t adjust_root (pagenum_t rootnum) {
	
	Page_t* root = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (rootnum, root);

	/* Case0: nonempty root.
	 * key already deleted, nothing to be done.
	 */

	if (root->p_header.number_of_keys > 0) {
		printf("number of key : %d\n", root->p_header.number_of_keys);
		free (root);
		return rootnum;
	}

	/* Case1: root has no child, root becomes free page.
	 * whole tree is empty.
	 */
	else if (root->p_header.is_leaf == 1) {
		printf("tree is going empty!\n");
		free (root);
		// root page becomes free page.
		file_free_page (rootnum);
		// page header update root page to 0.
		Page_t* _header = (Page_t*)malloc(sizeof(Page_t));
		file_read_page (0, _header);
		_header->m_data.rootpagenum = 0;
		file_write_page (0, _header);
		free (_header);
		return 0;
	}

	/* Case2: root has a child, promote the first child
	 * as the new root.
	 */
	else {
		// if left child exist, left child become root.
		if (root->p_header.right_sibling_page_num != 0) {
			rootnum = root->p_header.right_sibling_page_num;
		} else {
			rootnum = root->internal_record[0].child_pagenum;
		}
		free (root);
		Page_t* _header = (Page_t*)malloc(sizeof(Page_t));
		file_read_page (0, _header);
		_header->m_data.rootpagenum = rootnum;
		file_write_page(0, _header);
		free (_header);
		return rootnum;
	}
}

int update_link (pagenum_t pagenum) {
	
	pagenum_t parent_n, left_n, next_n;
	Page_t* parent, * left, * c;
	int i;

	c = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (pagenum, c);
	parent_n = c->p_header.parent_page_number;
	next_n = c->p_header.right_next_pagenum;
	free (c);

	if (parent_n == 0) {
		perror("Trying to update root's link");
		exit(EXIT_FAILURE);
	}

	parent = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (parent_n, parent);
	
	i = 0;
	while (i < parent->p_header.number_of_keys &&
			parent->internal_record[i] != pagenum)
		i++;
	if (i == 0)
		left_n = parent->p_header.right_next_pagenum;
	else if (i != parent->p_header.number_of_keys) {
		left_n = i - 1;
	}
	else {
		left_n = get_left_neighbor (parent);
	}

	free (parent);

	left = (Page_t*)malloc(sizeof(Page_t));
	file_read_page (left_n, left);
	
	left->p_header.left_most_pagenum = next_n;
	file_write_page (left_n, left);
	free (left);
	return 0;
}

// helper function for update link.
// find left neighbor leaf no matter how far.
pagenum_t get_left_neighbor (Page_t* page) {
	
	pagenum_t parent;
	int i;
	Page_t* parent_pg;
	
	parent = page->p_header.parent_page_number;

	parent_pg = (Page_t*)malloc(sizeof(Page_t));
	
	i = 0;
	// until get to root.
	while (parent != 0) {
		file_read_page (parent, parent_pg);
		
	}

}




