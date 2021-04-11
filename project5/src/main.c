#include "bpt.h"
#include <time.h>

// MAIN

int main (void) {

	char instruction;
	char pathname[100];
	int64_t key;
	char value[120];
	pagenum_t inp_page;
	int tid, tid2;
	int cnt = 0;
	int cnt2 = 0;
	clock_t start, end, now;
	float delay;

	FILE* fp;

	usage_1();
	printf ("> ");
	while (scanf ("%c", &instruction) != EOF) {
		switch (instruction) {
			case 'q':
				while (getchar() != (int)'\n');
				return EXIT_SUCCESS;
				break;
			case 'o':
				scanf(" %[^\n]", pathname);
				tid = open_table (pathname);
				cnt++;
				if (tid != -1 && cnt < 10) {
					printf ("open_table success.\ntid: %d\n", tid);
				} else {
					perror ("file open failed.");
					return -1;
				}
				break;
			case 'i':
				cnt2 = 0;
				scanf ("%d %[^\n]", &tid, pathname);
				fp = fopen (pathname, "r");
				start = clock();
				while(1) {
					cnt2++;
					fscanf (fp, "%ld ", &key);
					now = clock();
					sprintf (value, "%ld", (long int)now);
					if (db_insert (tid, key, value) != 0) {
						perror ("key insertion failed");
					}
					if (cnt2 % 10 == 0)
						print_header (tid);
					if (feof(fp)) break;
				}
				end = clock();

				fclose (fp);
				delay = (float)(end - start) / CLOCKS_PER_SEC;
				printf ("%.3f s\n", delay);
				break;
			case 'f':
				scanf ("%d %ld", &tid, &key);
				if (db_find (tid, key, value)) {
					printf("the key is not in database.\n");
				} else {
					printf("key : %ld , value : %s\n", key, value);
				}
				break;
			case 'd':
				cnt2 = 0;
				fp = fopen ("inputd.txt", "r");
				scanf("%d", &tid);
				start = clock();
				while(1) {
					cnt2++;
					fscanf (fp, "%ld ", &key);
					if (db_delete (tid, key)) {
						printf("the key is not in database.\n");
					} else {
						printf("key deletion success.\n");
					}
					if (cnt2 % 10 == 0)
						print_header (tid);
					if (feof(fp)) break;
				}
				end = clock();

				fclose (fp);
				delay = (float)(end - start) / CLOCKS_PER_SEC;
				printf ("%.3f s\n", delay);
				break;
			case 'j':
				scanf("%d %d %[^\n]", &tid, &tid2, pathname);
				join_table (tid, tid2, pathname);
				break;
			case 'p':
				scanf("%d %lu", &tid, &inp_page);
				print_page(tid, inp_page);
				break;
			case 'h':
				scanf("%d", &tid);
				print_header(tid);
				break;
			default:
				usage_1();
				break;
		}
		while (getchar() != (int)'\n');
		printf("> ");
	}
	printf("\n");

	return EXIT_SUCCESS;
}
