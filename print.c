#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "opt.h"
#include "print.h"

const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void get_print_data(FTSENT *ent) {
	struct passwd *user;
	struct group *group;
	struct tm *time;
	struct stat *st = ent->fts_statp;
	print_data *data = calloc(1, sizeof(print_data));
	ent->fts_pointer = data;

	snprintf(data->inode, sizeof data->inode, "%llu",
	         (long long unsigned)st->st_ino);

	long long block_bytes = st->st_blocks * 512;
	if (opt.humanize) {
		// size=5, because 999B is 4 chars, and 1000B = 1.0K is 4 chars
		if (humanize_number(data->blocks, 5, block_bytes, "",
		                    HN_AUTOSCALE,
		                    HN_DECIMAL | HN_B | HN_NOSPACE) == -1)
			err(1, "humanize_number(%lld)", block_bytes);
	} else {
		snprintf(data->blocks, sizeof data->blocks, "%lld",
		         (long long)ceil(block_bytes / (double)opt.blocksize));
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
		if (humanize_number(data->size, 5, st->st_size, "",
		                    HN_AUTOSCALE,
		                    HN_DECIMAL | HN_B | HN_NOSPACE) == -1)
			err(1, "humanize_number(%lld)", (long long)st->st_size);
	} else {
		// cast to potentially larger size for compatibility with more
		// platforms
		snprintf(data->size, sizeof data->size, "%lld",
		         (long long)st->st_size);
	}

	if (opt.time == STATUS_CHANGED)
		time = localtime(&st->st_ctime);
	else if (opt.time == LAST_MODIFIED)
		time = localtime(&st->st_mtime);
	else
		time = localtime(&st->st_atime);

	snprintf(data->time, sizeof data->time, "%s %2d %02d:%02d ",
	         months[time->tm_mon], time->tm_mday, time->tm_hour,
	         time->tm_min);

	data->filename = strdup(ent->fts_name);
	for (char *c = data->filename; *c; ++c)
		if (opt.hide_nonprintable && !isprint((int)*c))
			*c = '?';

	if (S_ISDIR(st->st_mode))
		data->mode_char = '/';
	else if (S_ISLNK(st->st_mode))
		data->mode_char = '@';
	else if (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
		data->mode_char = '*';
#ifndef LINUX
	else if (S_ISWHT(st->st_mode))
		data->mode_char = '%';
#endif
	else if (S_ISSOCK(st->st_mode))
		data->mode_char = '=';
	else if (S_ISFIFO(st->st_mode))
		data->mode_char = '|';

	if (ent->fts_info == FTS_SL || ent->fts_info == FTS_SLNONE) {
		char path[PATH_MAX] = {0};
		snprintf(path, sizeof path, "%s/%s", ent->fts_path,
		         ent->fts_accpath);

		if (readlink(path, data->sym_target, sizeof data->sym_target) ==
		    -1) {
			err(1, "readlink(%s)", path);
		}
	}
}

void print_all(FTSENT *children) {
	unsigned max_inode_len, max_block_len, max_nlink_len, max_user_len,
	    max_group_len, max_size_len, max_major_len, max_minor_len,
	    max_time_len;
	long long block_total, size_total;

	max_inode_len = max_block_len = max_nlink_len = max_user_len =
	    max_group_len = max_size_len = max_major_len = max_minor_len =
	        max_time_len = 0;
	block_total = size_total = 0;

	for (FTSENT *cur = children; cur; cur = cur->fts_link) {
		if (opt.go_into_dirs &&
		    (cur->fts_name[0] == '.' && opt.filter == NORMAL))
			continue;

		max_inode_len = MAX(max_inode_len, strlen(DATA(cur)->inode));
		max_block_len = MAX(max_block_len, strlen(DATA(cur)->blocks));
		max_nlink_len = MAX(max_nlink_len, strlen(DATA(cur)->nlink));
		max_user_len = MAX(max_user_len, strlen(DATA(cur)->user));
		max_group_len = MAX(max_group_len, strlen(DATA(cur)->group));
		max_time_len = MAX(max_time_len, strlen(DATA(cur)->time));

		if (opt.humanize)
			size_total += cur->fts_statp->st_size;
		else
			block_total += cur->fts_statp->st_blocks;

		if (S_ISCHR(cur->fts_statp->st_mode) || S_ISBLK(cur->fts_statp->st_mode)) {
			// we do this so the comma is aligned for all major, minor pairs
			unsigned major_len = (unsigned)floor(log10(DATA(cur)->major) + 1);
			unsigned minor_len = (unsigned)floor(log10(DATA(cur)->minor) + 1);
			max_major_len = MAX(max_major_len, major_len);
			max_minor_len = MAX(max_minor_len, minor_len);
			max_size_len = MAX(max_size_len, max_major_len + 2 + max_minor_len);
		} else {
			max_size_len = MAX(max_size_len, strlen(DATA(cur)->size));
		}
	}

	if ((opt.long_mode || opt.print_blocks) && isatty(STDOUT_FILENO)) {
		if (opt.humanize) {
			char buf[5];
			if (humanize_number(
			        buf, sizeof buf, size_total, "", HN_AUTOSCALE,
			        HN_DECIMAL | HN_B | HN_NOSPACE) == -1)
				err(1, "humanize_number(%lld)", size_total);
			printf("total %s\n", buf);
		} else {
			printf("total %lld\n", (long long)block_total);
		}
	}

	for (FTSENT *cur = children; cur; cur = cur->fts_link) {
		if (opt.go_into_dirs &&
		    (cur->fts_name[0] == '.' && opt.filter == NORMAL))
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

			printf("%-*s  %-*s  ", max_user_len, DATA(cur)->user,
			       max_group_len, DATA(cur)->group);

			if (S_ISCHR(cur->fts_statp->st_mode) || S_ISBLK(cur->fts_statp->st_mode)) {
				char buf[11 + 2 + 11] = {0};
				snprintf(buf, sizeof buf, "%*d, %*d",
				         max_major_len, DATA(cur)->major,
				         max_minor_len, DATA(cur)->minor);
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
				snprintf(path, sizeof path, "%s/%s",
				         cur->fts_path, cur->fts_accpath);

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
