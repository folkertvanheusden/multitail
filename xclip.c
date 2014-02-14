/* GPLv2 applies
 * SVN revision: $Revision$
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _LARGEFILE64_SOURCE
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mt.h"
#include "error.h"
#include "config.h"
#include "utils.h"
#include "term.h"
#include "ui.h"

char *xclip = "/usr/bin/xclip";

void send_to_xclip(char *what)
{
	int fds[2] = { 0 };
	pid_t pid = -1;

	if (pipe(fds) == -1)
		error_exit(TRUE, TRUE, "error creating pipe\n");

	pid = fork();
	if (pid == -1)
		error_exit(TRUE, TRUE, "error forking\n");

	if (pid == 0)
	{
		close(0);
		if (dup(fds[0]) == -1)
			error_exit(TRUE, TRUE, "dup() failed\n");

		close(1);
		open("/dev/null", O_RDWR);

		close(2);
		open("/dev/null", O_RDWR);

		if (execl(xclip, xclip, NULL) == -1)
			error_exit(TRUE, FALSE, "execve of %s failed\n", xclip);

		exit(1);
	}

	WRITE(fds[1], what, strlen(what), "xclip");

	close(fds[1]);
	close(fds[0]);
}

void send_to_clipboard(buffer *pb)
{
	if (file_exist(xclip) == -1)
		error_popup("Copy to clipboard", -1, "xclip program not found");
	else if (getenv("DISPLAY") == NULL)
		error_popup("Copy to clipboard", -1, "DISPLAY environment variable not set");
	else
	{
		char *data = NULL;
		int len_out = 0;
		int loop = 0;
		NEWWIN *mywin = create_popup(9, 40);

		win_header(mywin, "Copy buffer to X clipboard");
		mydoupdate();

		for(loop=0; loop<pb -> curpos; loop++)
		{
			int len = 0;

			if ((pb -> be)[loop].Bline == NULL)
				continue;

			len = strlen((pb -> be)[loop].Bline);

			data = (char *)realloc(data, len_out + len + 1);

			memcpy(&data[len_out], (pb -> be)[loop].Bline, len + 1);

			len_out += len;
		}

		send_to_xclip(data);

		free(data);

		mvwprintw(mywin -> win, 3, 2, "Finished!");
		mvwprintw(mywin -> win, 4, 2, "Press any key to continue...");
		mydoupdate();

		(void)wait_for_keypress(-1, 0, mywin, 0);

		delete_popup(mywin);
	}
}
