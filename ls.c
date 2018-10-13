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
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

typedef enum {
    NORMAL,
    ALL,
    ALL_EXCEPT_DOT
} list_filter;

typedef struct {
    bool long_mode;
    bool recurse;
    list_filter filter;
} options;

void print_long(FTSENT *ent)
{
    char mode[12]; // 11 chars + null, according to man page
    struct passwd *user;
    struct group *group;

    strmode(ent->fts_statp->st_mode, mode);
    user = getpwuid(ent->fts_statp->st_uid);
    group = getgrgid(ent->fts_statp->st_gid);

    printf("%s%ld %s %s %ld %s\n",
            mode,
            ent->fts_statp->st_nlink,
            user->pw_name,
            group->gr_name,
            ent->fts_statp->st_size,
            ent->fts_name);
}

void ls(char *files[], int files_len, options *opt)
{
    int fts_flags = FTS_PHYSICAL;
    if (opt->filter == ALL)
        fts_flags |= FTS_SEEDOT;

    FTS *fts = fts_open(files, fts_flags, NULL);
    if (fts == NULL)
        err(1, "fts_open %s", files[0]);

    for (FTSENT *cur = fts_read(fts); cur != NULL; cur = fts_read(fts)) {
        if (cur->fts_info == FTS_D) {
            if (files_len > 1 || opt->recurse)
                printf("%s:\n", cur->fts_path);
            for (FTSENT *ent = fts_children(fts, 0); ent != NULL; ent = ent->fts_link) {
                if (ent->fts_name[0] == '.' && opt->filter == NORMAL)
                    continue;

                if (opt->long_mode)
                    print_long(ent);
                else
                    printf("%s\n", ent->fts_name);
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
        .long_mode = false,
        .filter = (getuid() == 0 ? ALL_EXCEPT_DOT : NORMAL)
    };

    while ((ch = getopt(argc, argv, "AacCdFfhiklnqRrSstuwx1")) != -1) {
        switch (ch) {
            case 'A':
                opt.filter = ALL_EXCEPT_DOT;
                break;
            case 'a':
                opt.filter = ALL;
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
