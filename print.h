#ifndef _PRINT_H_
#define _PRINT_H_

#include <fts.h>
#include <limits.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define DATA(ent) ((print_data *)ent->fts_pointer)

typedef struct {
	char inode[21];
	char blocks[21];
	char mode[12];
	char nlink[11];
	char *user;
	char *group;
	char time[3 + 1 + 2 + 1 + 5 + 1]; // e.g. Jan 12 11:22
	union {
		char size[21]; // e.g.: x, x
		struct {
			int major;
			int minor;
		};
	};
	char *filename;
	char mode_char;
	char sym_target[PATH_MAX];
} print_data;

void get_print_data(FTSENT *);
void print_all(FTSENT *);

#endif
