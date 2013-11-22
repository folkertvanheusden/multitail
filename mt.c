#define _LARGEFILE64_SOURCE	/* required for GLIBC to enable stat64 and friends */
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <regex.h>
#if !defined(__APPLE__) && !defined(__CYGWIN__)
#include <search.h>
#endif
#include <math.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <glob.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#ifndef AIX
#include <sys/termios.h> /* needed on Solaris 8 */
#endif
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__GLIBC__) && ( __GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 2))
#include <sys/sysinfo.h>
#endif
/* syslog receive */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <wchar.h>
#include <wctype.h>

#include "mt.h"
#include "globals.h"
#include "error.h"
#include "mem.h"
#include "utils.h"
#include "scrollback.h"
#include "help.h"
#include "term.h"
#include "history.h"
#include "cv.h"
#include "selbox.h"
#include "stripstring.h"
#include "color.h"
#include "misc.h"
#include "ui.h"
#include "exec.h"
#include "diff.h"
#include "config.h"
#include "cmdline.h"

/* #define KEYB_DEBUG */

void LOG(char *s, ...)
{
#ifdef _DEBUG
	va_list ap;
	FILE *fh = fopen("log.log", "a+");
	if (!fh)
	{
		endwin();
		error_exit("error logging\n");
	}

	va_start(ap, s);
	vfprintf(fh, s, ap);
	va_end(ap);

	fclose(fh);
#endif
}

typedef void (*sh_t)(int);
void set_signal(int sig, sh_t func, char *signame)
{
	if (SIG_ERR == signal(sig, func)) error_exit("Setting of handler for signal %s failed.\n", signame);
}

void free_subentry(proginfo *entry)
{
	int loop;

	/* free up filters */
	for(loop=0; loop<entry -> n_strip; loop++)
		myfree((entry -> pstrip)[loop].del);
	myfree(entry -> pstrip);

	/* free all those allocated memory blocks */
	myfree(entry -> filename);
	myfree(entry -> cdef.field_del);
	if (entry -> status)
		delete_popup(entry -> status);
	if (entry -> data)
		delete_popup(entry -> data);

	/* free buffers for diff (if any) */
	if (entry -> restart.diff.bcur)
		delete_array(entry -> restart.diff.bcur, entry -> restart.diff.ncur);
	if (entry -> restart.diff.bprev)
		delete_array(entry -> restart.diff.bprev, entry -> restart.diff.nprev);

	/* delete regular expressions */
	for(loop=0; loop<entry -> n_re; loop++)
		free_re(&(entry -> pre)[loop]);
	myfree(entry -> pre);

	/* stop process */
	stop_process(entry -> pid);

	/* close pipe to (tail) process */
	myclose(entry -> fd);
	if (entry -> wfd != entry -> fd) myclose(entry -> wfd);
}

void buffer_replace_pi_pointers(int f_index, proginfo *org, proginfo *new)
{
	int loop;

	for(loop=0; loop<lb[f_index].curpos; loop++)
	{
		if ((lb[f_index].be)[loop].pi == org)
		{
			if (!new)
			{
				myfree((lb[f_index].be)[loop].Bline);
				(lb[f_index].be)[loop].Bline = NULL;
				(lb[f_index].be)[loop].pi = NULL;
			}
			else
			{
				(lb[f_index].be)[loop].pi = new;
			}
		}
	}
}

void delete_be_in_buffer(buffer *pb)
{
	int loop;

	for(loop=0; loop<pb -> curpos; loop++)
		myfree((pb -> be)[loop].Bline);
	myfree(pb -> be);
	pb ->  be = NULL;
	pb -> curpos = 0;
}

char delete_entry(int f_index, proginfo *sub)
{
	char delete_all = 0;

	/* no children? then sub must be pointing to current */
	if (pi[f_index].next == NULL)
	{
		sub = NULL;
	}

	/* stop the process(es) we're watching ('tail' most of the time) */
	if (sub == NULL) /* delete all? */
	{
		proginfo *cur = &pi[f_index];

		do
		{
			free_subentry(cur);
			cur = cur -> next;
		}
		while(cur);

		/* free the subwindows (if any) */
		cur = pi[f_index].next;
		while(cur)
		{
			proginfo *dummy = cur -> next;
			myfree(cur);
			cur = dummy;
		}

		delete_all = 1;
	}
	else
	{
		free_subentry(sub);
	}

	/* remove entry from array */
	if (sub == NULL) /* delete entry in the main array */
	{
		int n_to_move = (nfd - f_index) - 1;

		/* free buffers */
		delete_be_in_buffer(&lb[f_index]);

		if (n_to_move > 0)
		{
			int loop;

			/* update buffer proginfo-pointers */
			for(loop=f_index + 1; loop<nfd; loop++)
			{
				/* replace lb[f_index].pi -> pi[loop] met pi[loop-1] */
				buffer_replace_pi_pointers(loop, &pi[loop], &pi[loop - 1]);
			}

			/* prog info */
			memmove(&pi[f_index], &pi[f_index+1], sizeof(proginfo) * n_to_move);
			/* buffers */
			memmove(&lb[f_index], &lb[f_index+1], sizeof(buffer) * n_to_move);
		}

		nfd--;

		/*  shrink array */
		if (nfd == 0)	/* empty? */
		{
			myfree(pi);
			pi = NULL;
			myfree(lb);
			lb = NULL;
		}
		else		/* not empty, just shrink */
		{
			pi = (proginfo *)myrealloc(pi, nfd * sizeof(proginfo));
			lb = (buffer *)  myrealloc(lb, nfd * sizeof(buffer));
		}
	}
	else		/* delete sub */
	{
		if (sub != &pi[f_index])	/* not in main array? */
		{
			proginfo *parent = &pi[f_index];

			/* find parent of 'sub' */
			while (parent -> next != sub)
				parent = parent -> next;

			parent -> next = sub -> next;

			buffer_replace_pi_pointers(f_index, sub, NULL);

			myfree(sub);
		}
		else				/* main array, find next */
		{
			proginfo *oldnext = pi[f_index].next;
			/* first, delete the entries of that entry (which is in the array) */
			buffer_replace_pi_pointers(f_index, &pi[f_index], NULL);
			/* then, 'rename' the pointers (original address -> address in array)... */
			buffer_replace_pi_pointers(f_index, oldnext, &pi[f_index]);
			/* and move the object to the array */
			memmove(&pi[f_index], oldnext, sizeof(proginfo));
			/* free the obsolete entry */
			myfree(oldnext);
		}
	}

	return delete_all;
}

/** do_exit
 * - in:      int sig
 * - returns: nothing (doesn't return!)
 * this function is called when MultiTail receives the TERM-signal. it is also
 * called by, for example, wait_for_keypress when ^c is pressed. it stops all
 * child-processes, ends the curses-library and exits with SUCCESS status
 */
void do_exit(void)
{
	proginfo *cur;
	int loop;

	set_signal(SIGCHLD, SIG_IGN, "do_exit");

	/* kill tail processes */
	for(loop=0; loop<nfd; loop++)
	{
		cur = &pi[loop];

		do
		{
			int r_i;

			for(r_i = 0; r_i < cur -> n_redirect; r_i++)
			{
				if ((cur -> predir)[r_i].type != 0)
				{
					myclose((cur -> predir)[r_i].fd);

					if ((cur -> predir)[r_i].type == REDIRECTTO_PIPE_FILTERED || (cur -> predir)[r_i].type == REDIRECTTO_PIPE)
						stop_process((cur -> predir)[r_i].pid);
				}
			}

			myclose(cur -> fd);
			if (cur -> wfd != -1 && cur -> wfd != cur -> fd)
				myclose(cur -> wfd);

			if (cur -> pid != -1)
				stop_process(cur -> pid);

			cur = cur -> next;
		}
		while(cur);
	}

	/* kill convesion scripts */
	for(loop=0; loop<n_conversions; loop++)
	{
		int loop2;

		for(loop2=0; loop2<conversions[loop].n; loop2++)
		{
			if (conversions[loop].pcs[loop2].pid != 0)
			{
				stop_process(conversions[loop].pcs[loop2].pid);
			}
		}
	}

	/* kill colorscripts */
	for(loop=0; loop<n_cschemes; loop++)
	{
		if (cschemes[loop].color_script.pid)
			stop_process(cschemes[loop].color_script.pid);
	}

	if (set_title)
		gui_window_header("");

#ifdef _DEBUG
	/* free up as much as possible */
	clean_memory();

	/* to be able to check for memory leaks, (try to :-]) free up the whole
	 * array of windows and such. normally, don't do this: libc & kernel will
	 * take care of that
	 */
	dump_mem(SIGHUP);
#endif

	endwin();

	exit(EXIT_SUCCESS);
}

void signal_handler(int sig)
{
	set_signal(sig, SIG_IGN, "? from signal_handler (start)");

	switch(sig)
	{
		case SIGCHLD:
			need_died_procs_check = 1;
			break;

		case SIGINT:
		case SIGHUP:
		case SIGTERM:
			do_exit();
			break;

		case SIGUSR1:
			got_sigusr1 = 1;
			break;

		case SIGWINCH:
			terminal_changed = 1;
			break;

		default:
			error_popup("Signal handler", -1, "Unexpected signal %d received.", sig);
	}

	set_signal(sig, signal_handler, "? from signal_handler (exit)");
}

void select_display_start_and_end(char *string, char mode, int wrap_offset, int win_width, int *prt_start, int *prt_end, int *disp_end)
{
	int nbytes = strlen(string);

	*prt_start = 0;
	*prt_end = nbytes;
	*disp_end = nbytes;

	/* figure out what to display (because of linewraps and such) */
	if (mode == 'a')
	{
		/* default is everything */
		*disp_end = -1;
	}
	else if (mode == 'l')
	{
		*prt_start= min(wrap_offset, nbytes);
		*prt_end = min(nbytes, win_width + wrap_offset);
		*disp_end = min(win_width, nbytes - *prt_start);
	}
	else if (mode == 'r')
	{
		*prt_start = max(0, nbytes - win_width);
		*disp_end = win_width;
	}
	else if (mode == 'S')
	{
		*prt_start = find_char_offset(string, ':');
		if (*prt_start == -1)
			*prt_start = 0;
		*disp_end = nbytes - *prt_start;
	}
	else if (mode == 's')
	{
		if (nbytes > 16)
			*prt_start = find_char_offset(&string[16], ' ');
		else
			*prt_start = -1;

		if (*prt_start == -1)
			*prt_start = 0;

		*disp_end = nbytes - *prt_start;
	}
	else if (mode == 'o')
	{
		*prt_start = min(wrap_offset, nbytes);
		*disp_end = nbytes - *prt_start;
	}
}

int draw_tab(NEWWIN *win)
{
	if (tab_width)
	{
		int curx = getcurx(win -> win), loop;
		int move = (((curx / tab_width) + 1) * tab_width) - curx;

		for(loop=0; loop<move; loop++)
			waddch(win -> win, ' ');

		return move;
	}

	return 0;
}


char find_highlight_matches(regmatch_t *matches, char use_regex, int offset)
{
	int match_offset;

	for(match_offset=0; match_offset<MAX_N_RE_MATCHES; match_offset++)
	{
		char matching;

		if (matches[match_offset].rm_so == -1)
		{
			return 0;
		}

		matching = offset >= matches[match_offset].rm_so && offset < matches[match_offset].rm_eo;

		if ((use_regex == 'c' && matching) || (use_regex == 'C' && !matching))
		{
			return 1;
		}
	}

	return 0;
}

myattr_t * find_cmatches_index(color_offset_in_line *cmatches, int n_cmatches, mybool_t has_merge_colors, int offset)
{
	static myattr_t final_color;
	int cmatches_index = 0;
	int fg_composed = -1, bg_composed = -1;
	int attrs = -1;
	char first_set = 1;

	for(cmatches_index=0; cmatches_index<n_cmatches; cmatches_index++)
	{
		if (offset >= cmatches[cmatches_index].start && offset < cmatches[cmatches_index].end)
		{
			if (has_merge_colors == MY_FALSE)
			{
				return &cmatches[cmatches_index].attrs;
			}

			if (cmatches[cmatches_index].merge_color == MY_TRUE)
			{
				/* merge these colors */
				if (cmatches[cmatches_index].attrs.colorpair_index != -1)
				{
					int fg_fc, bg_fc;

					fg_fc = cp.fg_color[cmatches[cmatches_index].attrs.colorpair_index];
					bg_fc = cp.bg_color[cmatches[cmatches_index].attrs.colorpair_index];

					if (fg_fc != -1 && fg_composed == -1)
						fg_composed = fg_fc;
					if (bg_fc != -1 && bg_composed == -1)
						bg_composed = bg_fc;
				}

				if (cmatches[cmatches_index].attrs.attrs != -1)
				{
					if (attrs == -1)
						attrs = cmatches[cmatches_index].attrs.attrs;
					else
						attrs |= cmatches[cmatches_index].attrs.attrs;
				}
			}
			else if (first_set)
			{
				first_set = 0;

				fg_composed = cp.fg_color[cmatches[cmatches_index].attrs.colorpair_index];
				bg_composed = cp.bg_color[cmatches[cmatches_index].attrs.colorpair_index];
				attrs = cmatches[cmatches_index].attrs.attrs;
			}
		}
	}

	if (fg_composed != -1 || bg_composed != -1 || final_color.attrs != -1)
	{
		final_color.attrs = attrs;
		final_color.colorpair_index = find_or_init_colorpair(fg_composed, bg_composed, 1);

		return &final_color;
	}

	return NULL;
}

