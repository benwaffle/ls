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
#include <sys/sysmacros.h>

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

options opt;

const char *months[] = {
    "Jan", "Feb", "Mar",
    "Apr", "May", "Jun",
    "Jul", "Aug", "Sep",
    "Oct", "Nov", "Dec"
};

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

void get_print_data(FTSENT *ent)
{
    struct passwd *user;
    struct group *group;
    struct tm *time;
    struct stat *st = ent->fts_statp;
    print_data *data = calloc(1, sizeof(print_data));
    ent->fts_pointer = data;

    snprintf(data->inode, sizeof data->inode, "%llu", (long long unsigned)st->st_ino);

    long long block_bytes = st->st_blocks * 512;
    if (opt.humanize) {
        if (humanize_number(data->blocks, sizeof data->blocks, block_bytes, "", HN_AUTOSCALE, HN_DECIMAL | HN_B | HN_NOSPACE) == -1)
            err(1, "humanize_number(%lld)", block_bytes);
    } else {
        snprintf(data->blocks, sizeof data->blocks, "%lld", (long long)ceil(block_bytes / (double)opt.blocksize));
    }

    strmode(st->st_mode, data->mode);

    snprintf(data->nlink, sizeof data->nlink, "%ld", (long)st->st_nlink);

    if (!opt.numerical_ids && (user = getpwuid(st->st_uid)) != NULL)
        data->user = strdup(user->pw_name);
    else {
        data->user = calloc(1, 11);
        snprintf(data->user, 11, "%d", st->st_uid);
    }

    if (!opt.numerical_ids && (group = getgrgid(st->st_gid)) != NULL)
        data->group = strdup(group->gr_name);
    else {
        data->group = calloc(1, 11);
        snprintf(data->group, 11, "%d", st->st_gid);
    }

    if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
        data->major = major(st->st_rdev);
        data->minor = minor(st->st_rdev);
    } else if (opt.humanize) {
        if (humanize_number(data->size, sizeof data->size, st->st_size, "", HN_AUTOSCALE, HN_DECIMAL | HN_B | HN_NOSPACE) == -1)
            err(1, "humanize_number(%lld)", (long long)st->st_size);
    } else {
        // cast to potentially larger size for compatibility with more platforms
        snprintf(data->size, sizeof data->size, "%lld", (long long)st->st_size);
    }

    if (opt.time == STATUS_CHANGED)
        time = localtime(&st->st_ctime);
    else if (opt.time == LAST_MODIFIED)
        time = localtime(&st->st_mtime);
    else
        time = localtime(&st->st_atime);

    snprintf(data->time, sizeof data->time, "%s %2d %02d:%02d ", months[time->tm_mon], time->tm_mday, time->tm_hour, time->tm_min);

    data->filename = strdup(ent->fts_name);
    for (char *c = data->filename; *c; ++c)
        if (opt.hide_nonprintable && !isprint((int)*c))
            *c = '?';

    if (S_ISDIR(st->st_mode)) data->mode_char = '/';
    else if (S_ISLNK(st->st_mode)) data->mode_char = '@';
    else if (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) data->mode_char = '*';
#ifndef LINUX
    else if (S_ISWHT(st->st_mode)) data->mode_char = '%';
#endif
    else if (S_ISSOCK(st->st_mode)) data->mode_char = '=';
    else if (S_ISFIFO(st->st_mode)) data->mode_char = '|';

    if (ent->fts_info == FTS_SL || ent->fts_info == FTS_SLNONE) {
        char path[PATH_MAX] = {0};
        snprintf(path, sizeof path, "%s/%s", ent->fts_path, ent->fts_accpath);

        if (readlink(path, data->sym_target, sizeof data->sym_target) == -1) {
            err(1, "readlink(%s)", path);
        }
    }
}

