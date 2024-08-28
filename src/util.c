#include "util.h"

#ifdef __IMPL_BASENAME
const char *__basename(const char *path)
{
	const char *p = strrchr(path, '/');
	return p != NULL ? p + 1 : path;
}
#endif

#ifdef __IMPL_GET_TIME
int __get_time(struct timespec *ts)
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL))
	{
		return -1;
	}
	ts->tv_sec = tv.tv_sec;
	ts->tv_nsec = tv.tv_usec * 1000L;
	return 0;
}
#endif