void gen_wordwrap_offsets(char *string, int start_offset, int end_offset, int win_width, int **offsets)
{
	int check_x = start_offset;
	int n_ww = 0;

	for(;;)
	{
		int max_len = wordwrapmaxlength < win_width ? wordwrapmaxlength : win_width - 2;

		check_x += win_width - 1;
		if (check_x >= end_offset)
			break;

		while(max_len >= 0 && (!isspace(string[check_x])) && check_x > 0)
		{
			check_x--;
			max_len--;
		}

		if (max_len >= 0)
		{
			*offsets = (int *)myrealloc(*offsets, (n_ww + 1) * sizeof(int));
			(*offsets)[n_ww++] = check_x;
		}
		else
		{
			check_x += wordwrapmaxlength;
		}
	}

	*offsets = (int *)myrealloc(*offsets, (n_ww + 1) * sizeof(int));
	(*offsets)[n_ww] = -1;
}

int count_utf_bytes(int c)
{
        if ((c & 0xe0) == 0xc0)
                return 2;
        else if ((c & 0xf0) == 0xe0)
                return 3;
        else if ((c & 0xf8) == 0xf0)
                return 4;

        return 1;
}

void do_color_print(proginfo *cur, char *use_string, int prt_start, int prt_end, int disp_end, color_offset_in_line *cmatches, int n_cmatches, mybool_t has_merge_colors, char start_reverse, regmatch_t *matches, int matching_regex, char use_regex, NEWWIN *win)
{
	int offset;
	myattr_t cdev = { -1, -1 };
	char default_reverse_state = 0;
	int disp_offset = 0;
	char highlight_part = toupper(use_regex) == 'C';
	char use_colorschemes = cur -> cdef.colorize == 'S' || cur -> cdef.colorize == 'T' || highlight_part;
	int *ww = NULL;
	int ww_offset = 0;

	if (global_highlight_str)
	{
		if (regexec(&global_highlight_re, use_string, 0, NULL, 0) == 0)
			default_reverse_state = 1;
	}

	if (cur -> line_wrap == 'w')
	{
		gen_wordwrap_offsets(use_string, prt_start, prt_end, getmaxx(win -> win), &ww);
	}

	/* print text */
	for(offset=prt_start; offset<prt_end;)
	{
		char re_inv = default_reverse_state;
		char is_control_or_extended_ascii = 0;
		myattr_t new_cdev = { -1, -1 };

		wchar_t wcur = 0;
		const char *dummy = &use_string[offset];

		/*int rc = */mbsrtowcs(&wcur, &dummy, 1, NULL);
		/* if (rc != 1)
			error_exit("> %d: %02x|%02x %02x %02x %02x", rc, *(dummy - 1), *dummy, *(dummy + 1), *(dummy + 2), *(dummy + 3)); */

		if (ww != NULL && offset == ww[ww_offset])
		{
			wprintw(win -> win, "\n");
			ww_offset++;

			if (iswspace(wcur))
			{
				offset++;

				continue;
			}
		}

		/* find things to colorize */
		if (use_colorschemes)
		{
			if (highlight_part && find_highlight_matches(matches, use_regex, offset))
			{
				re_inv = 1;
			}
			else
			{
				/* if there's a list of offsets where colors should be displayed, check if the
				 * current offset in the string (the one we're going to display) is somewhere
				 * in that list off offsets
				 */
				myattr_t *pa = find_cmatches_index(cmatches, n_cmatches, has_merge_colors, offset);

				if (pa != NULL)
					new_cdev = *pa;
			}
		}

		/* control-characters will be displayed as an inverse '.' */
		if (iswcntrl(wcur))
		{
			is_control_or_extended_ascii = 1;

			if (!iswspace(wcur))
				re_inv = 1;
		}

		if (re_inv)
			new_cdev.attrs = inverse_attrs;

		/* new color selected? then switch off previous color */
		if (cdev.colorpair_index != new_cdev.colorpair_index || cdev.attrs != new_cdev.attrs)
		{
			myattr_off(win, cdev);

			myattr_on(win, new_cdev);

			cdev = new_cdev;
		}

		if (!is_control_or_extended_ascii)
		{
			waddnwstr(win -> win, &wcur, 1);

			disp_offset++;
		}
		else
		{
			/* error_exit("> 126 %d: %02x %02x %02x", wcur, use_string[offset+0],  use_string[offset+1], use_string[offset+2]); */

			if (wcur == 9 && tab_width > 0)	/* TAB? */
			{
				disp_offset += draw_tab(win);
			}
			/* 13 (CR) is just silently ignored */
			else if (wcur != 13)
			{
				if (caret_notation)
				{
					wprintw(win -> win, "^%c", 'a' + wcur - 1);
					disp_offset++;
				}
				else
					waddch(win -> win, '.');

				disp_offset++;
			}
		}

		if (disp_offset >= disp_end && disp_end != -1)
			break;

		offset += count_utf_bytes(use_string[offset]);
	}

	if (prt_start == prt_end)       /* scrolled out line */
		wprintw(win -> win, "\n");

	if (cdev.attrs != -1)
		myattr_off(win, cdev);

	myfree(ww);
}

void color_print(int f_index, NEWWIN *win, proginfo *cur, char *string, regmatch_t *matches, int matching_regex, mybool_t force_to_winwidth, int start_at_offset, int end_at_offset, double ts, char show_window_nr)
{
	char reverse = 0;
	myattr_t cdev = { -1, -1 };
	int prt_start = 0, prt_end = 0, disp_end = 0;
	int mx = -1;
	char use_regex = 0;
	color_offset_in_line *cmatches = NULL;
	int n_cmatches = 0;
	mybool_t has_merge_colors = MY_FALSE;
	char *use_string = NULL;
	int x;

	/* stop if there's no window tou output too */
	if (!win)
		return;

	/* check if the cursor is not at the beginning of the line
	 * if it is not and it is also not at the most right position, move it
	 * to the left (when it is already at the right most position, it'll move
	 * to the left itself when emitting text)
	 */
	mx = getmaxx(win -> win);
	x = getcurx(win -> win);
	if ((x != 0 && x != mx) || !suppress_empty_lines)
		wprintw(win -> win, "\n");

	/* cur == NULL? Markerline! */
	if (IS_MARKERLINE(cur))
	{
		draw_marker_line(win, string, cur);

		return;
	}

	if (f_index >= 0)
	{
		/* add a window-id [xx] in front of each line */
		if (show_window_nr)
		{
			mx -= wprintw(win -> win, window_number, f_index);
		}

		/* add a subwindow-ID: [xx] in front of each line */
		if (show_subwindow_id || (show_window_nr && pi[f_index].next != NULL))
		{
			proginfo *ppi = &pi[f_index];
			int subwin_nr = 0;

			while(ppi != cur && ppi -> next != NULL)
			{
				subwin_nr++;
				ppi = ppi -> next;
			}

			mx -= wprintw(win -> win, subwindow_number, subwin_nr);
		}
	}

	/* add a timestamp in front of each line */
	if (cur -> add_timestamp)
	{
		char timestamp[1024];
		double_ts_to_str(ts, line_ts_format, timestamp, sizeof(timestamp));

		mx -= wprintw(win -> win, "%s ", timestamp);
	}

	/* add a label */
	LOG("Label\n");
	if (cur -> label != NULL && (cur -> label)[0])
	{
		mx -= wprintw(win -> win, "%s", cur -> label);
	}

	if (mx <= 0) mx = 4;

	/* had a matching regexp? */
	if (matching_regex != -1)
		use_regex = (cur -> pre)[matching_regex].use_regex;

	/* select color or generate list of offset for colors */
	if (cur -> cdef.colorize) /* no need to do this for b/w terminals */
	{
		/* choose_color not only generates a list of colors but if there are terminal-
		 * codes in that string which set a color those are stripped as well
		 * the stripping part should be moved to a seperate function
		 */
		cdev = choose_color(string, cur, &cmatches, &n_cmatches, &has_merge_colors, &use_string);

		/* if not using colorschemes (which can have more then one color
		 * per line), set the color
		 */
		if (cur -> cdef.colorize != 'S' && cur -> cdef.colorize != 'T')
		{
			myattr_on(win, cdev);

			if (cdev.attrs != -1 && cdev.attrs & A_REVERSE)
				reverse = 1;
		}
	}

	/* select start and end of string to display */
	LOG("lwo: %d, mx: %d, ps: %d, pe: %d, de: %d\n", cur -> line_wrap_offset, mx, prt_start, prt_end, disp_end);
	select_display_start_and_end(USE_IF_SET(use_string, string), force_to_winwidth == MY_TRUE?'l':cur -> line_wrap, cur -> line_wrap_offset, mx, &prt_start, &prt_end, &disp_end);

	if (prt_start == 0 && start_at_offset > 0)
	{
		prt_start = start_at_offset;
		prt_end = end_at_offset;
	}

	/* and display on terminal */
	LOG("ps: %d, pe: %d, de: %d\n", prt_start, prt_end, disp_end);
	LOG("%s\n", USE_IF_SET(use_string, string));
	do_color_print(cur, USE_IF_SET(use_string, string), prt_start, prt_end, disp_end, cmatches, n_cmatches, has_merge_colors, reverse, matches, matching_regex, use_regex, win);

	myattr_off(win, cdev);

	myfree(use_string);
	myfree(cmatches);
}

void check_filter_exec(char *cmd, char *matching_string)
{
	int str_index, par_len = strlen(matching_string);
	int cmd_len = strlen(cmd);
	char *command = mymalloc(cmd_len + 1/* cmd + space */
			+ 1 + (par_len * 2) + 1 + 1); /* "string"\0 */
	int loop;

	memcpy(command, cmd, cmd_len);
	str_index = cmd_len;
	command[str_index++] = ' ';
	command[str_index++] = '\"';
	for(loop=0; loop<par_len; loop++)
	{
		if (matching_string[loop] == '"' || matching_string[loop] == '`')
		{
			command[str_index++] = '\\';
		}

		command[str_index++] = matching_string[loop];
	}
	command[str_index++] = '\"';
	command[str_index] = 0x00;

	if (execute_program(command, 1) == -1)
		error_popup("Execute triggered by filter", -1, "Failed to execute command '%s'.", command);

	myfree(command);
}

/* check if the current line matches with a regular expression
   returns 0 if a regexp failed to execute
 */

