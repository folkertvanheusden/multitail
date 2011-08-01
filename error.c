#define _LARGEFILE64_SOURCE     /* required for GLIBC to enable stat64 and friends */
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <regex.h>
#if defined(__GLIBC__)
#include <execinfo.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "utils.h"
#include "version.h"

void error_exit(char *file, const char *function, int line, char *format, ...)
{
	va_list ap;

	(void)endwin();

	fprintf(stderr, version_str, VERSION);
	fprintf(stderr, "\n\n");

	fprintf(stderr,"A problem occured at line %d in function %s (from file %s):\n\n", line, function, file);
	va_start(ap, format);
	(void)vfprintf(stderr, format, ap);
	va_end(ap);
	if (errno) fprintf(stderr, "\nerrno variable (if applicable): %d which means %s\n\n", errno, strerror(errno));

	fprintf(stderr, "\nBinary build at %s %s\n", __DATE__, __TIME__);

	dump_mem(SIGHUP);

	fflush(NULL);

	(void)kill(0, SIGTERM); /* terminate every process in the process group of the current process */

	exit(EXIT_FAILURE);
}

