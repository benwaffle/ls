#include <stdio.h>
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

typedef enum {
    NORMAL,
    ALL,
    ALL_EXCEPT_DOT
} list_filter;

typedef struct {
    bool long_mode;
    bool recurse;
} options;

void ls(char *files[], int files_len, options *opt)
{
    FTS *fts = fts_open(files, 0, NULL);
    if (fts == NULL)
        err(1, "fts_open %s", files[0]);

    for (FTSENT *cur = fts_read(fts); cur != NULL; cur = fts_read(fts)) {
        if (cur->fts_info == FTS_D) {
            if (files_len > 1 || opt->recurse)
                printf("%s:\n", cur->fts_path);
            for (FTSENT *ent = fts_children(fts, 0); ent != NULL; ent = ent->fts_link) {
                printf("%s\n", ent->fts_accpath);
            }
            printf("\n"); // TODO: don't print for last entry
            if (!opt->recurse)
                fts_set(fts, cur, FTS_SKIP);
        }
    }
    if (errno != 0)
        err(1, "fts_read");
    fts_close(fts);
}

int main(int argc, char *argv[])
{
    int ch;
    options opt;
    list_filter filter = getuid() == 0 ? ALL_EXCEPT_DOT : NORMAL;
    bool multi_col = isatty(STDOUT_FILENO);

    opt = (options){
        .recurse = false,
        .long_mode = false
    };

    while ((ch = getopt(argc, argv, "AacCdFfhiklnqRrSstuwx1")) != -1) {
        switch (ch) {
            case 'A':
                filter = ALL_EXCEPT_DOT;
                break;
            case 'a':
                filter = ALL;
                break;
            case 'C':
                multi_col = true;
                break;
            case 'l':
                opt.long_mode = true;
                break;
            case 'R':
                opt.recurse = true;
                break;
        }
    }

    (void)multi_col;
    (void)filter;

    argc -= optind;
    argv += optind;

    char **files = argv;
    int files_len = argc;

    // ls, without args, behaves as if you ran `ls .'
    if (*argv == NULL) {
        files = (char*[]){".", NULL};
        files_len = 1;
    }

    ls(files, files_len, &opt);
}