/* this code can be optimized quit a bit but I wanted to get it to work quickly so I expanded everything (2006/03/28) */
char check_filter(proginfo *cur, char *string, regmatch_t **pmatch, char **error, int *matching_regex, char do_re, char *display)
{
	regmatch_t local_matches[MAX_N_RE_MATCHES];
	int loop;
	/* if no regexps, then always matches so it's the default */
	char re_exec_ok = 1;

	*error = NULL;
	if (pmatch)
		*pmatch = NULL;
	*matching_regex = -1;

	*display = 1;

	local_matches[1].rm_so = local_matches[1].rm_eo = -1; /* let the compiler stop complaining */

	/* do this regular expression? */
	for(loop=0; loop<cur -> n_re; loop++)
	{
		int rc;
		char cmd = (cur -> pre)[loop].use_regex;
		char invert = (cur -> pre)[loop].invert_regex;

		/* SKIP DISABLED REGEXPS */
		if (!cmd)
			continue;

		/* EXECUTE THE REGEXPS */
		if (pmatch)
		{
			if (*pmatch == NULL)
				*pmatch = (regmatch_t *)mymalloc(sizeof(regmatch_t) * MAX_N_RE_MATCHES);

			rc = regexec(&(cur -> pre)[loop].regex, string, MAX_N_RE_MATCHES, *pmatch, 0);
		}
		else
		{
			rc = regexec(&(cur -> pre)[loop].regex, string, MAX_N_RE_MATCHES, local_matches, 0);
		}

		/* IF ERROR, ABORT EVERYTHING */
		if (rc != 0 && rc != REG_NOMATCH)
		{
			*error = convert_regexp_error(rc, &(cur -> pre)[loop].regex);
			re_exec_ok = 0;
			break;
		}

		/* MATCHED? */
		if (rc == 0)
		{
			if (!invert)
			{
				*matching_regex = loop;

				if (cmd == 'm')
				{
					*display = 1;
					(cur -> pre)[loop].match_count++;
					return 1;
				}
				else if (cmd == 'v')
				{
					*display = 0;
					(cur -> pre)[loop].match_count++;
					return 1;
				}
				else if (cmd == 'x' && do_re)
				{
					check_filter_exec((cur -> pre)[loop].cmd, string);
					(cur -> pre)[loop].match_count++;
				}
				else if (cmd == 'X' && do_re)
				{
					regmatch_t *usep = USE_IF_SET(*pmatch, local_matches);
					int len = usep[1].rm_eo - usep[1].rm_so;
					char *dummy = (char *)mymalloc(len + 1);
					memcpy(dummy, &string[usep[1].rm_so], len);
					dummy[len] = 0x00;
					check_filter_exec((cur -> pre)[loop].cmd, dummy);
					(cur -> pre)[loop].match_count++;
					myfree(dummy);
				}
				else if (toupper(cmd) == 'B' && do_re)
				{
					beep();
					(cur -> pre)[loop].match_count++;
				}
			}
			else
			{
				if (cmd == 'm')
				{
					*display = 0;
				}
			}
		}
		else
		{
			myfree(*pmatch);
			*pmatch = NULL;

			if (invert)
			{
				*matching_regex = loop;

				if (cmd == 'm')
				{
					*display = 1;
					(cur -> pre)[loop].match_count++;
					return 1;
				}
				else if (cmd == 'v')
				{
					*display = 0;
					(cur -> pre)[loop].match_count++;
					return 1;
				}
				else if (cmd == 'x' && do_re)
				{
					check_filter_exec((cur -> pre)[loop].cmd, string);
					(cur -> pre)[loop].match_count++;
				}
				else if (cmd == 'X' && do_re)
				{
					int len = *pmatch ? (*pmatch)[1].rm_eo - (*pmatch)[1].rm_so : local_matches[1].rm_eo - local_matches[1].rm_so;
					char *dummy = (char *)mymalloc(len + 1);
					memcpy(dummy, &string[local_matches[1].rm_so], len);
					dummy[len] = 0x00;
					check_filter_exec((cur -> pre)[loop].cmd, dummy);
					(cur -> pre)[loop].match_count++;
					myfree(dummy);
				}
				else if (toupper(cmd) == 'B' && do_re)
				{
					beep();
					(cur -> pre)[loop].match_count++;
				}
			}
			else
			{
				if (cmd == 'm')
				{
					*display = 0;
				}
			}
		}
	}

	return re_exec_ok;
}

void do_print(int f_index, proginfo *cur, char *string, regmatch_t *matches, int matching_regex, double ts)
{
	/* check filter */
	/* if cur == NULL, then marker line: don't check against filter */
	if (IS_MARKERLINE(cur))
	{
		color_print(f_index, pi[f_index].data, cur, string, NULL, -1, MY_FALSE, 0, 0, ts, 0);
	}
	else
	{
		color_print(f_index, pi[f_index].data, cur, string, matches, matching_regex, MY_FALSE, 0, 0, ts, 0);
	}
}

char is_buffer_too_full(buffer *lb, int n_bytes_to_add)
{
	return (lb -> curpos >= lb -> maxnlines && lb -> maxnlines > 0) || ((lb -> curbytes + n_bytes_to_add) >= lb -> maxbytes && lb -> maxbytes > 0 && n_bytes_to_add < lb -> maxbytes);
}

void do_buffer(int f_index, proginfo *cur, char *string, char filter_match, dtime_t now)
{
	/* remember string */
	if (lb[f_index].bufferwhat == 'a' || filter_match == 1)
	{
		int cur_line;
		int line_len = 0;
		int move_index = 0, old_curpos = lb[f_index].curpos;
		int min_shrink = 0;

		if (string) line_len = strlen(string);

		if (is_buffer_too_full(&lb[f_index], line_len))
			min_shrink = default_min_shrink;

		/* remove enough lines untill there's room */
		if (lb[f_index].curpos > 0)
		{

			while(is_buffer_too_full(&lb[f_index], line_len) || min_shrink > 0)
			{
				/* delete oldest */
				if (lb[f_index].be[move_index].Bline)
				{
					lb[f_index].curbytes -= strlen(lb[f_index].be[move_index].Bline);
					myfree(lb[f_index].be[move_index].Bline);
				}

				move_index++;

				lb[f_index].curpos--;

				if (lb[f_index].curpos == 0)
					break;

				min_shrink--;
			}

		}

		if (move_index > 0)
		{
			/* move entries over the deleted one */
			memmove(&lb[f_index].be[0], &lb[f_index].be[move_index], (old_curpos - move_index) * sizeof(buffered_entry));
		}
		else
		{
			/* grow array */
			lb[f_index].be = (buffered_entry *)myrealloc(lb[f_index].be, sizeof(buffered_entry) * (lb[f_index].curpos + 1));
		}

		cur_line = lb[f_index].curpos++;

		/* add the logline itself */
		if (string)
		{
			lb[f_index].be[cur_line].Bline = mystrdup(string);
			lb[f_index].curbytes += line_len;
		}
		else
			lb[f_index].be[cur_line].Bline = NULL;

		/* remember pointer to subwindow (required for setting colors etc.) */
		lb[f_index].be[cur_line].pi = cur;

		/* remember when this string was buffered */
		lb[f_index].be[cur_line].ts = now;
	}
}

void do_redirect(redirect_t *predir, char *buffer, int nbytes, char add_lf)
{
	int failed = 0;

	if (predir -> type == REDIRECTTO_SOCKET_FILTERED || predir -> type == REDIRECTTO_SOCKET)
	{
		int msg_len = nbytes + 6;
		char *msg = (char *)mymalloc(msg_len + 1);;

		snprintf(msg, msg_len, "<%d>%s\n", predir -> prio_fac, buffer);

		if (sendto(predir -> fd, msg, msg_len, 0, (const struct sockaddr *)&(predir -> sai), sizeof(predir -> sai)) == -1)
			failed = 1;

		myfree(msg);
	}
	else
	{
		if (WRITE(predir -> fd, buffer, nbytes, "redirect") != nbytes)
			failed = 1;

		if (add_lf == MY_TRUE && WRITE(predir -> fd, "\n", 1, "redirect") != 1)
			failed = 1;
	}

	if (failed)
	{
		error_popup("Error redirecting output", HELP_REDIRECT_FAILED, "Redirecting the output failed: %s\nRedirection stopped.", strerror(errno));

		/* stop redirecting */
		myclose(predir -> fd);
		predir -> fd = -1;
		predir -> type = REDIRECTTO_NONE;
	}
}

void redirect(proginfo *cur, char *data, int n_bytes, mybool_t is_filtered)
{
	int r_i;

	for(r_i=0; r_i < cur -> n_redirect; r_i++)
	{
		if (is_filtered == MY_TRUE && ((cur -> predir)[r_i].type == REDIRECTTO_FILE_FILTERED ||
					(cur -> predir)[r_i].type == REDIRECTTO_PIPE_FILTERED || (cur -> predir)[r_i].type == REDIRECTTO_SOCKET_FILTERED))
		{
			do_redirect(&(cur -> predir)[r_i], data, n_bytes, 1);
		}
		else if (is_filtered == MY_FALSE && ((cur -> predir)[r_i].type == REDIRECTTO_FILE ||
					(cur -> predir)[r_i].type == REDIRECTTO_PIPE || (cur -> predir)[r_i].type == REDIRECTTO_SOCKET))
		{
			do_redirect(&(cur -> predir)[r_i], data, n_bytes, 0);
		}
	}
}

void delete_all_markerlines(void)
{
	int index;

	for(index=0; index<nfd; index++)
	{
		int loop, ndeleted = 0;
		buffer cur_lb;

		memset(&cur_lb, 0x00, sizeof(buffer));

		for(loop=0; loop<lb[index].curpos; loop++)
		{
			if ((lb[index].be)[loop].Bline == NULL)
			{
				ndeleted++;
				continue;
			}

			cur_lb.be = myrealloc(cur_lb.be, (cur_lb.curpos + 1) * sizeof(buffered_entry));
			cur_lb.be[cur_lb.curpos].Bline = (lb[index].be)[loop].Bline;
			cur_lb.be[cur_lb.curpos].pi    = (lb[index].be)[loop].pi;
			cur_lb.be[cur_lb.curpos].ts    = (lb[index].be)[loop].ts;
			cur_lb.curpos++;
		}

		delete_be_in_buffer(&lb[index]);
		lb[index].be = cur_lb.be;
		lb[index].curpos -= ndeleted;
	}
}

int add_marker_if_changed(int f_index, proginfo *cur)
{
	int changed = 0;

	if (lb[f_index].last_win != cur && (lb[f_index].marker_of_other_window || global_marker_of_other_window) && lb[f_index].marker_of_other_window != (char)-1)
	{
		add_markerline(f_index, cur, MARKER_CHANGE, NULL);

		lb[f_index].last_win = cur;

		changed = 1;
	}

	return changed;
}

int emit_to_buffer_and_term(int f_index, proginfo *cur, char *line)
{
	regmatch_t *pmatch = NULL;
	char *error = NULL;
	int matching_regex = -1;
	char display = 1;
	char *new_line = NULL;
	char do_print_and_buffer = 0;
	char marker_added = 0;
	char something_got_displayed = 0;
	dtime_t now = get_ts();

	if (replace_by_markerline && strstr(line, replace_by_markerline) != NULL)
	{
		add_markerline(f_index, cur, MARKER_REGULAR, NULL);
		return 1;
	}

	new_line = convert(&cur -> conversions, line);

	(void)check_filter(cur, new_line, &pmatch, &error, &matching_regex, 1, &display);

	/* error happened while processing regexp? */
	if (error)
	{
		if (!marker_added)
		{
			marker_added = 1;
			add_marker_if_changed(f_index, cur);
		}
		add_markerline(f_index, cur, MARKER_MSG, error);
		do_print_and_buffer = 1;
		something_got_displayed = 1;

		myfree(error);
	}

	if (cur -> repeat.suppress_repeating_lines)
	{
		if ((!cur -> repeat.last_line && !new_line) ||
				(cur -> repeat.last_line != NULL && new_line != NULL && strcmp(cur -> repeat.last_line, new_line) == 0))
		{
			cur -> repeat.n_times_repeated++;
		}
		else
		{
			do_print_and_buffer = 1;
			if (new_line)
				cur -> repeat.last_line = mystrdup(new_line);
			else
				cur -> repeat.last_line = NULL;
		}
	}
	else
		do_print_and_buffer = 1;

	if (do_print_and_buffer)
	{
		if ((cur -> repeat.n_times_repeated > 0 || display) && !marker_added)
		{
			marker_added = 1;
			something_got_displayed |= add_marker_if_changed(f_index, cur);
		}

		if (cur -> repeat.n_times_repeated)
		{
			char message[128];

			snprintf(message, sizeof(message), "Last message repeated %d times", cur -> repeat.n_times_repeated);
			do_print(f_index, cur, message, NULL, -1, now);
			do_buffer(f_index, cur, message, 1, now);
			something_got_displayed = 1;

			cur -> repeat.n_times_repeated = 0;
			myfree(cur -> repeat.last_line);
			cur -> repeat.last_line = NULL;
		}

		if (display)
		{
			char *stripped = do_strip(cur, new_line);
			/* output new text */
			do_print(f_index, cur, USE_IF_SET(stripped, new_line), pmatch, matching_regex, now);
			myfree(stripped);
			something_got_displayed = 1;

			redirect(cur, new_line, strlen(new_line), MY_TRUE);
		}

		do_buffer(f_index, cur, new_line, display, now);
	}

	if (pmatch) myfree(pmatch);
	if (new_line != line) myfree(new_line);

	return something_got_displayed;
}

