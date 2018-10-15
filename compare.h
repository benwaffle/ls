#ifndef _COMPARE_H_
#define _COMPARE_H_

#include <fts.h>

int cmp_alpha(const FTSENT **a, const FTSENT **b);
int cmp_time(const FTSENT **a, const FTSENT **b);
int cmp_size(const FTSENT **a, const FTSENT **b);
int main_compare(const FTSENT **a, const FTSENT **b);

#endif
