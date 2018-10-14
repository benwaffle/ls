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
#include <time.h>

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
} options;

const char *months[] = {
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"
};

void print(FTSENT *ent, options *opt)
{
    char mode[12]; // 11 chars + null, according to man page
    struct passwd *user;
    struct group *group;
    struct tm *time;
    struct stat *st = ent->fts_statp;

    if (opt->long_mode) {
        strmode(st->st_mode, mode);

        printf("%s%d ", mode, st->st_nlink);

        if (!opt->numerical_ids && (user = getpwuid(st->st_uid)) != NULL)
            printf("%s ", user->pw_name);
        else
            printf("%d ", st->st_uid);

        if (!opt->numerical_ids && (group = getgrgid(st->st_gid)) != NULL)
            printf("%s ", group->gr_name);
        else
            printf("%d ", st->st_gid);

        if (opt->time == STATUS_CHANGED)
            time = localtime(&st->st_ctime);
        else if (opt->time == LAST_MODIFIED)
            time = localtime(&st->st_mtime);
        else
            time = localtime(&st->st_mtime);

        if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode))
            printf("%d, %d ", major(st->st_rdev), minor(st->st_rdev));
        else
            printf("%lld ", st->st_size);

        printf("%s %2d %02d:%02d ", months[time->tm_mon], time->tm_mday, time->tm_hour, time->tm_min);
    }

    printf("%s", ent->fts_name);

    if (opt->file_type_char) {
        if (S_ISDIR(st->st_mode)) printf("/");
        else if (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) printf("*");
        else if (S_ISLNK(st->st_mode)) printf("@");
        else if (S_ISWHT(st->st_mode)) printf("%%");
        else if (S_ISSOCK(st->st_mode)) printf("=");
        else if (S_ISFIFO(st->st_mode)) printf("|");
    }

    printf("\n");
}

int sort_alpha(const FTSENT **a, const FTSENT **b)
{
    return strcmp((*a)->fts_name, (*b)->fts_name);
}

int sort_size(const FTSENT **a, const FTSENT **b)
{
    off_t asz, bsz;

    asz = (*a)->fts_statp->st_size;
    bsz = (*b)->fts_statp->st_size;

    if (asz > bsz) return -1;
    if (asz == bsz) return 0;
    return 1;
}

void ls(char *files[], int files_len, options *opt)
{
    int fts_flags = FTS_PHYSICAL;
    if (opt->filter == ALL)
        fts_flags |= FTS_SEEDOT;

    typedef int compar(const FTSENT **, const FTSENT **);

    compar *cmp = NULL;

    if (opt->sort == SIZE)
        cmp = sort_size;
    /*else if (opt->sort == TIME) {
        cmp = sort_time;
    }*/ else if (opt->sort == ALPHABETICAL)
        cmp = sort_alpha;

    FTS *fts = fts_open(files, fts_flags, cmp);
    if (fts == NULL)
        err(1, "fts_open %s", files[0]);

    for (FTSENT *cur = fts_read(fts); cur != NULL; cur = fts_read(fts)) {
        // ls -R does not show dotfiles
        if (cur->fts_name[0] == '.' && opt->filter == NORMAL && cur->fts_level > 0) {
            fts_set(fts, cur, FTS_SKIP);
            continue;
        }

        if (cur->fts_info == FTS_D) {
            if (files_len > 1 || opt->recurse)
                printf("%s:\n", cur->fts_path);

            for (FTSENT *ent = fts_children(fts, 0); ent != NULL; ent = ent->fts_link) {
                if (ent->fts_name[0] == '.' && opt->filter == NORMAL)
                    continue;

                print(ent, opt);
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
    bool multi_col = isatty(STDOUT_FILENO);

    opt = (options){
        .recurse = false,
        .long_mode = false,
        .filter = (getuid() == 0 ? ALL_EXCEPT_DOT : NORMAL),
        .sort = ALPHABETICAL,
        .sort_reverse = false,
        .time = LAST_MODIFIED,
        .numerical_ids = false,
        .file_type_char = false
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
            case 'c':
                opt.time = STATUS_CHANGED;
                break;
            case 'F':
                opt.file_type_char = true;
                break;
            case 'f':
                opt.sort = NOT_SORTED;
                break;
            case 'l':
                opt.long_mode = true;
                break;
            case 'n':
                opt.numerical_ids = true;
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
            case 't':
                opt.time = LAST_MODIFIED;
                break;
            case 'u':
                opt.time = LAST_ACCESSED;
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
