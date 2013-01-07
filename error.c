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

void error_exit_(char *file, const char *function, int line, char *format, ...)
{
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

	fprintf(stderr, "The following error occured:\n");
	fprintf(stderr, "---------------------------\n");
	va_start(ap, format);
	(void)vfprintf(stderr, format, ap);
	va_end(ap);

	fprintf(stderr, "\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "If this is a bug, please report the following information:\n");
	fprintf(stderr, "---------------------------------------------------------\n");
	fprintf(stderr, "This problem occured at line %d in function %s (from file %s):\n", line, function, file);
	if (errno) fprintf(stderr, "errno variable was then: %d which means \"%s\"\n", errno, strerror(errno));
	fprintf(stderr, "Binary build at %s %s\n", __DATE__, __TIME__);
#if defined(__GLIBC__)
        fprintf(stderr, "Execution path:\n");
        for(index=0; index<trace_size; ++index)
                fprintf(stderr, "\t%d %s\n", index, messages[index]);
#endif

	fflush(NULL);

	(void)kill(0, SIGTERM); /* terminate every process in the process group of the current process */

	exit(EXIT_FAILURE);
}

