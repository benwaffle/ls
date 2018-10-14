#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <locale.h>
#include <string.h>

#define HN_DECIMAL              0x01
#define HN_NOSPACE              0x02
#define HN_B                    0x04
#define HN_DIVISOR_1000         0x08

#define HN_GETSCALE             0x10
#define HN_AUTOSCALE            0x20

int
humanize_number(char *buf, size_t len, int64_t bytes,
    const char *suffix, int scale, int flags)
{
	const char *prefixes, *sep;
	int	b, r, s1, s2, sign;
	int64_t	divisor, max, post = 1;
	size_t	i, baselen, maxscale;

	if(buf == NULL) return -1;
	if(suffix == NULL) return -1;
	if(scale < 0) return -1;

	if (flags & HN_DIVISOR_1000) {
		/* SI for decimal multiplies */
		divisor = 1000;
		if (flags & HN_B)
			prefixes = "B\0k\0M\0G\0T\0P\0E";
		else
			prefixes = "\0\0k\0M\0G\0T\0P\0E";
	} else {
		/*
		 * binary multiplies
		 * XXX IEC 60027-2 recommends Ki, Mi, Gi...
		 */
		divisor = 1024;
		if (flags & HN_B)
			prefixes = "B\0K\0M\0G\0T\0P\0E";
		else
			prefixes = "\0\0K\0M\0G\0T\0P\0E";
	}

#define	SCALE2PREFIX(scale)	(&prefixes[(scale) << 1])
	maxscale = 6;

	if ((size_t)scale > maxscale &&
	    (scale & (HN_AUTOSCALE | HN_GETSCALE)) == 0)
		return (-1);

	if (buf == NULL || suffix == NULL)
		return (-1);

	if (len > 0)
		buf[0] = '\0';
	if (bytes < 0) {
		sign = -1;
		baselen = 3;		/* sign, digit, prefix */
		if (-bytes < INT64_MAX / 100)
			bytes *= -100;
		else {
			bytes = -bytes;
			post = 100;
			baselen += 2;
		}
	} else {
		sign = 1;
		baselen = 2;		/* digit, prefix */
		if (bytes < INT64_MAX / 100)
			bytes *= 100;
		else {
			post = 100;
			baselen += 2;
		}
	}
	if (flags & HN_NOSPACE)
		sep = "";
	else {
		sep = " ";
		baselen++;
	}
	baselen += strlen(suffix);

	/* Check if enough room for `x y' + suffix + `\0' */
	if (len < baselen + 1)
		return (-1);

	if (scale & (HN_AUTOSCALE | HN_GETSCALE)) {
		/* See if there is additional columns can be used. */
		for (max = 100, i = len - baselen; i-- > 0;)
			max *= 10;

		/*
		 * Divide the number until it fits the given column.
		 * If there will be an overflow by the rounding below,
		 * divide once more.
		 */
		for (i = 0; bytes >= max - 50 && i < maxscale; i++)
			bytes /= divisor;

		if (scale & HN_GETSCALE) {
			return (int)i;
		}
	} else
		for (i = 0; i < (size_t)scale && i < maxscale; i++)
			bytes /= divisor;
	bytes *= post;

	/* If a value <= 9.9 after rounding and ... */
	if (bytes < 995 && i > 0 && flags & HN_DECIMAL) {
		/* baselen + \0 + .N */
		if (len < baselen + 1 + 2)
			return (-1);
		b = ((int)bytes + 5) / 10;
		s1 = b / 10;
		s2 = b % 10;
		r = snprintf(buf, len, "%d%s%d%s%s%s",
		    sign * s1, localeconv()->decimal_point, s2,
		    sep, SCALE2PREFIX(i), suffix);
	} else
		r = snprintf(buf, len, "%" PRId64 "%s%s%s",
		    sign * ((bytes + 50) / 100),
		    sep, SCALE2PREFIX(i), suffix);

	return (r);
}
