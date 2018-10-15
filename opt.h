#ifndef _OPT_H_
#define _OPT_H_

#include <stdbool.h>

typedef enum {
    NORMAL,
    ALL,
    ALL_EXCEPT_DOT
} list_filter;

typedef enum {
    STATUS_CHANGED,
    LAST_MODIFIED,
    LAST_ACCESSED
} time_category;

typedef enum {
    NOT_SORTED,
    SIZE,
    TIME,
    ALPHABETICAL
} sort_field;

typedef struct {
    bool long_mode;
    bool recurse;
    list_filter filter;
    sort_field sort;
    bool sort_reverse;
    time_category time;
    bool numerical_ids;
    bool file_type_char;
    bool print_inode;
    bool print_blocks;
    long blocksize;
    bool blocks_kb;
    bool humanize;
    bool hide_nonprintable;
    bool go_into_dirs;
} options;

extern options opt;

#endif
