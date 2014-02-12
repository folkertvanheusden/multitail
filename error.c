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

void error_exit_(BOOL show_errno, BOOL show_st, char *file, const char *function, int line, char *format, ...)
{
	int e = errno;
	va_list ap;
#if defined(__GLIBC__)
	int index;
        void *trace[128];
        int trace_size = backtrace(trace, 128);
        char **messages = backtrace_symbols(trace, trace_size);
#endif

	(void)endwin();

	fprintf(stderr, version_str, VERSION);
	fprintf(stderr, "\n\n");

	fprintf(stderr, "The following problem occured:\n");
	fprintf(stderr, "-----------------------------\n");
	va_start(ap, format);
	(void)vfprintf(stderr, format, ap);
	va_end(ap);

	if (show_errno || show_st)
		fprintf(stderr, "If this is a bug, please report the following information:\n");

	if (e && show_errno)
		fprintf(stderr, "errno variable was then: %d which means \"%s\"\n", e, strerror(e));

	if (show_st)
	{
#if defined(__GLIBC__)
		fprintf(stderr, "Execution path:\n");
		for(index=0; index<trace_size; ++index)
			fprintf(stderr, "\t%d %s\n", index, messages[index]);
#endif
	}

	fflush(NULL);

	(void)kill(0, SIGTERM); /* terminate every process in the process group of the current process */

	exit(EXIT_FAILURE);
}

