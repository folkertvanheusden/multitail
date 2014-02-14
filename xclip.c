/* GPLv2 applies
 * SVN revision: $Revision$
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "gen.h"
#include "error.h"
#include "config.h"
#include "utils.h"

void send_to_xclip(const char *what)
{
	int fds[2] = { 0 };
	pid_t pid = -1;

	if (pipe(fds) == -1)
		error_exit(TRUE, "error creating pipe\n");

	pid = fork();
	if (pid == -1)
		error_exit(TRUE, "error forking\n");

	if (pid == 0)
	{
		char *no_arguments[] = { NULL };

		close(0);
		dup(fds[0]);

		close(1);
		open("/dev/null", O_RDWR);

		close(2);
		open("/dev/null", O_RDWR);

		if (execve(xclip, no_arguments, no_arguments) == -1)
			error_exit(TRUE, "execve of %s failed\n", xclip);

		exit(1);
	}

	WRITE(fds[1], what, strlen(what));

	close(fds[1]);
	close(fds[0]);
}