void update_statusline(NEWWIN *status, int win_nr, proginfo *cur)
{
	if (mode_statusline > 0 && status != NULL && cur != NULL)
	{
		int dx;
		myattr_t attrs = statusline_attrs;
		int statusline_len = 0;
		off64_t	fsize = (off64_t)-1;
		time_t	ts = time(NULL);
		char show_f1 = 0;
		int help_str_offset = 0;
		int total_info_len = 0;
		int win_width;
		char *fname = cur -> filename;
		char timestamp[TIMESTAMP_EXTEND_BUFFER];

		if (win_nr == terminal_main_index)
			attrs.colorpair_index = find_colorpair(COLOR_RED, -1, 0);
		else if (mail)
			attrs.colorpair_index = find_colorpair(COLOR_GREEN, -1, 0);

		if ((ts - mt_started) < 5)
			show_f1 = 1;

		myattr_on(status, attrs);

		draw_line(status, LINE_BOTTOM);

		if (filename_only)
		{
			char *dummy = strrchr(cur -> filename, '/');
			if (dummy) fname = dummy + 1;
		}

		win_width = getmaxx(status -> win);
		mvwprintw(status -> win, 0, 0, "%02d] %s", win_nr, shorten_filename(USE_IF_SET(cur -> win_title, fname), win_width - 4));

		if (cur -> wt == WT_FILE)
			(void)file_info(cur -> filename, &fsize, TT_MTIME, &ts, NULL);

		get_now_ts(statusline_ts_format, timestamp, sizeof(timestamp));

		help_str_offset = 4 + strlen(USE_IF_SET(cur -> win_title, fname)); /* 4: '%02d] '!! (window nr.) */
		statusline_len = help_str_offset + strlen(timestamp) + 1; /* 4: '%02d] '!! (window nr.) */

		if (win_nr == terminal_main_index)
			wprintw(status -> win, ", press <CTRL>+<a>, <d> to exit");
		else if (mail)
			wprintw(status -> win, " You've got mail!");

		dx = getcurx(status -> win);
		if (dx >= (statusline_len + 13))
		{
			if (cur -> paused)
			{
				color_on(status, find_colorpair(COLOR_YELLOW, -1, 0));
				mvwprintw(status -> win, 0, dx - 10, "  Paused  ");
				color_off(status, find_colorpair(COLOR_YELLOW, -1, 0));
			}
			else if (cur -> wt == WT_COMMAND)
			{
				int vmsize = get_vmsize(cur -> pid);

				total_info_len = statusline_len + 12;

				if (vmsize != -1 && dx >= (statusline_len + 30))
				{
					int str_x = dx - strlen(timestamp) - 30;
					char *vmsize_str = amount_to_str(vmsize);
					mvwprintw(status -> win, 0, str_x, "%6s (VMsize) %5d (PID) - %s", vmsize_str, cur -> pid, timestamp);
					myfree(vmsize_str);

					total_info_len = statusline_len + 30;
				}
				else
				{
					if (cur -> last_exit_rc != 0)
					{
						mvwprintw(status -> win, 0, dx - strlen(timestamp) - 26, "Last rc: %d, %5d (PID) - %s", WEXITSTATUS(cur -> last_exit_rc), cur -> pid, timestamp);
					}
					else
						mvwprintw(status -> win, 0, dx - strlen(timestamp) - 12, "%5d (PID) - %s", cur -> pid, timestamp);
				}
			}
			else if (fsize == -1)
			{
				if (cur -> wt == WT_STDIN || cur -> wt == WT_SOCKET)
					mvwprintw(status -> win, 0, dx - strlen(timestamp), "%s", timestamp);
				else
				{
					mvwprintw(status -> win, 0, dx - strlen(timestamp) - 6, "??? - %s", timestamp);
					total_info_len = statusline_len + 6;
				}
			}
			else
			{
				int cur_len = 0;

				if (fsize < cur -> last_size)
					add_markerline(win_nr, cur, MARKER_MSG, " file got truncated");
				cur -> last_size = fsize;

				if (afs)
				{
					char *filesize = amount_to_str(fsize);
					cur_len = 3 + strlen(filesize);
					mvwprintw(status -> win, 0, dx - (strlen(timestamp) + cur_len), "%s - %s", filesize, timestamp);
					myfree(filesize);
				}
				else
				{
					cur_len = 13;
					/* is this trick still neccessary as I moved from off_t to off64_t? */
#if 0
					/* this trick is because on MacOS X 'off_t' is specified as a 64 bit integer */
#endif
					if (sizeof(off64_t) == 8)
						mvwprintw(status -> win, 0, dx - (strlen(timestamp) + cur_len), "%10lld - %s", fsize, timestamp);
					else
						mvwprintw(status -> win, 0, dx - (strlen(timestamp) + cur_len), "%10ld - %s", fsize, timestamp);
				}

				total_info_len = statusline_len + cur_len;
			}
		}

		if (show_f1)
		{
			if (use_colors) color_on(status, find_colorpair(COLOR_YELLOW, -1, 0));
			if (dx >= (total_info_len + 32))
				mvwprintw(status -> win, 0, help_str_offset, " *Press F1/<CTRL>+<h> for help* ");
			else if (dx >= (total_info_len + 21))
				mvwprintw(status -> win, 0, help_str_offset, " F1/<CTRL>+<h>: help ");
			else if (dx >= (total_info_len + 13))
				mvwprintw(status -> win, 0, help_str_offset, " F1/^h: help ");
			if (use_colors) color_off(status, find_colorpair(COLOR_YELLOW, -1, 0));
		}

		myattr_off(status, attrs);

		update_panels();
	}
}

void create_window_set(int startx, int width, int nwindows, int indexoffset)
{
	int loop;

	int term_offset = 0;
	int n_not_hidden = 0;
	int n_height_not_set = 0;
	int n_lines_requested = 0, n_with_nlines_request = 0;
	int n_lines_available = 0;
	int n_lines_available_per_win = 0;
	int reserved_lines = mode_statusline >= 0?1:0;
	int *lines_per_win = (int *)mymalloc(nwindows * sizeof(int));

	set_signal(SIGWINCH, SIG_IGN, "SIGWINCH");

	memset(lines_per_win, 0x00, sizeof(int) * nwindows);

	/* count the number of windows that are not hidden */
	for(loop=0; loop<nwindows; loop++)
	{
		int cur_win_index = loop + indexoffset;

		if (cur_win_index >= nfd)
			break;

		if (pi[cur_win_index].hidden == 0)
			n_not_hidden++;
	}

	if (n_not_hidden == 0)
	{
		free(lines_per_win);
		return;
	}

	/* count the number of lines needed by the windows for which
	 * the height is explicitly set
	 * also count the number of windows with this request set
	 */
	for(loop=0; loop<nwindows; loop++)
	{
		int cur_win_index = loop + indexoffset;
		if (cur_win_index >= nfd) break;

		if (pi[cur_win_index].hidden == 0 && pi[cur_win_index].win_height != -1)
		{
			n_lines_requested += (pi[cur_win_index].win_height + reserved_lines);
			n_with_nlines_request++;
		}
	}

	/* fill in the array with heights */
	/* not enough lines? simple fill-in */
	n_lines_available = max_y;
	n_height_not_set = n_not_hidden - n_with_nlines_request;
	if (n_height_not_set > 0)
		n_lines_available_per_win = (max_y - n_lines_requested) / n_height_not_set;
	if (n_height_not_set > 0 && n_lines_available_per_win < 3)
	{
		int lost_lines;
		int n_windows_that_will_fit;

		n_lines_available_per_win = max(3, max_y / n_not_hidden);
		n_windows_that_will_fit = max_y / n_lines_available_per_win;

		lost_lines = max_y - (n_lines_available_per_win * n_windows_that_will_fit);

		for(loop=0; loop<nwindows; loop++)
		{
			int cur_win_index = loop + indexoffset;
			if (cur_win_index >= nfd) break;

			if (pi[cur_win_index].hidden == 0)
			{
				lines_per_win[loop] = n_lines_available_per_win - reserved_lines;
				if (lost_lines)
				{
					lines_per_win[loop]++;
					lost_lines--;
				}
				n_lines_available -= n_lines_available_per_win;

				if (n_lines_available < n_lines_available_per_win)
					break;
			}
		}
	}
	else	/* enough lines */
	{
		int lost_lines = (max_y - n_lines_requested) - (n_lines_available_per_win * (n_not_hidden - n_with_nlines_request));

		/* not enough lines available? then ignore all requests */
		for(loop=0; loop<nwindows; loop++)
		{
			/* need to keep space for a statusline? */
			int cur_win_index = loop + indexoffset;
			if (cur_win_index >= nfd) break;

			if (!pi[cur_win_index].hidden)
			{
				if (pi[cur_win_index].win_height != -1)
				{
					lines_per_win[loop] = pi[cur_win_index].win_height;
					n_lines_available -= (pi[cur_win_index].win_height + reserved_lines);
				}
				else
				{
					lines_per_win[loop] = n_lines_available_per_win - reserved_lines;
					if (lost_lines)
					{
						lines_per_win[loop]++;
						lost_lines--;
					}
					n_lines_available -= (n_lines_available_per_win + reserved_lines);
				}
			}
		}
	}


	/* re-create windows */
	for(loop=0; loop<nwindows; loop++)
	{
		int redraw_loop;

		int cur_index = loop + indexoffset;
		if (cur_index >= nfd) break;

		if (pi[cur_index].hidden)
			continue;

		if (lines_per_win[loop] == 0)
			continue;

		/* create data window, clear en set scroll ok */
		if (statusline_above_data)
			pi[cur_index].data = mynewwin(lines_per_win[loop], width, term_offset + 1, startx);
		else
			pi[cur_index].data = mynewwin(lines_per_win[loop], width, term_offset, startx);
		bottom_panel(pi[cur_index].data -> pwin);
		werase(pi[cur_index].data -> win);
		scrollok(pi[cur_index].data -> win, TRUE);/* supposed to always return OK, according to the manpage */

		/* create status window */
		if (mode_statusline >= 0)
		{
			if (statusline_above_data)
				pi[cur_index].status = mynewwin(1, width, term_offset, startx);
			else
				pi[cur_index].status = mynewwin(1, width, term_offset + lines_per_win[loop], startx);
			bottom_panel(pi[cur_index].status -> pwin);
			werase(pi[cur_index].status -> win);

			/* create status-line */
			update_statusline(pi[cur_index].status, cur_index, &pi[cur_index]);
		}

		term_offset += lines_per_win[loop] + (mode_statusline >= 0?1:0);

		/* display old strings */
		for(redraw_loop=max(0, lb[cur_index].curpos - lines_per_win[loop]); redraw_loop<lb[cur_index].curpos; redraw_loop++)
		{
			double ts = lb[cur_index].be[redraw_loop].ts;

			if (IS_MARKERLINE(lb[cur_index].be[redraw_loop].pi) && lb[cur_index].be[redraw_loop].Bline != NULL)
			{
				do_print(cur_index, lb[cur_index].be[redraw_loop].pi, lb[cur_index].be[redraw_loop].Bline, NULL, -1, ts);
			}
			else if (lb[cur_index].be[redraw_loop].Bline)
			{
				char display;
				int matching_regex = -1;
				regmatch_t *pmatch = NULL;
				char *error = NULL;

				/* check filter */
				(void)check_filter(lb[cur_index].be[redraw_loop].pi, lb[cur_index].be[redraw_loop].Bline, &pmatch, &error, &matching_regex, 0, &display);
				if (error)
				{
					do_print(cur_index, lb[cur_index].be[redraw_loop].pi, error, NULL, -1, ts);
					myfree(error);
				}

				if (display)
				{
					char *stripped = do_strip(lb[cur_index].be[redraw_loop].pi, lb[cur_index].be[redraw_loop].Bline);
					do_print(cur_index, lb[cur_index].be[redraw_loop].pi, USE_IF_SET(stripped, lb[cur_index].be[redraw_loop].Bline), pmatch, matching_regex, ts);
					myfree(stripped);
				}

				myfree(pmatch);
			}
		}

		update_panels();
	}

	myfree(lines_per_win);

	/* set signalhandler for terminal resize */
	set_signal(SIGWINCH, signal_handler, "SIGWINCH");
}

