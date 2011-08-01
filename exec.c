#define _LARGEFILE64_SOURCE     /* required for GLIBC to enable stat64 and friends */
#include <sys/types.h>
#include <regex.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "error.h"
#include "utils.h"
#include "mem.h"
#include "term.h"
#include "my_pty.h"
#include "globals.h"
#include "ui.h"


int start_tail(char *filename, char retry_open, char follow_filename, int initial_tail, int *pipefd)
{
	pid_t pid;

	/* start child process */
	if ((pid = fork()) == 0)
	{
		char *pars[16], *posix_version = NULL;
		int npars = 0;
		char nlines_buffer[32];

		setpgid(0,0);

		setup_for_childproc(pipefd[1], 0, "dumb");

		/* create command for take last n lines & follow and start tail */

		/* the command to start */
		pars[npars++] = tail;

		/* Linux' tail has the --retry option, but not all
		 * other UNIX'es have this, so I implemented it
		 * myself
		 */
		if (retry_open)
		{
			int rc;
			struct stat64 buf;

			for(;;)
			{
				rc = stat64(filename, &buf);
				if (rc == -1)
				{
					if (errno != ENOENT)
					{
						fprintf(stderr, "Error while looking for file %s: %d\n", filename, errno);
						exit(EXIT_FAILURE);
					}
				}
				else if (rc == 0)
					break;

				usleep(WAIT_FOR_FILE_DELAY * 1000);
			}

#if defined(linux) || defined(__CYGWIN__) || defined(__GNU__)
			pars[npars++] = "--retry";
#endif
		}

		/* get the posix compliance level */
		posix_version = getenv("_POSIX2_VERSION");

		/* follow filename is only supported on *BSD and Linux */
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(linux) || defined(__CYGWIN__) || defined(__APPLE__) || defined(__GNU__)
		if (follow_filename)
		{
#if defined(linux) || defined(__CYGWIN__) || defined(__GNU__)
			pars[npars++] = "--follow=name";
#elif defined(__OpenBSD__)
			pars[npars++] = "-f";
#else
			pars[npars++] = "-F";
#endif
			/* number of lines to tail initially */
			pars[npars++] = "-n";
			snprintf(nlines_buffer, sizeof(nlines_buffer), "%d", initial_tail);
			pars[npars++] = nlines_buffer;
		}
		else
#endif
		{
#if !defined(linux) && !defined(__CYGWIN__) && !defined(__GNU__)
			if (follow_filename && gnu_tail)
				pars[npars++] = "--follow=name";
#endif

			/* check the posix compliance level */
			if ((posix_version && atoi(posix_version) >= 200112) || posix_tail == MY_TRUE)
			{
				pars[npars++] = "-f";
				pars[npars++] = "-n";
				snprintf(nlines_buffer, sizeof(nlines_buffer), "%d", initial_tail);
				pars[npars++] = nlines_buffer;
			}
			else
			{
				/* number of lines to tail initially and 'follow file' ('f') */
				snprintf(nlines_buffer, sizeof(nlines_buffer), "-%dlf", initial_tail);
				pars[npars++] = nlines_buffer;
			}
		}

		/* add the filename to monitor */
		pars[npars++] = filename;
		pars[npars] = NULL;

		/* run tail! */
		if (-1 == execvp(pars[0], pars)) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error while starting process %s.\n", pars[0]);

		/* if execlp returns, an error occured */
		error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "An error occured while starting process %s!\n", pars[0]);
	}

	return pid;
}

void start_proc_signal_handler(int sig)
{
	if (sig != SIGTERM) error_popup("Signal handler(2)", -1, "Unexpected signal %d.\n", sig);

	stop_process(tail_proc);

	exit(1);
}

int start_proc(proginfo *cur, int initial_tail)
{
	cur -> n_runs++;

	if (cur -> wt == WT_COMMAND)
	{
		int fd_master, fd_slave;

		/* allocate pseudo-tty & fork*/
		cur -> pid = get_pty_and_fork(&fd_master, &fd_slave);
		if (-1 == cur -> pid) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "An error occured while invoking get_pty_and_fork.\n");

		/* child? */
		if (cur -> pid == 0)
		{
			setpgid(0,0);

			/* reset signal handler for SIGTERM */
			signal(SIGTERM, SIG_DFL);

			/* sleep if requested and only when 2nd or 3d (etc.) execution time */
			if (cur -> restart.restart && cur -> restart.first == 0)
				sleep(cur -> restart.restart);

			/* connect slave-fd to stdin/out/err */
			setup_for_childproc(fd_slave, 1, term_t_to_string(cur -> cdef.term_emul));

			/* start process */
			if (-1 == execlp(shell, shell, "-c", cur -> filename, (void *)NULL)) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error while starting \"%s -c '%s'\".\n", shell, cur -> filename);

			/* if execlp returns, an error occured */
			error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error while starting process %s!\n", shell);
		}

#if defined(sun) || defined(__sun) || defined(AIX) || defined(_HPUX_SOURCE) || defined(OSF1) || defined(scoos)
		/* these platforms only have the slave-fd available in the childprocess
		 *                  * so don't try to close it as the parent process doesn't have it
		 *
		 */
#else
		if (myclose(fd_slave) == -1) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "An error occured while closing the slave fd (pseudo tty, fd %d)\n", fd_slave);
