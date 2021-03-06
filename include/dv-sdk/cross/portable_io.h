#ifndef DV_SDK_PORTABLE_IO_H_
#define DV_SDK_PORTABLE_IO_H_

#ifdef __cplusplus

#	include <cstdlib>

#else

#	include <stdlib.h>

#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Make PATH_MAX available by including the right headers.
 * This makes it easier to work cross-platform, especially
 * on MacOS X or Linux where this is in a different file.
 */
#include <limits.h>

#if defined(__linux__)
#	include <linux/limits.h>
#endif

#if defined(__APPLE__)
#	include <sys/syslimits.h>
#endif

#ifndef PATH_MAX
#	error "No PATH_MAX defined."
#endif

/**
 * Fully resolve and clean up a (relative) file path.
 * What can be done depends on OS support.
 * Remember to free() the returned string after use!
 *
 * @param path a (relative) file path.
 * @return the absolute, clean file path.
 */
char *portable_realpath(const char *path);

/**
 * Synchronize a file to storage (flush all changes).
 *
 * @param fd file descroptor.
 * @return zero on success, -1 on error (errno is set).
 */
int portable_fsync(int fd);

/**
 * Get the user's home directory path as a string.
 * Returned string is a dynamically allocated copy,
 * always remember to free() it to avoid a memory leak.
 *
 * @return string containing user home directory path.
 *         Always remember to free() this!
 */
char *portable_get_user_home_directory(void);

/**
 * Get the current executable's location as a string.
 * Returned string is a dynamically allocated copy,
 * always remember to free() it to avoid a memory leak.
 *
 * @return string containing current executable path.
 *         Always remember to free() this!
 */
char *portable_get_executable_location(void);

#ifdef __cplusplus
}
#endif

#endif /* DV_SDK_PORTABLE_IO_H_ */