void print_all(FTSENT *children)
{
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define DATA(ent) ((print_data*)ent->fts_pointer)
    unsigned max_inode_len, max_block_len, max_nlink_len, max_user_len, max_group_len, max_size_len, max_major_len, max_minor_len, max_time_len;
    blkcnt_t block_total;

    max_inode_len = max_block_len = max_nlink_len = max_user_len = max_group_len = max_size_len = max_major_len = max_minor_len = max_time_len = 0;
    block_total = 0;

    for (FTSENT *cur = children; cur; cur = cur->fts_link) {
        if (opt.go_into_dirs && (cur->fts_name[0] == '.' && opt.filter == NORMAL))
            continue;

        max_inode_len = MAX(max_inode_len, strlen(DATA(cur)->inode));
        max_block_len = MAX(max_block_len, strlen(DATA(cur)->blocks));
        max_nlink_len = MAX(max_nlink_len, strlen(DATA(cur)->nlink));
        max_user_len = MAX(max_user_len, strlen(DATA(cur)->user));
        max_group_len = MAX(max_group_len, strlen(DATA(cur)->group));
        max_time_len = MAX(max_time_len, strlen(DATA(cur)->time));

        block_total += cur->fts_statp->st_blocks;

        if (S_ISCHR(cur->fts_statp->st_mode) || S_ISBLK(cur->fts_statp->st_mode)) {
            unsigned major_len = (unsigned)floor(log10(DATA(cur)->major)+1);
            unsigned minor_len = (unsigned)floor(log10(DATA(cur)->minor)+1);
            max_major_len = MAX(max_major_len, major_len);
            max_minor_len = MAX(max_minor_len, minor_len);
            max_size_len = MAX(max_size_len, max_major_len + 2 + max_minor_len); // maj, min
        } else {
            max_size_len = MAX(max_size_len, strlen(DATA(cur)->size));
        }
    }

    if (opt.print_blocks) {
        printf("total %lld\n", (long long)block_total);
    }

    for (FTSENT *cur = children; cur; cur = cur->fts_link) {
        if (opt.go_into_dirs && (cur->fts_name[0] == '.' && opt.filter == NORMAL))
            continue;

        if (opt.print_inode) {
            printf("%*s ", max_inode_len, DATA(cur)->inode);
        }

        if (opt.print_blocks) {
            printf("%*s ", max_block_len, DATA(cur)->blocks);
        }

        if (opt.long_mode) {
            printf("%s ", DATA(cur)->mode);

            printf("%*s ", max_nlink_len, DATA(cur)->nlink);


            printf("%-*s  %-*s  ", max_user_len, DATA(cur)->user, max_group_len, DATA(cur)->group);

            if (S_ISCHR(cur->fts_statp->st_mode) || S_ISBLK(cur->fts_statp->st_mode)) {
                char buf[11+2+11] = {0};
                snprintf(buf, sizeof buf, "%*d, %*d", max_major_len, DATA(cur)->major, max_minor_len, DATA(cur)->minor);
                printf("%*s ", max_size_len, buf);
            } else {
                printf("%*s ", max_size_len, DATA(cur)->size);
            }

            printf("%*s ", max_time_len, DATA(cur)->time);
        }

        printf("%s", DATA(cur)->filename);

        if (opt.file_type_char) {
            putchar(DATA(cur)->mode_char);
        }

        if (opt.long_mode) {
            if (cur->fts_info == FTS_SL || cur->fts_info == FTS_SLNONE) {
                char path[PATH_MAX] = {0};
                snprintf(path, sizeof path, "%s/%s", cur->fts_path, cur->fts_accpath);

                char buf[PATH_MAX] = {0};
                if (readlink(path, buf, sizeof buf) == -1) {
                    err(1, "readlink(%s)", path);
                }
                printf(" -> %s", buf);
            }
        }

        printf("\n");
    }
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

    int ignore;
    (void)getbsize(&ignore, &opt.blocksize);

    if (opt.blocks_kb)
        opt.blocksize = 1024;

    ls(files, files_len);
}
