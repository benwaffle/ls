#include <sys/stat.h>

#include <err.h>
#include <string.h>
#include <time.h>

#include "compare.h"
#include "opt.h"

/**
 * Compare two FTSENTs lexicographically
 */
int cmp_alpha(const FTSENT **a, const FTSENT **b) {
	return strcmp((*a)->fts_name, (*b)->fts_name);
}

/**
 * Compare two FTSENTs chronologically
 */
int cmp_time(const FTSENT **a, const FTSENT **b) {
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

	if (at > bt)
		return -1;
	if (at == bt)
		return 0;
	return 1;
}

/**
 * Compare two FTSENTs by file size
 */
int cmp_size(const FTSENT **a, const FTSENT **b) {
	off_t asz, bsz;

	asz = (*a)->fts_statp->st_size;
	bsz = (*b)->fts_statp->st_size;

	if (asz > bsz)
		return -1;
	if (asz == bsz)
		return 0;
	return 1;
}

/**
 * The main comparison function that is passed to fts_open(3). This delegates
 * to a comparison function above and handles fallbacks and reverse sorting.
 */
int main_compare(const FTSENT **a, const FTSENT **b) {
	int res;

	if (opt.sort == SIZE) {
		res = cmp_size(a, b);
	} else if (opt.sort == TIME) {
		res = cmp_time(a, b);
		if (res == 0)
			res = cmp_alpha(a, b);
	} else if (opt.sort == ALPHABETICAL) {
		res = cmp_alpha(a, b);
	} else {
		errx(1, "invalid sort function");
	}

	if (opt.sort_reverse)
		res *= -1;
	return res;
}