void create_windows(void)
{
	int loop;
	int n_not_hidden = 0;

	/* close windows */
	for(loop=0; loop<nfd; loop++)
	{
		if (pi[loop].status)
		{
			delete_popup(pi[loop].status);
			pi[loop].status = NULL;
		}
		if (pi[loop].data)
		{
			delete_popup(pi[loop].data);
			pi[loop].data = NULL;
		}
	}

	if (splitlines)
	{
		for(loop=0; loop<n_splitlines; loop++)
			delete_popup(splitlines[loop]);
		myfree(splitlines);
		splitlines = NULL;
		n_splitlines = 0;
	}

	if (menu_win)
	{
		delete_popup(menu_win);
		menu_win = NULL;
	}

	werase(stdscr);

	for(loop=0; loop<nfd; loop++)
	{
		if (!pi[loop].hidden)
			n_not_hidden++;
	}

	/* no windows? display list of keys */
	if (nfd == 0 || n_not_hidden == 0)
	{
		menu_win = mynewwin(max_y, max_x, 0, 0);
		werase(menu_win -> win);

		wprintw(menu_win -> win, version_str);
		wprintw(menu_win -> win, "\n\n");

		wprintw(menu_win -> win, "%s\n", F1);
	}
	else
	{
		if (split > 0 && nfd > 1 && n_not_hidden >= split)
		{
			int cols_per_col = max_x / split;
			int win_per_col = nfd / split;
			int x = 0, indexoffset = 0;
			int *vs = (int *)mymalloc(sizeof(int) * split);
			int *vn = (int *)mymalloc(sizeof(int) * split);

			if (vertical_split)
			{
				int cols_in_use = 0;
				int cols_set = 0;
				int other_cols_size;

				for(loop=0; loop<split; loop++)
				{
					if (vertical_split[loop] >= 4)
					{
						vs[loop] = vertical_split[loop];
						cols_in_use += vertical_split[loop];
						cols_set++;
					}
				}

				/* determine width of other (=not set) columns */
				if (cols_set < split)
				{
					other_cols_size = (max_x - cols_in_use) / (split - cols_set);

					/* doesn't fit? */
					if (other_cols_size < 4)
					{
						/* then use the predetermined size */
						for(loop=0; loop<split; loop++)
							vs[loop] = cols_per_col;
					}
					else /* fill in the not given windows */
					{
						for(loop=0; loop<split; loop++)
						{
							if (vertical_split[loop] < 4)
								vs[loop] = other_cols_size;
						}
					}
				}
			}
			else
			{
				for(loop=0; loop<split; loop++)
					vs[loop] = cols_per_col;
			}

			if (n_win_per_col)
			{
				int win_in_use = 0;
				int win_set = 0;
				int other_win_n;

				for(loop=0; loop<split; loop++)
				{
					if (n_win_per_col[loop] > 0)
					{
						vn[loop] = n_win_per_col[loop];
						win_in_use += n_win_per_col[loop];
						win_set++;
					}
				}

				if (win_set < split)
				{
					other_win_n = (nfd - win_in_use) / (split - win_set);

					if (other_win_n == 0)
					{
						for(loop=0; loop<split; loop++)
							vn[loop] = win_per_col;
					}
					else
					{
						for(loop=0; loop<split; loop++)
						{
							if (n_win_per_col[loop] < 1)
								vn[loop] = other_win_n;
						}
					}
				}
			}
			else
			{
				for(loop=0; loop<split; loop++)
					vn[loop] = win_per_col;
			}

			splitlines = NULL;
			if (splitline_mode != SL_NONE)
			{
				n_splitlines = split - 1;
				if (n_splitlines > 0)
					splitlines = (NEWWIN **)mymalloc(sizeof(NEWWIN *) * n_splitlines);
			}

			for(loop=0; loop<split; loop++)
			{
				if (loop == (split - 1))
				{
					int max_n_win_per_col = max_y / 4;

					create_window_set(x, max_x - x, min(max_n_win_per_col, nfd - indexoffset), indexoffset);
				}
				else
				{
					if (splitline_mode != SL_NONE)
					{
						myattr_t sl_attrs = statusline_attrs;

						create_window_set(x, vs[loop] - 1, vn[loop], indexoffset);


						if (splitline_mode == SL_REGULAR)
							sl_attrs = statusline_attrs;
						else
							sl_attrs = splitline_attrs;
						splitlines[loop] = mynewwin(max_y, 1, 0, x + vs[loop] - 1);
						bottom_panel(splitlines[loop] -> pwin);
						myattr_on(splitlines[loop], sl_attrs);
						draw_line(splitlines[loop], LINE_LEFT);
						myattr_off(splitlines[loop], sl_attrs);
					}
					else
					{
						create_window_set(x, vs[loop], vn[loop], indexoffset);
					}

					x += vs[loop];
					indexoffset += vn[loop];
				}
			}

			myfree(vn);
			myfree(vs);
		}
		else if (nfd != 0)
		{
			create_window_set(0, max_x, nfd, 0);
		}
	}

	mydoupdate();
}

void do_set_bufferstart(int f_index, char store_what_lines, int maxnlines)
{
	lb[f_index].bufferwhat = store_what_lines;

	if (maxnlines < lb[f_index].maxnlines)
	{
		delete_be_in_buffer(&lb[f_index]);

		lb[f_index].curpos = 0;
	}

	lb[f_index].maxnlines = maxnlines;
}

char close_window(int winnr, proginfo *cur, mybool_t stop_proc)
{
	if (winnr == terminal_main_index)
	{
		terminal_index = NULL;
		terminal_main_index = -1;
	}

	/* make sure it is really gone */
	if (stop_proc)
		stop_process(cur -> pid);

	/* restart window? */
	if (cur -> restart.restart >= 0)
	{
		int initial_n_lines_tail;

		/* close old fds */
		if (myclose(cur -> fd) == -1) error_exit("Closing file descriptor of read-end of pipe failed.\n");
		if (cur -> fd != cur -> wfd)
		{
			if (myclose(cur -> wfd) == -1) error_exit("Closing file descriptor of write-end of pipe failed.\n");
		}

		/* do diff */
		if (cur -> restart.do_diff)
		{
			generate_diff(winnr, cur);

			/* update statusline */
			update_statusline(pi[winnr].status, winnr, cur);
		}


		if (cur -> initial_n_lines_tail == -1)
			initial_n_lines_tail = max_y / (nfd + 1);
		else
			initial_n_lines_tail = cur -> initial_n_lines_tail;

		/* restart tail-process */
		if (start_proc(cur, initial_n_lines_tail) != -1)
		{
			cur -> restart.is_restarted = 1;

			return -1;
		}

		/* ...and if that fails, go on with the closing */
	}

	if (warn_closed)
	{
		NEWWIN *mywin;
		int subwinnr = 0;
		proginfo *dummy = &pi[winnr];
		int win_width = 18 + 4;
		int win_name_len = strlen(cur -> filename);

		while(dummy -> next)
		{
			subwinnr++;
			dummy = dummy -> next;
		}

		if (win_name_len > win_width && (win_name_len + 4) < max_x)
			win_width = win_name_len;
		mywin = create_popup(5, win_width);

		color_on(mywin, 1);
		mvwprintw(mywin -> win, 1, 2, "Window %d/%d closed", winnr, subwinnr);
		mvwprintw(mywin -> win, 2, 2, "%s", cur -> filename);
		mvwprintw(mywin -> win, 3, 2, "Exit code: %d", WEXITSTATUS(cur -> last_exit_rc));
		draw_border(mywin);
		color_off(mywin, 1);

		wmove(mywin -> win, 1, 2);
		mydoupdate();

		/* wait_for_keypress(HELP_WINDOW_CLOSED, 0); */
		(void)getch();

		delete_popup(mywin);
	}

	if (do_not_close_closed_windows)
	{
		cur -> closed = 1;
		add_markerline(winnr, cur, MARKER_MSG, " end of file reached");
		return 127;
	}
	else
	{
		/* file no longer exists (or so) */
		return delete_entry(winnr, cur);
	}
}

void do_restart_window(proginfo *cur)
{
	/* stop process */
	stop_process(cur -> pid);

	/* close pipes */
	myclose(cur -> fd);
	myclose(cur -> wfd);

	/* re-start tail */
	if (start_proc(cur, 1) == -1)
		error_exit("Failed to start process %s.\n", cur -> filename);
}

char * key_to_keybinding(char what)
{
	static char buffer[3];

	buffer[1] = buffer[2] = 0x00;

	if (what < 32)
	{
		buffer[0] = '^';
		buffer[1] = 'a' + what - 1;
	}
	else
	{
		buffer[0] = what;
	}

	return buffer;
}

void write_escape_str(FILE *fh, char *string)
{
	int loop, len = strlen(string);

	fprintf(fh, "\"");
	for(loop=0; loop<len; loop++)
	{
		if (string[loop] == '\"')
			fprintf(fh, "\\");
		fprintf(fh, "%c", string[loop]);
	}
	fprintf(fh, "\"");
}

void emit_myattr_t(FILE *fh, myattr_t what)
{
	if (what.colorpair_index != -1)
	{
		/* FG */
		if (cp.fg_color[what.colorpair_index] != -1)
			fprintf(fh, "%s", color_to_string(cp.fg_color[what.colorpair_index]));

		/* BG */
		if (cp.bg_color[what.colorpair_index] != -1)
			fprintf(fh, ",%s", color_to_string(cp.bg_color[what.colorpair_index]));
	}

	/* attributes */
	if (what.attrs > 0)
	{
		if (what.colorpair_index == -1)
			fprintf(fh, ",,");
		else
			fprintf(fh, ",");

		fprintf(fh, "%s", attr_to_str(what.attrs));
	}
}

void add_pars_per_file(char *re, char *colorscheme, int n_buffer_lines, int buffer_bytes, char change_win_marker, int fs, int es, char *conversion_scheme)
{
	int loop;

	for(loop=0; loop<n_pars_per_file; loop++)
	{
		if (strcmp(ppf[loop].re_str, re) == 0)
			break;
	}

	if (loop == n_pars_per_file)
	{
		n_pars_per_file++;

		/* add to list */
		ppf = (pars_per_file *)myrealloc(ppf, sizeof(pars_per_file) * n_pars_per_file);
		memset(&ppf[loop], 0x00, sizeof(pars_per_file));
		ppf[loop].re_str = mystrdup(re);

		/* compile & store regular expression */
		compile_re(&ppf[loop].regex, re);

		ppf[loop].buffer_maxnlines = -1;
		ppf[loop].buffer_bytes = -1;
	}

	if (colorscheme != NULL)
	{
		ppf[loop].n_colorschemes++;
		ppf[loop].colorschemes = (char **)myrealloc(ppf[loop].colorschemes, sizeof(char *) * ppf[loop].n_colorschemes);
		ppf[loop].colorschemes[ppf[loop].n_colorschemes - 1] = mystrdup(colorscheme);
	}

	if (conversion_scheme != NULL)
	{
		ppf[loop].n_conversion_schemes++;
		ppf[loop].conversion_schemes = (char **)myrealloc(ppf[loop].conversion_schemes, sizeof(char *) * ppf[loop].n_conversion_schemes);
		ppf[loop].conversion_schemes[ppf[loop].n_conversion_schemes - 1] = mystrdup(conversion_scheme);
	}

	if (n_buffer_lines != -1)
		ppf[loop].buffer_maxnlines = n_buffer_lines;

	if (buffer_bytes != -1)
		ppf[loop].buffer_bytes = buffer_bytes;

	if (change_win_marker != (char)-1)
		ppf[loop].change_win_marker = change_win_marker;

	if (fs != -1)
	{
		add_to_iat(&ppf[loop].filterschemes, fs);
	}

	if (es != -1)
	{
		add_to_iat(&ppf[loop].editschemes, es);
	}
}

void redraw_statuslines(void)
{
	int loop;

	/* update statuslines */
	for(loop=0; loop<nfd; loop++)
	{
		proginfo *chosen = NULL;

		if (lb[loop].curpos > 0)
		{
			chosen = (lb[loop].be)[lb[loop].curpos - 1].pi; /* chosen = lb[loop].pi[lb[loop].curpos - 1]; */ 

			if (IS_MARKERLINE(chosen))
				chosen = NULL;
		}

		if (!chosen)
			chosen = &pi[loop];

		update_statusline(pi[loop].status, loop, chosen);
	}
}

int exec_bind(char key)
{
	int executed = 0;
	int loop;

	for(loop=0; loop<n_keybindings; loop++)
	{
		if (keybindings[loop].key == key)
		{
			gui_window_header(keybindings[loop].command);

			if (execute_program(keybindings[loop].command, 0) == 0)
				executed = 1;

			break;
		}
	}

	return executed;
}

