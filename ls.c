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
} options;

options opt;

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

void print(FTSENT *ent)
{
    char mode[12]; // 11 chars + null, according to man page
    struct passwd *user;
    struct group *group;
    struct tm *time;
    struct stat *st = ent->fts_statp;

    if (opt.print_blocks) {
        long long block_bytes = st->st_blocks * 512;
        if (opt.humanize) {
            char buf[5]; // null + 4 chars, because 999B fits, and 1000B = 1.0K
            if (humanize_number(buf, sizeof buf, block_bytes, "", HN_AUTOSCALE, HN_DECIMAL | HN_B | HN_NOSPACE) == -1)
                err(1, "humanize_number(%lld)", block_bytes);

            printf("%s ", buf);
        } else
            printf("%lld ", (long long)ceil(block_bytes / (double)opt.blocksize));
    }

    if (opt.print_inode) {
        // cast to potentially larger size for compatibility with more platforms
        printf("%llu ", (long long unsigned)st->st_ino);
    }

    if (opt.long_mode) {
        strmode(st->st_mode, mode);

        printf("%s%d ", mode, st->st_nlink);

        if (!opt.numerical_ids && (user = getpwuid(st->st_uid)) != NULL)
            printf("%s ", user->pw_name);
        else
            printf("%d ", st->st_uid);

        if (!opt.numerical_ids && (group = getgrgid(st->st_gid)) != NULL)
            printf("%s ", group->gr_name);
        else
            printf("%d ", st->st_gid);

        if (opt.time == STATUS_CHANGED)
            time = localtime(&st->st_ctime);
        else if (opt.time == LAST_MODIFIED)
            time = localtime(&st->st_mtime);
        else
            time = localtime(&st->st_atime);

        if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode))
            printf("%d, %d ", major(st->st_rdev), minor(st->st_rdev));
        else if (opt.humanize) {
            char buf[5]; // null + 4 chars, because 999B fits, and 1000B = 1.0K
            if (humanize_number(buf, sizeof buf, st->st_size, "", HN_AUTOSCALE, HN_DECIMAL | HN_B | HN_NOSPACE) == -1)
                err(1, "humanize_number(%lld)", (long long)st->st_size);

            printf("%s ", buf);
        } else
            // cast to potentially larger size for compatibility with more platforms
            printf("%lld ", (long long)st->st_size);

        printf("%s %2d %02d:%02d ", months[time->tm_mon], time->tm_mday, time->tm_hour, time->tm_min);
    }

    for (char *c = ent->fts_name; *c; ++c) {
        if (opt.hide_nonprintable && !isprint((int)*c))
            putchar('?');
        else
            putchar(*c);
    }

    if (opt.file_type_char) {
        if (S_ISDIR(st->st_mode)) putchar('/');
        else if (S_ISLNK(st->st_mode)) putchar('@');
        else if (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) putchar('*');
        else if (S_ISWHT(st->st_mode)) putchar('%');
        else if (S_ISSOCK(st->st_mode)) putchar('=');
        else if (S_ISFIFO(st->st_mode)) putchar('|');
    }

    if (opt.long_mode) {
        if (ent->fts_info == FTS_SL || ent->fts_info == FTS_SLNONE) {
            char path[PATH_MAX] = {0};
            snprintf(path, sizeof path, "%s/%s", ent->fts_path, ent->fts_accpath);

            char buf[PATH_MAX] = {0};
            if (readlink(path, buf, sizeof buf) == -1) {
                err(1, "readlink(%s)", path);
            }
            printf(" -> %s", buf);
        }
    }

    printf("\n");
}

int cmp_alpha(const FTSENT **a, const FTSENT **b)
{
    return strcmp((*a)->fts_name, (*b)->fts_name);
}

int cmp_time(const FTSENT **a, const FTSENT **b)
{
    time_t at, bt;

    if (opt.time == STATUS_CHANGED) {
        at = (*a)->fts_statp->st_ctime;
        bt = (*b)->fts_statp->st_ctime;
    } else if (opt.time == LAST_ACCESSED) {
        at = (*a)->fts_statp->st_atime;
        bt = (*b)->fts_statp->st_atime;
    } else {
        at = (*a)->fts_statp->st_mtime;
        bt = (*b)->fts_statp->st_mtime;
    }

    if (at > bt) return -1;
    if (at == bt) return 0;
    return 1;
}

int cmp_size(const FTSENT **a, const FTSENT **b)
{
    off_t asz, bsz;

    asz = (*a)->fts_statp->st_size;
    bsz = (*b)->fts_statp->st_size;

    if (asz > bsz) return -1;
    if (asz == bsz) return 0;
    return 1;
}

int main_compare(const FTSENT **a, const FTSENT **b)
{
    int res;

    if (opt.sort == SIZE)
        res = cmp_size(a, b);
    else if (opt.sort == TIME) {
        res = cmp_time(a, b);
        if (res == 0)
            res = cmp_alpha(a, b);
    } else if (opt.sort == ALPHABETICAL)
        res = cmp_alpha(a, b);
    else
        errx(1, "invalid sort function");

    if (opt.sort_reverse)
        res *= -1;
    return res;
}

void ls(char *files[], int files_len)
{
    int fts_flags = FTS_PHYSICAL;
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

        if (cur->fts_info == FTS_D) {
            if (files_len > 1 || opt.recurse)
                printf("%s:\n", cur->fts_path);

            for (FTSENT *ent = fts_children(fts, 0); ent != NULL; ent = ent->fts_link) {
                if (ent->fts_name[0] == '.' && opt.filter == NORMAL)
                    continue;

                print(ent);
            }
            printf("\n"); // TODO: don't print for last entry
            if (!opt.recurse)
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
        .humanize = false,
        .blocks_kb = false,
        .hide_nonprintable = isatty(STDOUT_FILENO)
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

    int ignore;
    (void)getbsize(&ignore, &opt.blocksize);

    if (opt.blocks_kb)
        opt.blocksize = 1024;

    ls(files, files_len);
}
