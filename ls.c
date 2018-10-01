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

void ls(char *files[], bool recurse)
{
    FTS *fts = fts_open(files, 0, NULL);
    if (fts == NULL)
        err(1, "fts_open %s", files[0]);

    for (FTSENT *cur = fts_read(fts); cur != NULL; cur = fts_read(fts)) {
        if (cur->fts_info == FTS_D) {
            printf("%s:\n", cur->fts_path);
            for (FTSENT *ent = fts_children(fts, 0); ent != NULL; ent = ent->fts_link) {
                printf("%s\n", ent->fts_accpath);
            }
            printf("\n");
            if (!recurse)
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
    list_filter filter = getuid() == 0 ? ALL_EXCEPT_DOT : NORMAL;
    bool multi_col = isatty(STDOUT_FILENO);
    bool file_type_char = false;
    bool recurse = false;

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
            case 'F':
                file_type_char = true;
                break;
            case 'R':
                recurse = true;
                break;
        }
    }

    (void)file_type_char;
    (void)multi_col;
    (void)filter;

    argc -= optind;
    argv += optind;

    char **files = argv;

    if (*argv == NULL)
        files = (char*[]){".", NULL};

    ls(files, recurse);
}