#endif

		/* remember master-fd (we'll read from that one) */
		cur -> fd = fd_master;
		cur -> wfd = fd_master;

		/* next time, sleep */
		cur -> restart.first = 0;
	}
	else if (cur -> wt == WT_FILE)
	{
		int pipefd[2];

		/* create a pipe, will be to child-process */
		if (-1 == pipe(pipefd)) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error creating pipe.\n");

		if (cur -> check_interval)
		{
			/* start the process that will check every 'interval' seconds
			 * if a matching file with a more recent mod-time is available
			 */
			cur -> pid = fork();
			if (cur -> pid == 0)
			{
				char *cur_file = NULL, *new_file;

				setpgid(0,0);

				signal(SIGTERM, start_proc_signal_handler);

				for(;;)
				{
					/* find (new) file */
					new_file = find_most_recent_file(cur -> filename, cur_file);

					/* if find_most_recent_file returned NOT null, a file was found
					 * which is more recent
					 */
					if (new_file != NULL)
					{
						/* if there was a previous file, see if it is different
						 * from the new filename. this should always be the case!
						 */
						if (cur_file && strcmp(new_file, cur_file) != 0)
						{
							stop_process(tail_proc);
						}

						/* remember new filename */
						myfree(cur_file);
						cur_file = new_file;

						/* and start a proc process */
						tail_proc = start_tail(cur_file, cur -> retry_open, cur -> follow_filename, initial_tail, pipefd);
						if (tail_proc == -1)
						{
							break;
						}
					}
					else
					{
						/* LOG("no file found for pattern %s\n", cur -> filename); */
					}

					sleep(cur -> check_interval);
				}

				/* LOG("stopped checking for file pattern %s\n", cur -> filename); */

				exit(1);
			}
		}
		else
		{
			cur -> pid = start_tail(cur -> filename, cur -> retry_open, cur -> follow_filename, initial_tail, pipefd);
		}

		cur -> fd = pipefd[0];
		cur -> wfd = pipefd[1];
	}

	if (cur -> pid > -1)
		return 0;

	return -1;
}

int execute_program(char *execute, char bg)
{
	int status;
	pid_t child;

	if (bg)
	{
		/* to prevent meltdowns, only a limited number of
		 * processes can be executed
		 */
		if (n_children >= MAX_N_SPAWNED_PROCESSES)
			return 0;
	}
	else
		endwin();

	child = fork();
	if (child == 0)
	{
		setpgid(0,0);

		if (bg)
			setup_for_childproc(open_null(), 1, "dumb");

		/* start process */
		if (-1 == execlp(shell, shell, "-c", execute, (void *)NULL)) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error while starting \"%s -c '%s'\".\n", execute);

		/* if execlp returns, an error occured */
		error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error while starting process!\n");
	}
	else if (child == -1)
	{
		error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Failed to fork child process.\n");
	}

	if (bg)
	{
		/* remember this childprocess: we'll see if it has
		 * died in the main-loop
		 */
		children_list[n_children++] = child;
	}
	else
	{
		/* wait for the childprocess to exit */
		if (waitpid(child, &status, 0) == -1)
			error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error while waiting for process to exit.\n");

		/* restore (n)curses */
		mydoupdate();
	}

	return 0;
}

void init_children_reaper(void)
{
	/* init list of pids to watch for exit */
	memset(children_list, 0x00, sizeof(children_list));
}

pid_t exec_with_pipe(char *command, int pipe_to_proc[], int pipe_from_proc[])
{
	pid_t pid = -1;

	if (pipe(pipe_to_proc) == -1)
		error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error creating pipe.\n");

	if (pipe(pipe_from_proc) == -1)
		error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error creating pipe.\n");

	if ((pid = fork()) == -1)
		error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "fork() failed.\n");

	if (pid == 0)
	{
		myclose(0);
		if (mydup(pipe_to_proc[0]) == -1)   error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "dup() failed.\n");
		myclose(pipe_to_proc[1]); /* will not write to itself, only parent writes to it */
		myclose(1);
		myclose(2);
		if (mydup(pipe_from_proc[1]) == -1) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "dup() failed.\n");
		if (mydup(pipe_from_proc[1]) == -1) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "dup() failed.\n");
		myclose(pipe_from_proc[0]);

		/* start process */
/*		if (-1 == execlp(shell, shell, "-c", command, (void *)NULL)) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "execlp of %s failed\n", command); */
		if (-1 == execlp(command, command, (void *)NULL)) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error while starting '%s'.\n", command);

		/* if execlp returns, an error occured */
		error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error while starting process '%s'.\n", command);
	}

	return pid;
}

pid_t exec_with_pty(char *command, int *fd)
{
	int fd_master = -1, fd_slave = -1;
	pid_t pid = get_pty_and_fork(&fd_master, &fd_slave);
	if (-1 == pid) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "An error occured while invoking get_pty_and_fork.\n");

	if (pid == 0)
	{
		setpgid(0,0);

		myclose(fd_master);

		/* connect slave-fd to stdin/out/err */
		setup_for_childproc(fd_slave, 1, "dumb");

		/* start process */
		if (-1 == execlp(command, command, (void *)NULL)) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error while starting '%s'.\n", command);
	}

	*fd = fd_master;

#if defined(sun) || defined(__sun) || defined(AIX) || defined(_HPUX_SOURCE) || defined(OSF1) || defined(scoos)
	/* see start_proc */
#else
	if (myclose(fd_slave) == -1) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Error closing slave-fd (pseudo tty, fd %d)\n", fd_slave);
#endif

	return pid;
}

void exec_script(script *pscript)
{
	if (pscript -> pid == 0)
	{
		int to[2], from[2];

		pscript -> pid = exec_with_pipe(pscript -> script, to, from);
		pscript -> fd_r = from[0];
		pscript -> fd_w = to[1];
/*
		int fd;

		pscript -> pid = exec_with_pty(pscript -> script, &fd);
		pscript -> fd_r =
		pscript -> fd_w = fd;
	*/
	}
}
