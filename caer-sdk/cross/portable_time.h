#ifndef CAER_SDK_PORTABLE_TIME_H_
#define CAER_SDK_PORTABLE_TIME_H_

#ifdef __cplusplus

#	include <cstdlib>
#	include <ctime>

#else

#	include <stdbool.h>
#	include <stdlib.h>
#	include <time.h>

#endif

#ifdef __cplusplus
extern "C" {
#endif

bool portable_clock_gettime_monotonic(struct timespec *monoTime);
bool portable_clock_gettime_realtime(struct timespec *realTime);
struct tm portable_clock_localtime(void);

#ifdef __cplusplus
}
#endif

#endif /* CAER_SDK_PORTABLE_TIME_H_ */
