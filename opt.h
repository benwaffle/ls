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

typedef struct options {
    long blocksize;
    list_filter filter;
    sort_field sort;
    time_category time;
    bool blocks_kb : 1;
    bool file_type_char : 1;
    bool go_into_dirs : 1;
    bool hide_nonprintable : 1;
    bool humanize : 1;
    bool long_mode : 1;
    bool numerical_ids : 1;
    bool print_blocks : 1;
    bool print_inode : 1;
    bool recurse : 1;
    bool sort_reverse : 1;
} options;

extern options opt;

#endif
