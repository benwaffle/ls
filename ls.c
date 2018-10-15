#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <fts.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <err.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <math.h>

#include "compare.h"
#include "opt.h"
#include "print.h"

options opt;
void ls(char *files[], int files_len)
{
    int fts_flags = FTS_PHYSICAL;
    bool first = true;
    if (opt.filter == ALL)
        fts_flags |= FTS_SEEDOT;

    FTS *fts = fts_open(files, fts_flags, (opt.sort == NOT_SORTED ? NULL : main_compare));
    if (fts == NULL)
        err(1, "fts_open %s", files[0]);

    for (FTSENT *cur = fts_read(fts); cur != NULL; cur = fts_read(fts)) {
        // ls -R does not show dotfiles
        if (cur->fts_name[0] == '.' && opt.filter == NORMAL && cur->fts_level > 0) {
            fts_set(fts, cur, FTS_SKIP);
            continue;
        }

        if (!opt.go_into_dirs)
            fts_set(fts, cur, FTS_SKIP);

        if (cur->fts_info == FTS_D) {
            if (first)
                first = false;
            else
                printf("\n");

            if ((files_len > 1 || opt.recurse) && opt.go_into_dirs)
                printf("%s:\n", cur->fts_path);

            if (opt.go_into_dirs) {
                FTSENT *children = fts_children(fts, 0);
                for (FTSENT *ent = children; ent; ent = ent->fts_link)
                    get_print_data(ent);

                print_all(children);

                for (FTSENT *ent = children; ent; ent = ent->fts_link) {
                    free(DATA(ent)->user);
                    free(DATA(ent)->group);
                    free(DATA(ent)->filename);
                    free(ent->fts_pointer);
                }
            } else {
                get_print_data(cur);
                cur->fts_link = NULL;
                print_all(cur);
                free(DATA(cur)->user);
                free(DATA(cur)->group);
                free(DATA(cur)->filename);
                free(cur->fts_pointer);
            }

            if (!opt.recurse)
                fts_set(fts, cur, FTS_SKIP);
        } else if (cur->fts_info == FTS_DC
                || cur->fts_info == FTS_DNR
                || cur->fts_info == FTS_ERR
                || cur->fts_info == FTS_NS
                || cur->fts_info == FTS_NSOK) {
            fprintf(stderr, "%s: %s: %s\n", getprogname(), cur->fts_accpath, strerror(cur->fts_errno));
        }
    }

    if (errno != 0)
        err(1, "fts_read");
    fts_close(fts);
}

int main(int argc, char *argv[])
{
    int ch;
    bool multi_col = isatty(STDOUT_FILENO);

    opt = (options){
        .long_mode = false,
        .recurse = false,
        .filter = (getuid() == 0 ? ALL_EXCEPT_DOT : NORMAL),
        .sort = ALPHABETICAL,
        .sort_reverse = false,
        .time = LAST_MODIFIED,
        .numerical_ids = false,
        .file_type_char = false,
        .print_inode = false,
        .print_blocks = false,
        .blocksize = 512,
        .blocks_kb = false,
        .humanize = false,
        .hide_nonprintable = isatty(STDOUT_FILENO),
        .go_into_dirs = true
    };

    while ((ch = getopt(argc, argv, "AacCdFfhiklnqRrSstuwx1")) != -1) {
        switch (ch) {
            case '1':
                opt.long_mode = false;
                break;
            case 'A':
                opt.filter = ALL_EXCEPT_DOT;
                break;
            case 'a':
                opt.filter = ALL;
                break;
            case 'C':
                multi_col = true;
                break;
            case 'c':
                opt.time = STATUS_CHANGED;
                break;
            case 'd':
                opt.go_into_dirs = false;
                break;
            case 'F':
                opt.file_type_char = true;
                break;
            case 'f':
                opt.sort = NOT_SORTED;
                break;
            case 'h':
                opt.humanize = true;
                break;
            case 'i':
                opt.print_inode = true;
                break;
            case 'k':
                opt.blocks_kb = true;
                break;
            case 'l':
                opt.long_mode = true;
                break;
            case 'n':
                opt.numerical_ids = true;
                opt.long_mode = true;
                break;
            case 'q':
                opt.hide_nonprintable = true;
                break;
            case 'R':
                opt.recurse = true;
                break;
            case 'r':
                opt.sort_reverse = true;
                break;
            case 'S':
                opt.sort = SIZE;
                break;
            case 's':
                opt.print_blocks = true;
                break;
            case 't':
                opt.sort = TIME;
                break;
            case 'u':
                opt.time = LAST_ACCESSED;
                break;
            case 'w':
                opt.hide_nonprintable = false;
                break;
        }
    }

    (void)multi_col;

    argc -= optind;
    argv += optind;

    char **files = argv;
    int files_len = argc;

    // ls, without args, behaves as if you ran `ls .'
    if (*argv == NULL) {
        files = (char*[]){".", NULL};
        files_len = 1;
    }

    if (opt.print_blocks || opt.long_mode) {
        int ignore;
        (void)getbsize(&ignore, &opt.blocksize);
    }

    if (opt.blocks_kb)
        opt.blocksize = 1024;

    ls(files, files_len);
}
