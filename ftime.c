#include "ftime.h"
#include <time.h>

static inline time_t __fly_abssec(time_t t);
__fly_static size_t __fly_time_format(char *buffer, size_t maxlen, const struct tm *timeptr);
__fly_static time_t __fly_time_diffsec(struct tm *t1, struct tm *t2);

static inline time_t __fly_abssec(time_t t)
{
	return t < 0 ? -1*t : t;
}

__fly_static time_t __fly_time_diffsec(struct tm *t1, struct tm *t2)
{

#define __FLY_MIN_SEC			(60)
#define __FLY_HOUR_SEC			(60*__FLY_MIN_SEC)
	time_t t1_sec, t2_sec, diff_sec;

	t1_sec = mktime(t1);
	t2_sec = mktime(t2);

	if (t1_sec == (time_t) -1 || t2_sec == (time_t) -1)
		return (time_t) -1;

	diff_sec = t1_sec - t2_sec;
	return diff_sec;
}

__fly_static size_t __fly_time_format(char *buffer, size_t maxlen, const struct tm *tptr)
{
	return strftime(buffer, maxlen, "%F %T", tptr);
}

int fly_logtime(char *buffer, int bufsize, fly_time_t *t)
{
	struct tm gm, local;
	char tformat_buf[FLY_TFORMAT_LEN];
	time_t diff_sec;
	bool minus;
	struct {
		int H;
		int M;
		int S;
	} __fly_hms;

	if (gmtime_r((const time_t *) &t->tv_sec, &gm) == NULL)
		return -1;
	if (localtime_r((const time_t *) &t->tv_sec, &local) == NULL)
		return -1;

	diff_sec = __fly_time_diffsec(&gm, &local);
	if (diff_sec == (time_t) -1)
		return -1;

	minus = diff_sec < 0 ? true : false;
	__fly_hms.H = __fly_abssec(diff_sec) / __FLY_HOUR_SEC;
	__fly_hms.M = __fly_abssec(diff_sec) % __FLY_HOUR_SEC;
	__fly_hms.S = __fly_abssec(diff_sec) % __FLY_MIN_SEC;

	if (__fly_time_format(tformat_buf, FLY_TFORMAT_LEN, &gm) == 0)
		return -1;

	return snprintf(buffer, bufsize, "%s [UTC%s%d:%s%d]", tformat_buf, minus ? "+" : "-", __fly_hms.H, __fly_hms.M < 10 ? "0" : "", __fly_hms.M);
}

int fly_imt_fixdate(char *buf, size_t buflen, time_t *time)
{
	size_t len;
	struct tm gmtm;

	if (gmtime_r(time, &gmtm) == NULL)
		return -1;

	len = strftime(buf, buflen, FLY_IMT_FIXDATE_FORMAT, &gmtm);
	if (len == 0)
		return -1;
	buf[buflen] = '\0';
	return 0;
}

int fly_cmp_imt_fixdate(char *t1, __unused size_t t1_len, char *t2, __unused size_t t2_len)
{
	struct tm t1_tm, t2_tm;
	time_t t1_time, t2_time;

	if (strptime(t1, FLY_IMT_FIXDATE_FORMAT, &t1_tm) == NULL)
		return -1;
	if (strptime(t2, FLY_IMT_FIXDATE_FORMAT, &t2_tm) == NULL)
		return -1;

	t1_time = mktime(&t1_tm);
	t2_time = mktime(&t2_tm);

	if (t1_time == (time_t) -1 || t2_time == (time_t) -1)
		return -1;

	return t1_time >= t2_time ? 1 : 0;
}
