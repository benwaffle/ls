#ifndef _COMPARE_H_
#define _COMPARE_H_

#include <fts.h>

int cmp_alpha(const FTSENT **, const FTSENT **);
int cmp_time(const FTSENT **, const FTSENT **);
int cmp_size(const FTSENT **, const FTSENT **);
int main_compare(const FTSENT **, const FTSENT **);

#endif