void set_default_parameters_if_not_given_do(proginfo *cur, int pi_index)
{
	int maxnlines = default_maxnlines, maxbytes = default_maxbytes;
	int pm = find_path_max();
	char *real_fname = (char *)mymalloc(pm);

	do
	{
		int ppf_index;

		if (cur -> wt == WT_COMMAND || cur -> wt == WT_STDIN || cur -> wt == WT_SOCKET || cur -> check_interval != 0 || cur -> retry_open)
		{
			strncpy(real_fname, cur -> filename, pm);
			real_fname[pm - 1] = 0x00;
		}
		else
		{
			if (!realpath(cur -> filename, real_fname))
				error_exit("Problem obtaining complete (real) path of file %s.\n", cur -> filename);
		}

		/* check if any default parameters are given in the configfile */
		for(ppf_index=0; ppf_index<n_pars_per_file; ppf_index++)
		{
			int rc;

			if ((rc = regexec(&ppf[ppf_index].regex, real_fname, 0, NULL, 0)) == 0)
			{
				int fs_loop, es_loop;
				int c_nr;

				for(fs_loop=0; fs_loop<get_iat_size(&ppf[ppf_index].filterschemes); fs_loop++)
				{
					int fs = get_iat_element(&ppf[ppf_index].filterschemes, fs_loop);

					duplicate_re_array(pfs[fs].pre, pfs[fs].n_re, &cur -> pre, &cur -> n_re);
				}

				for(es_loop=0; es_loop<get_iat_size(&ppf[ppf_index].editschemes); es_loop++)
				{
					int es = get_iat_element(&ppf[ppf_index].editschemes, es_loop);

					duplicate_es_array(pes[es].strips, pes[es].n_strips, &cur -> pstrip, &cur -> n_strip);
				}

				/* set default colorscheme if not already given */
				if (ppf[ppf_index].n_colorschemes > 0 && (get_iat_size(&cur -> cdef.color_schemes) == 0 && cur -> wt != WT_COMMAND && (cur -> cdef.colorize == 'n' || cur -> cdef.colorize == 0)))
				{
					for(c_nr=0; c_nr<ppf[ppf_index].n_colorschemes; c_nr++)
					{
						int cs_index = find_colorscheme(ppf[ppf_index].colorschemes[c_nr]);
						if (cs_index == -1)
							error_exit("Color scheme '%s' is not known.\n", ppf[ppf_index].colorschemes[c_nr]);
						add_color_scheme(&cur -> cdef.color_schemes, cs_index);
					}
					cur -> cdef.colorize = 'S';
				}

				for(c_nr=0; c_nr<ppf[ppf_index].n_conversion_schemes; c_nr++)
					add_conversion_scheme(&cur -> conversions, ppf[ppf_index].conversion_schemes[c_nr]);

				if (ppf[ppf_index].buffer_maxnlines != -1)
					maxnlines = ppf[ppf_index].buffer_maxnlines;

				if (ppf[ppf_index].buffer_bytes != -1)
				{
					maxbytes = ppf[ppf_index].buffer_bytes;
				}
			}
			/* we should inform the user of any errors while executing
			 * the regexp! */
			else
			{
				char *error = convert_regexp_error(rc, &ppf[ppf_index].regex);

				if (error)
					error_popup("Set default parameters", -1, "Execution of regular expression failed with error:\n%s\n", error);
			}
		}

		cur = cur -> next;
	} while(cur);

	if (pi_index != -1)
	{
		if (lb[pi_index].maxnlines == -1 && lb[pi_index].maxbytes == -1)
		{
			if (maxbytes)
			{
				lb[pi_index].maxbytes  = maxbytes;
				lb[pi_index].maxnlines = 0;
			}
			else
			{
				lb[pi_index].maxnlines = maxnlines;
				lb[pi_index].maxbytes  = 0;
			}
		}
		else if (lb[pi_index].maxnlines == -1)
		{
			lb[pi_index].maxnlines = maxnlines;
		}
		else /* if (lb[pi_index].maxbytes == -1) */
		{
			lb[pi_index].maxbytes = maxbytes;
		}
	}

	myfree(real_fname);
}

void set_default_parameters_if_not_given(void)
{
	int pi_index;

	for(pi_index=0; pi_index<nfd; pi_index++)
		set_default_parameters_if_not_given_do(&pi[pi_index], pi_index);
}

void start_all_processes(char *nsubwindows)
{
	int loop;

	for(loop=0; loop<nfd; loop++)
	{
		proginfo *cur = &pi[loop];
		int cur_win_size = max_y / nsubwindows[loop];

		do
		{
			cur -> statistics.start_ts = time(NULL);

			if (cur -> wt == WT_STDIN)
			{
				int old_0 = mydup(0);

				if (old_0 == -1) error_exit("Cannot dup(0).\n");

				if (myclose(0) == -1) error_exit("Error closing fd 0.\n");

				if (myopen("/dev/tty", O_RDONLY) != 0) error_exit("New fd != 0\n");

				cur -> fd = old_0;
				cur -> wfd = -1;
				cur -> pid = -1;
			}
			else if (cur -> wt == WT_SOCKET)
			{
				char *dummy = mystrdup(cur -> filename);
				char *colon = strchr(dummy, ':');
				struct addrinfo hints;
				struct addrinfo* result;
				struct addrinfo* rp;
				int sfd = -1, s;

				char *host = NULL;
				char *service = "syslog";

				if (colon)
				{
					service = colon + 1;
					*colon = 0x00;
					if (colon > dummy)
						host = dummy;
				}

				memset(&hints, 0x00, sizeof(struct addrinfo));
				hints.ai_family = AF_UNSPEC;
				hints.ai_socktype = SOCK_DGRAM;
				hints.ai_flags = AI_PASSIVE;
				hints.ai_protocol = 0;
				hints.ai_canonname = NULL;
				hints.ai_addr = NULL;
				hints.ai_next = NULL;

				s = getaddrinfo(host, service, &hints, &result);
				if (s != 0)
					error_exit("Failed to create socket for receiving syslog data on %s: %s.\n", cur -> filename, gai_strerror(s));

				for (rp = result; rp != NULL; rp = rp -> ai_next)
				{
					sfd = socket(rp -> ai_family, rp -> ai_socktype, rp -> ai_protocol);
					if (sfd == -1)
						continue;
					if (bind(sfd, rp -> ai_addr, rp -> ai_addrlen) == 0)
						break;
					close(sfd);
				}

				freeaddrinfo(result);

				if (rp == NULL)
					error_exit("Failed to create socket for receiving syslog data on %s.\n", cur -> filename);

				cur -> wfd = cur -> fd = sfd;

				cur -> wfd = cur -> fd = socket(AF_INET, SOCK_DGRAM, 0);
				if (cur -> fd == -1)

					myfree(dummy);

				cur -> pid = -1;
			}
			else
			{
				if (cur -> initial_n_lines_tail == -1)
					cur -> initial_n_lines_tail = cur_win_size;

				/* start the tail process for this file/command */
				if (start_proc(cur, cur -> initial_n_lines_tail) == -1)
					error_exit("Failed to start process for %s.\n", cur -> filename);
			}

			cur = cur -> next;
		} while(cur);
	}
}

void version(void)
{
	printf("%s", version_str);
	printf("\n\nThank you for using MultiTail.\n");
	printf("If you have any suggestion on how I can improve this program,\n");
	printf("do not hesitate to contact me at folkert@vanheusden.com\n");
	printf("Website is available at: http://www.vanheusden.com/multitail/\n\n\n");
}

void init_check_for_mail(void)
{
	if (check_for_mail > 0 && mail_spool_file != NULL)
	{
		if (stat64(mail_spool_file, &msf_info) == -1)
		{
			check_for_mail = 0;

			printf("Could not determine size of file '%s' (which is supposed to be ", mail_spool_file);
			printf("your mailfile): mail-check is disabled.\n");
			printf("You can prevent this message by adding the line 'check_mail:0' ");
			printf("in "CONFIG_FILE" or in .multitailrc in your home-directory.\n\n");
			printf("Press enter to continue...");
			fflush(NULL);
			getchar();
		}
		else
		{
			msf_prev_size = msf_info.st_size;
			msf_last_check = get_ts();
		}
	}
}

void main_loop(void)
{
	/* create windows */
	set_do_refresh(2);

	for(;;)
	{
		int c = wait_for_keypress(HELP_MAIN_MENU, 0, NULL, 0);

		if (terminal_main_index == -1)
		{
			int uc = toupper(c);

			set_do_refresh(1);

			if (mail)
			{
				mail = 0;
				redraw_statuslines();
			}

			if (c == -1)
			{
				set_do_refresh(2);
				continue;
			}
			else if (uc == 'Q' || uc == 'X')
			{
				break;
			}
			else if (c == 'Z')	/* clear all marker lines */
			{
				delete_all_markerlines();
				set_do_refresh(2);
				continue;
			}
			else if (uc == 'A')
			{
				if (add_window())
					set_do_refresh(2);
				continue;
			}
			else if (uc == 'H' || c == 8)
			{
				show_help(HELP_MAIN_MENU);
				continue;
			}
			else if (c == 'r' || c == 12)
			{
				set_do_refresh(2);
				continue;
			}
			else if (c == 'R')
			{
				restart_window();
				continue;
			}
			else if (c == 'i')
			{
				info();
				continue;
			}
			else if (c == 't')
			{
				statistics_menu();
				continue;
			}
			else if (c == 'T')
			{
				truncate_file();
				continue;
			}
			else if (uc == 'J')
			{
				if (set_window_sizes())
					set_do_refresh(2);
				continue;
			}
			else if (uc == 'L')
			{
				list_keybindings();
				continue;
			}

			if (nfd == 0)
			{
				wrong_key();
				set_do_refresh(0);
				continue;
			}

			else if (uc == 'E' || uc == '\\')
			{
				if (edit_regexp())
					set_do_refresh(2);
			}
			else if (uc == 'F')
			{
				if (edit_strippers())
					set_do_refresh(2);
			}
			else if (uc == 'D')
			{
				if (delete_window())
					set_do_refresh(2);
				continue;
			}
			else if (uc == 'V')
			{
				if (toggle_vertical_split())
					set_do_refresh(2);
			}
			else if (c == 'c')
			{
				if (toggle_colors())
					set_do_refresh(2);
			}
			else if (c == 'C' && use_colors)
			{
				if (can_change_color())
					(void)color_management(NULL, NULL);
				else
					error_popup("Edit colors", HELP_CANNOT_EDIT_COLOR, "Your terminal doesn't support editing of colors.");
			}
			else if (uc == 'S')
			{
				if (swap_window())
					set_do_refresh(2);
				continue;
			}
			else if (c == 'z')
			{
				if (hide_window())
					set_do_refresh(2);
			}
			else if (c == 'u')
			{
				if (hide_all_but())
					set_do_refresh(2);
			}
			else if (c == 'U')
			{
				if (unhide_all_windows())
					set_do_refresh(2);
			}
			else if (uc == 'G')
			{
				screendump();
			}
			else if (uc == 'W')
			{
				write_script();
			}
			else if (uc == 'M')
			{
				set_buffering();
			}
			else if (c == 'b')
			{
				scrollback();
			}
			else if (c == 'B')
			{
				merged_scrollback_with_search(NULL, MY_FALSE);
			}
			else if (c == 'p')
			{
				do_pause();
			}
			else if (c == 'P')	/* pause an individual screen */
			{
				selective_pause();
			}
			else if (uc >= '0' && uc <= '9')
			{
				int index = uc - '0';

				if (index < nfd)
					add_markerline(index, NULL, MARKER_REGULAR, NULL);
				else
					wrong_key();
			}
			else if (uc == 13)
			{
				int loop;

				for(loop=0; loop<nfd; loop++)
					add_markerline(loop, NULL, MARKER_REGULAR, NULL);
			}
			else if (uc == 'K')
			{
				terminal_mode();
			}
			else if (uc == ' ')	/* space */
			{
				/* mail = 0;
				   redraw_statuslines(); */
			}
			else if (c == 'o')	/* wipe window */
			{
				if (nfd == 1)
					wipe_all_windows();
				else
					wipe_window();
			}
			else if (c == 'O')	/* wipe all windows */
			{
				wipe_all_windows();
			}
			else if (c == 'y')	/* set linewrap */
			{
				set_linewrap();
				set_do_refresh(2);
			}
			else if (c == 'Y')	/* send a signal to a window */
			{
				send_signal();
			}
			else if (c == '/')	/* search in all windows */
			{
				search_in_all_windows();
			}
			else if (c == '?')	/* highlight in all windows */
			{
				highlight_in_all_windows();
				set_do_refresh(2);
			}
			else if (c == 'I')
			{
				toggle_regexp_case_insensitive();
			}
			else if (c == 20)	/* ^t */
			{
				toggle_subwindow_nr();
				set_do_refresh(2);
			}
			else if (c == 22)	/* ^v */
			{
				select_conversionschemes();
			}
			else if (c == 'n')
			{
				clear_a_buffer();
			}
			else if (c == 'N')
			{
				clear_all_buffers();
			}
			else if (c == KEY_LEFT || c == KEY_RIGHT)
			{
				horizontal_scroll(c);
				set_do_refresh(2);
			}
			else
			{
				if (exec_bind(c))
					set_do_refresh(1);
				else
				{
					wrong_key();
					set_do_refresh(0);
				}
			}
		}
		else
		{
			do_terminal((char)c);
		}
	}
}

