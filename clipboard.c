/* GPLv2 applies
 * SVN revision: $Revision$
 * (C) 2006-2014 by folkert@vanheusden.com
 */
#define _LARGEFILE64_SOURCE
#include <fcntl.h>
#include <signal.h>
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
#include "clipboard.h"

char *clipboard = "/usr/bin/" CLIPBOARD_NAME;

void send_to_clipboard_binary(char *what)
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
		int loop;

		for(loop=0; loop<1024; loop++)
		{
			if (fds[0] != loop)
				close(loop);
		}

		signal(SIGHUP, SIG_DFL);

		if (dup(fds[0]) == -1)
			error_exit(TRUE, TRUE, "dup() failed\n");

		setsid();
#ifndef __minix
                setpgid(0, 0);
#endif

		if (execl(clipboard, clipboard, NULL) == -1)
			error_exit(TRUE, FALSE, "execl of %s failed\n", clipboard);

		exit(1);
	}

	WRITE(fds[1], what, strlen(what), CLIPBOARD_NAME);

	close(fds[1]);
	close(fds[0]);
}

void send_to_clipboard(buffer *pb)
{
	if (file_exist(clipboard) == -1)
		error_popup("Copy to clipboard", -1, CLIPBOARD_NAME " program not found");
	else if (getenv("DISPLAY") == NULL)
		error_popup("Copy to clipboard", -1, "DISPLAY environment variable not set");
	else
	{
		char *data = NULL;
		int len_out = 0;
		int loop = 0;
		NEWWIN *mywin = create_popup(9, 40);

#ifdef __APPLE__
		win_header(mywin, "Copy buffer to clipboard");
#else
		win_header(mywin, "Copy buffer to X clipboard");
#endif
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

		send_to_clipboard_binary(data);

		free(data);

		mvwprintw(mywin -> win, 3, 2, "Finished!");
		mvwprintw(mywin -> win, 4, 2, "Press any key to continue...");
		mydoupdate();

		(void)wait_for_keypress(-1, 0, mywin, 0);

		delete_popup(mywin);
	}
}