void check_for_valid_stdin()
{
	if (!ttyname(0))
	{
		if (errno == ENOTTY || errno == EINVAL)
			error_exit("Please use -j/-J when you want to pipe something into MultiTail.\n");
		else
			error_exit("ttyname(0) failed.\n");
	}
}

int main(int argc, char *argv[])
{
	int loop;
	char *dummy = getenv("MAILCHECK");
	struct servent *sl = getservbyname("syslog", "tcp");

	/* for mbsrtowcs */
	setlocale(LC_CTYPE, "");

	if (sl)
		syslog_port = sl -> s_port;

	cp.n_def = cp.size = 0;
	cp.fg_color = cp.bg_color = NULL;

	/* removed because if gives regexp compilation problems for some
	 * LANG=... environment variables settings
	 * setlocale(LC_ALL, ""); */

	time(&mt_started);

	set_signal(SIGTERM, signal_handler, "SIGTERM");
	set_signal(SIGINT,  signal_handler, "SIGINT" );
	set_signal(SIGHUP,  signal_handler, "SIGHUP" );
	set_signal(SIGCHLD, signal_handler, "SIGCHLD");

	shell            = mystrdup("/bin/sh");
	tail             = mystrdup("tail");
	ts_format        = mystrdup("%b %d %H:%M:%S");
	window_number    = mystrdup("[%02d]");
	cnv_ts_format    = mystrdup("%b %d %H:%M:%S %Y");
	line_ts_format   = mystrdup("%b %d %H:%M:%S %Y ");
	subwindow_number = mystrdup("[%02d]");
	statusline_ts_format = mystrdup("%b %d %H:%M:%S %Y");
	syslog_ts_format = mystrdup("%b %d %H:%M:%S %Y");

	mail_spool_file = getenv("MAIL");
	if (dummy) check_for_mail = atoi(dummy);

	/* calc. buffer length (at least a complete terminal screen) */
	initscr();
	min_n_bufferlines = max(MIN_N_BUFFERLINES, LINES);
	if (has_colors())
	{
		start_color();
		use_default_colors();
		use_colors = 1;
	}

	init_colors();

	init_colornames();

	/* start curses library */
	init_ncurses();

	/* verify size of terminal window */
	if (LINES < 23 || COLS < 78)
		error_popup("Terminal too small", -1, "Your terminal(-window) is %dx%d. That is too small\nfor MultiTail: at least 78x23 is required.\nSome menus will look garbled!", COLS, LINES);

	/* default parameters for the statusline */
	statusline_attrs.colorpair_index = find_or_init_colorpair(COLOR_WHITE, COLOR_BLACK, 0);
	statusline_attrs.attrs = A_REVERSE;

	endwin();

	do_commandline(argc, argv);

	cmdfile_h.history = search_h.history = NULL;

	load_configfile(config_file);

	init_history(&search_h);
	init_history(&cmdfile_h);

	if (defaultcscheme)
	{
		default_color_scheme = find_colorscheme(defaultcscheme);

		if (default_color_scheme == -1)
			error_exit("Default color scheme '%s' is not defined. Please check the MultiTail configuration file %s.\n", defaultcscheme, config_file);
	}

	if (markerline_attrs.colorpair_index == -1 && markerline_attrs.attrs == -1)
		markerline_attrs = find_attr(COLOR_RED, COLOR_BLACK, A_REVERSE);

	init_children_reaper();

	/* initialise mail-stuff */
	init_check_for_mail();

	/* it seems that on at least FreeBSD ncurses forgets all defined
	 * colorpairs after an endwin()
	 * so here were explicitly re-setting them
	 */
	for(loop=0; loop<cp.n_def; loop++)
		init_pair(loop, cp.fg_color[loop], cp.bg_color[loop]);

	/* get terminal type */
	get_terminal_type();

	set_default_parameters_if_not_given();

	/* start processes */
	start_all_processes(nsubwindows);
	myfree(nsubwindows);

	set_signal(SIGUSR1, signal_handler, "SIGUSR1");

	check_for_valid_stdin();

	main_loop();

	/* kill tail processes */
	do_exit();

	return 0;
}

void sigusr1_restart_tails(void)
{
	int f_index;

	for(f_index=0; f_index<nfd; f_index++)
	{
		proginfo *cur = &pi[f_index];

		do
		{
			if (cur -> wt == WT_FILE)
			{
				int pipefd[2];

				stop_process(cur -> pid);

				if (myclose(cur -> fd) == -1) error_exit("Closing read filedescriptor failed.\n");
				if (cur -> fd != cur -> wfd)
				{
					if (myclose(cur -> wfd) == -1) error_exit("Closing write filedescriptor failed.\n");
				}

				/* create a pipe, will be to child-process */
				if (-1 == pipe(pipefd)) error_exit("Error while creating pipe.\n");

				cur -> pid = start_tail(cur -> filename, cur -> retry_open, cur -> follow_filename, min_n_bufferlines, pipefd);
				cur -> fd = pipefd[0];
				cur -> wfd = pipefd[1];
			}

			cur = cur -> next;
		} while(cur);
	}
}

int check_for_died_processes(void)
{
	int a_window_got_closed = 0;

	for(;;)
	{
		int f_index;
		char found = 0;

		/* see if any processed stopped */
		int exit_code = 0;
		pid_t died_proc = waitpid(-1, &exit_code, WNOHANG | WUNTRACED);

		if (died_proc == 0)
		{
			break;
		}
		else if (died_proc == -1)
		{
			if (errno == ECHILD)
				break;

			error_exit("waitpid failed.\n");
		}

		/* now check every window for died processes */
		for(f_index=0; f_index<nfd && !found; f_index++)
		{
			proginfo *cur = &pi[f_index];

			do
			{
				/* did it exit? */
				if (died_proc == cur -> pid && cur -> wt != WT_STDIN && cur -> wt != WT_SOCKET)
				{
					int refresh_info = close_window(f_index, cur, MY_FALSE);

					found = a_window_got_closed = 1;

					/* is an entry deleted? (>=0) or restarted? (==-1) */
					if (refresh_info >= 0)
					{
						set_do_refresh(2);
					}
					else if (cur -> restart.do_diff && get_do_refresh() == 0)
					{
						if (get_do_refresh() != 2)
							set_do_refresh(1);
					}

					break;
				}

				cur = cur -> next;
			}
			while(cur);
		}

		/* check child processes */
		if (!found)
		{
			int loop;

			for(loop=0; loop<n_children; loop++)
			{
				if (died_proc == children_list[loop])
				{
					int n_pids_to_move = (n_children - loop) - 1;

					if (n_pids_to_move > 0)
						memmove(&children_list[loop], &children_list[loop + 1], sizeof(pid_t) * n_pids_to_move);

					n_children--;
					break;
				}
			}
		}
	}

	return a_window_got_closed;
}

int find_window(char *filename, int *index, proginfo **cur)
{
	int loop;

	for(loop=0; loop<nfd; loop++)
	{
		proginfo *pc = &pi[loop];

		do
		{
			if (strcmp(pc -> filename, filename) == 0)
			{
				if (index) *index = loop;
				if (cur) *cur = pc;

				return 0;
			}

			pc = pc -> next;
		}
		while(pc);
	}

	return -1;
}

void create_new_win(proginfo **cur, int *nr)
{
	int loop;

	proginfo *newpi = (proginfo *)myrealloc(pi, (nfd + 1) * sizeof(proginfo));
	lb = (buffer *)myrealloc(lb, (nfd + 1) * sizeof(buffer));

	/* 'lb' has pointers to 'pi'-structures. now since we've realloced the pi-
	 * array, we need to update to pointers to pi in the lb-array
	 */
	for(loop=0; loop<nfd; loop++)
		buffer_replace_pi_pointers(loop, &pi[loop], &newpi[loop]);

	/* now that everything is updated, we can safely forget the previous pointer */
	pi = newpi;
	memset(&pi[nfd], 0x00, sizeof(proginfo));

	/* initialize the new structure */
	memset(&lb[nfd], 0x00, sizeof(buffer));
	lb[nfd].maxnlines = min_n_bufferlines;
	lb[nfd].bufferwhat = 'f';

	if (cur) *cur = &pi[nfd];
	if (nr) *nr = nfd;

	nfd++;
}

int check_paths(void)
{
	int new_wins = 0;
	int loop;
	dtime_t now = get_ts();

	for(loop=0; loop<n_cdg; loop++)
	{
		if (now - cdg[loop].last_check >= cdg[loop].check_interval)
		{
			int fi;
			glob_t files;

			/* get list of files that match */
			if (glob(cdg[loop].glob_str, GLOB_ERR | GLOB_NOSORT, NULL, &files) != 0)
				continue;

			/* check each found file */
			for(fi=0; fi<files.gl_pathc; fi++)
			{
				time_t last_mod;
				mode_t ftype;

				if (file_info(files.gl_pathv[fi], NULL, cdg[loop].new_only, &last_mod, &ftype) == -1)
					continue;

				/* file got changed and/or is new and is a regular file?
				 * and is created after multitail started?
				 */
				if (last_mod >= cdg[loop].last_check &&
						find_window(files.gl_pathv[fi], NULL, NULL) == -1 &&
						S_ISREG(ftype) &&
						((cdg[loop].new_only && last_mod >= mt_started) || !cdg[loop].new_only)
				   )
				{
					proginfo *cur = NULL;
					int win_nr = -1;

					/* create new structure containing all info and such */
					if (!cdg[loop].in_one_window)
					{
						create_new_win(&cur, &win_nr);
					}
					else if (cdg[loop].window_nr == -1)
					{
						create_new_win(&cur, &cdg[loop].window_nr);
						win_nr = cdg[loop].window_nr;
					}
					else
					{
						cur = &pi[cdg[loop].window_nr];

						/* skip to end of chain */
						while (cur -> next) cur = cur -> next;

						/* allocate new entry */
						cur -> next = (proginfo *)mymalloc(sizeof(proginfo));
						cur = cur -> next;
						memset(cur, 0x00, sizeof(proginfo));
					}

					/* fill in */
					cur -> filename = mystrdup(files.gl_pathv[fi]);
					cur -> line_wrap = default_linewrap;
					cur -> win_height = -1;
					cur -> statistics.sccfirst = 1;
					set_default_parameters_if_not_given_do(cur, win_nr);

					/* start the tail process for this file/command */
					if (start_proc(cur, max_y / nfd) == -1)
						error_exit("Failed to start tail process for %s.\n", cur -> filename);

					new_wins = 1;
				}
			} /* check all matched files */

			cdg[loop].last_check = now;
		} /* time for a check? */
	} /* check all search patterns  */

	return new_wins;
}

void resize_terminal_do(NEWWIN *popup)
{
	determine_terminal_size(&max_y, &max_x);

	if (ERR == resizeterm(max_y, max_x)) error_exit("An error occured while resizing terminal(-window).\n");

	endwin();
	refresh(); /* <- as specified by ncurses faq, was: doupdate(); */

	create_windows();

	if (popup)
	{
		if ((popup -> x_off + popup -> width) > max_x ||
				(popup -> y_off + popup -> height) > max_y)
		{
			popup -> x_off = max(max_x - (popup -> width + 1), 0);
			popup -> y_off = max(max_y - (popup -> height + 1), 0);
			move_panel(popup -> pwin, popup -> y_off, popup -> x_off);
			mydoupdate();
		}
	}
}


int process_global_keys(int what_help, NEWWIN *popup, char cursor_shift)
{
	int c = getch();

	if (c == KEY_RESIZE)
		return -1;

	redraw_statuslines();

	if (c == KEY_F(5))
	{
		resize_terminal_do(popup);

		set_do_refresh(2);
	}
	else if (c == exit_key)     /* ^C */
	{
		do_exit();

		error_exit("This should not be reached.\n");
	}
	else if (c == 8 || c == KEY_F(1))       /* HELP (^h / F1) */
	{
		show_help(what_help);
	}
	else if (popup != NULL && ((cursor_shift == 0 && c == KEY_LEFT) || (cursor_shift == 1 && c == KEY_SLEFT)))
	{
		if (popup -> x_off > 0)
		{
			popup -> x_off--;
			move_panel(popup -> pwin, popup -> y_off, popup -> x_off);
			mydoupdate();
		}
		else
			wrong_key();
	}
	else if (cursor_shift == 0 && c == KEY_UP)
	{
		if (popup != NULL && popup -> y_off > 0)
		{
			popup -> y_off--;
			move_panel(popup -> pwin, popup -> y_off, popup -> x_off);
			mydoupdate();
		}
		else
			wrong_key();
	}
	else if (popup != NULL && ((cursor_shift == 0 && c == KEY_RIGHT) || (cursor_shift == 1 && c == KEY_SRIGHT)))
	{
		if ((popup -> x_off + popup -> width) < max_x)
		{
			popup -> x_off++;
			move_panel(popup -> pwin, popup -> y_off, popup -> x_off);
			mydoupdate();
		}
		else
			wrong_key();
	}
	else if (cursor_shift == 0 && c == KEY_DOWN)
	{
		if (popup != NULL && (popup -> y_off + popup -> height) < max_y)
		{
			popup -> y_off++;
			move_panel(popup -> pwin, popup -> y_off, popup -> x_off);
			mydoupdate();
		}
		else
			wrong_key();
	}

	return c;
}

char process_input_data(int win_nr, proginfo *cur, char *data_in, int new_data_offset, int n_bytes_added, double now)
{
	char *pnt = data_in;
	char statusline_update_needed = 0;

	/* statistics */
	cur -> statistics.bytes_processed += n_bytes_added;

	data_in[new_data_offset + n_bytes_added] = 0x00;

	if (strchr(&data_in[new_data_offset], '\n'))
	{
		char emitted = 0;

		if (cur -> cont) /* reconnect lines with '\' */
		{
			char *contsearch = pnt;

			while((contsearch = strstr(contsearch, "\\\n")))
			{
				memmove(contsearch, contsearch + 2, strlen(contsearch + 2) + 1);
			}
		}

		/* display lines */
		for(;*pnt;)
		{
			char *end = strchr(pnt, '\n');
			if (!end)
				break;

			*end = 0x00;

			/* redirect to some other file/pipe? */
			redirect(cur, pnt, (int)(end - pnt), MY_FALSE);

			/* gen stats */
			store_statistics(cur, now);

			/* see if we need to beep already */
			if (beep_interval > 0)
			{
				if (++linecounter_for_beep == beep_interval)
				{
					linecounter_for_beep = 0;
					did_n_beeps++;
					beep();
				}
			}

			/* beep per (sub-)window */
			if (cur -> beep.beep_interval > 0)
			{
				if (++cur -> beep.linecounter_for_beep == cur -> beep.beep_interval)
				{
					cur -> beep.linecounter_for_beep = 0;
					cur -> beep.did_n_beeps++;
					beep();
				}
			}

			/* is this the output of a program which we should diff and such? */
			if (cur -> restart.do_diff)
			{
				store_for_diff(&cur -> restart.diff, pnt);
			}
			else /* no, just output */
			{
				if (get_do_refresh() == 0)
					set_do_refresh(1); /* after update interval, update screen */

				statusline_update_needed |= emit_to_buffer_and_term(win_nr, cur, pnt);
			}

			if (!emitted)
			{
				emitted = 1;

				if (cur -> restart.is_restarted && cur -> restart.restart_clear)
				{
					cur -> restart.is_restarted = 0;
					werase(pi[win_nr].data -> win);
				}

				update_panels();
			}

			pnt = end + 1;
		}
	}

	if (*pnt != 0x00)
	{
		int line_len = strlen(pnt) + 1;
		cur -> incomplete_line = mymalloc(line_len);
		memcpy(cur -> incomplete_line, pnt, line_len);
	}

	return statusline_update_needed;
}

int recv_from_fd(int fd, char **buffer, int new_data_offset, int read_size)
{
	struct sockaddr sa;
	socklen_t ssa_len = sizeof(sa);
	char *recv_buffer = mymalloc(read_size + 1);
	char time_buffer[TIMESTAMP_EXTEND_BUFFER];
	char *host = "";
	char *facility = "";
	char *severity = "";
	int added_bytes;
	int nbytes;

	get_now_ts(syslog_ts_format, time_buffer, sizeof(time_buffer));

	nbytes = recvfrom(fd, recv_buffer, read_size, 0, &sa, &ssa_len);
	recv_buffer[nbytes] = 0x00;

	if (resolv_ip_addresses == MY_TRUE)
	{
		struct sockaddr_in *sai = (struct sockaddr_in *)&sa;
		struct hostent *he = gethostbyaddr(&(sai -> sin_addr.s_addr), sizeof(sai -> sin_addr.s_addr), AF_INET);

		if (he != NULL && he -> h_name != NULL)
			host = he -> h_name;
		else
			host = "?";
	}
	else
	{
		host = inet_ntoa(((struct sockaddr_in *)&sa) -> sin_addr);
	}

	if (show_severity_facility == MY_TRUE)
	{
		char *gt = strchr(recv_buffer, '>');

		if (recv_buffer[0] == '<' && (recv_buffer[2] == '>' || recv_buffer[3] == '>' || recv_buffer[4] == '>') && gt != NULL)
		{
			int value = atoi(recv_buffer + 1);
			int severity_nr = value & 7;
			int facility_nr = value / 8;

			severity = severities[severity_nr];
			if (facility_nr >= 24)
				facility = "?";
			else
				facility = facilities[facility_nr];

			memmove(recv_buffer, gt + 1, strlen(gt));
		}
	}

	added_bytes = strlen(time_buffer) + 1 + strlen(host) + 1 + strlen(facility) + 1 + strlen(severity) + 1 + 1 + nbytes + 1;

	*buffer = myrealloc(*buffer, new_data_offset + added_bytes);

	nbytes = snprintf(&(*buffer)[new_data_offset], added_bytes, "%s%s%s%s%s%s%s%s %s\n",
			time_buffer, time_buffer[0]?" ":"",
			host,        host[0]       ?" ":"",
			facility,    facility[0]   ?" ":"",
			severity,    severity[0]   ?" ":"",
			recv_buffer);

	myfree(recv_buffer);

	return nbytes;
}

int wait_for_keypress(int what_help, double max_wait, NEWWIN *popup, char cursor_shift)
{
	int c = -1;
	dtime_t max_wait_start = get_ts();

	for(;;)
	{
		int deleted_entry_in_array = -1;
		int last_fd = 0, rc, loop;
		fd_set rfds;
		struct timeval tv;
		dtime_t now = get_ts();
		proginfo *last_changed_window = NULL;
		char prev_mail_status = mail;
		static double lastupdate = 0;
		double sleep = 32767.0;

		/* need to check any paths? */
		if (check_paths())
			set_do_refresh(2);

		sleep = min(sleep, check_for_mail > 0     ? max((msf_last_check + check_for_mail)  - now, 0) : 32767);
		sleep = min(sleep, heartbeat_interval > 0 ? max((heartbeat_t + heartbeat_interval) - now, 0) : 32767);
		sleep = min(sleep, max_wait > 0.0         ? max((max_wait_start + max_wait)        - now, 0) : 32767);

		for(loop=0; loop<n_cdg; loop++)
			sleep = min(sleep, max((cdg[loop].last_check + cdg[loop].check_interval) - now, 0));

		if (sleep < 32767.0)
		{
			if (sleep < 0.01)
				sleep = 0.01;

			tv.tv_sec = sleep;
			tv.tv_usec = (sleep - (double)tv.tv_sec) * 1000000;
		}
		else
		{
			tv.tv_sec = 32767;
			tv.tv_usec = 0;
		}

		FD_ZERO(&rfds);

		/* add stdin to fd-set: needed for monitoring key-presses */
		FD_SET(0, &rfds);

		/* add fd's of pipes to fd-set */
		for(loop=0; loop<nfd; loop++)
		{
			proginfo *cur = &pi[loop];

			if (cur -> paused)
				continue;

			if (cur -> closed)
				continue;

			do
			{
				if (cur -> fd != -1)
				{
					FD_SET(cur -> fd, &rfds);
					last_fd = max(last_fd, cur -> fd);
				}

				cur = cur -> next;
			}
			while(cur);
		}

		/* update screen? */
		if (get_do_refresh())
		{
			if (get_do_refresh() == 2)
				create_windows();

			if (now - lastupdate >= update_interval)
			{
				set_do_refresh(0);
				lastupdate = now;
				mydoupdate();
			}
		}

		/* wait for any data or key press */
		if ((rc = select(last_fd + 1, &rfds, NULL, NULL, tv.tv_sec != 32767 ? &tv : NULL)) == -1)
		{
			if (errno != EINTR && errno != EAGAIN)
				error_exit("select() returned an error.");
		}

		now = get_ts();

		if (heartbeat_interval > 0 && now - heartbeat_t >= heartbeat_interval)
		{
			heartbeat();

			heartbeat_t = now;
		}

		/* check for mail */
		if (now - msf_last_check >= check_for_mail)
		{
			do_check_for_mail(now);

			msf_last_check = now;
		}

		total_wakeups++;

		if (terminal_changed)
		{
			terminal_changed = 0;
			resize_terminal_do(popup);
		}

		if (max_wait != 0.0 && now - max_wait_start >= max_wait)
			break;

		if (got_sigusr1)
		{
			got_sigusr1 = 0;

			sigusr1_restart_tails();
		}

		if (rc > 0)
		{
			/* any key pressed? */
			if (FD_ISSET(0, &rfds))
			{
				mail = 0;

				c = process_global_keys(what_help, popup, cursor_shift);
				if (c != -1)
					break;
			}

			/* go through all fds */
			for(loop=0; loop<nfd; loop++)
			{
				proginfo *cur = &pi[loop];
				NEWWIN *status = pi[loop].status;

				if (cur -> paused)
					continue;

				if (cur -> closed)
					continue;

				do
				{

					if (cur -> fd != -1 && FD_ISSET(cur -> fd, &rfds))
					{
						char *buffer = NULL;
						int read_size = min(SSIZE_MAX, 65536);
						int nbytes, new_data_offset = cur -> incomplete_line ? strlen(cur -> incomplete_line) : 0;

						buffer = myrealloc(cur -> incomplete_line, new_data_offset + read_size + 1);
						cur -> incomplete_line = NULL;

						if (cur -> wt == WT_SOCKET)
						{
							nbytes = recv_from_fd(cur -> fd, &buffer, new_data_offset, read_size);
						}
						else
						{
							nbytes = read(cur -> fd, &buffer[new_data_offset], read_size);
						}

						/* read error or EOF? */
						if ((nbytes == -1 && errno != EINTR && errno != EAGAIN) || nbytes == 0)
						{
							if (nbytes == 0 && cur -> wt == WT_STDIN)
							{
								pi[loop].paused = 1;
								update_statusline(status, loop, cur);
								set_do_refresh(2);
							}
							else
							{
								if (last_changed_window == cur)
									last_changed_window = NULL;

								deleted_entry_in_array = close_window(loop, cur, MY_TRUE);

								if (deleted_entry_in_array >= 0)
								{
									set_do_refresh(2);
									free(buffer);
									goto closed_window;
								}
								else if (cur -> restart.do_diff && get_do_refresh() == 0)
								{
									if (get_do_refresh() != 2)
										set_do_refresh(1);
								}
							}
						}
						else if (nbytes != -1)	/* if nbytes == -1 it must be an interrupt while READ */
						{
							/* display statusline? */
							if (process_input_data(loop, cur, buffer, new_data_offset, nbytes, now))
							{
								update_statusline(status, loop, cur);
							}

							/* remember this window as it might be displayed in the GUI
							 * terminal window
							 */
							last_changed_window = cur;
						}

						myfree(buffer);
					}

					/* close idle, if requested */
					if (cur -> close_idle > 0 && now - cur -> statistics.lastevent > cur -> close_idle)
					{
						if (last_changed_window == cur)
							last_changed_window = NULL;

						deleted_entry_in_array = close_window(loop, cur, MY_TRUE);
						if (deleted_entry_in_array >= 0)
						{
							set_do_refresh(2);

							goto closed_window;
						}
					}

					if (cur -> mark_interval)
					{
						if (now - cur -> statistics.lastevent >= cur -> mark_interval)
						{
							add_markerline(loop, cur, MARKER_IDLE, NULL);

							if (get_do_refresh() == 0)
								set_do_refresh(1);

							cur -> statistics.lastevent = now;
						}
					}

					cur = cur -> next;
				}
				while(cur);

				if (deleted_entry_in_array > 0)
					break;
			}
		}

		/* see if any processes have died (processes started
		 * by matchin regular expressions)
		 */
		if (need_died_procs_check)
		{
			need_died_procs_check = 0;

			if (check_for_died_processes())
			{
				c = -1;
				break;
			}
		}

closed_window:

		/* any window changed? then we may want to update the terminal window header */
		if ((last_changed_window != NULL || mail != prev_mail_status) && set_title != NULL)
		{
			prev_mail_status = mail;

			draw_gui_window_header(last_changed_window);
		}

		if (deleted_entry_in_array >= 0)
		{
			c = -1;
			break;
		}
	}

	return c;
}
