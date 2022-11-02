#define _LARGEFILE64_SOURCE     /* required for GLIBC to enable stat64 and friends */
#include <sys/types.h>
#include <regex.h>
#include <sys/utsname.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "term.h"
#include "utils.h"
#include "error.h"
#include "mem.h"
#include "help.h"
#include "globals.h"
#include "ui.h"


void info(void)
{
	NEWWIN *mywin = create_popup(19, 60);
	int line = 7;
	struct utsname uinfo;
	int proc_u_line;
	char *term = getenv("TERM");

	mvwprintw(mywin -> win, 1, 2, "-=* MultiTail " VERSION " *=-");
	mvwprintw(mywin -> win, 3, 2, "Written by folkert@vanheusden.com");
	mvwprintw(mywin -> win, 4, 2, "Website: http://www.vanheusden.com/multitail/");
	if (!use_colors)
		mvwprintw(mywin -> win, line++, 2, "Your terminal doesn't support colors");

	if (uname(&uinfo) == -1)
		error_popup("Retrieving system information", -1, "uname() failed\n");
	else
	{
		line++;
		mvwprintw(mywin -> win, line++, 2, "Running on:");
#if defined(_GNU_SOURCE) && !defined(__APPLE__)
		mvwprintw(mywin -> win, line++, 2, "%s/%s %s %s", uinfo.nodename, uinfo.sysname, uinfo.machine, uinfo.domainname);
#else
		mvwprintw(mywin -> win, line++, 2, "%s/%s %s", uinfo.nodename, uinfo.sysname, uinfo.machine);
#endif
		mvwprintw(mywin -> win, line++, 2, "%s %s", uinfo.release, uinfo.version);
		line++;
	}

	if (has_colors())
		mvwprintw(mywin -> win, line++, 2, "colors: %d, colorpairs: %d (%d), change colors: %s", COLORS, COLOR_PAIRS, cp.n_def, can_change_color()?"yes":"no");
	else
		mvwprintw(mywin -> win, line++, 2, "Terminal does not support colors.");
	if (term)
		mvwprintw(mywin -> win, line++, 2, "Terminal size: %dx%d, terminal: %s", max_x, max_y, term);
	else
		mvwprintw(mywin -> win, line++, 2, "Terminal size: %dx%d", max_x, max_y);

	if (beep_interval > 0)
		mvwprintw(mywin -> win, line++, 2, "Did %d beeps.", did_n_beeps);

	proc_u_line = line++;

	escape_print(mywin, 16, 2, "_Press any key to exit this screen_");

#if defined(__FreeBSD__) || defined(linux) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(sun) || defined(__sun) || defined(__GNU__) || defined(__CYGWIN__)
	for(;;)
	{
		dtime_t run_time = get_ts() - mt_started;
#ifndef __CYGWIN__
		double v1, v2, v3;

		get_load_values(&v1, &v2, &v3);
		mvwprintw(mywin -> win, 6, 2, "Current load of system: %f %f %f", v1, v2, v3);
#endif

		if (run_time)
		{
			struct rusage usage;

			if (getrusage(RUSAGE_SELF, &usage) == -1)
				error_exit(TRUE, FALSE, "getrusage() failed\n");

			mvwprintw(mywin -> win, proc_u_line, 2, "Runtime: %02d:%02d:%02d, avg.proc.usage: %.2f%% ints/s: %.1f",
					(int)(run_time / 3600), ((int)run_time / 60) % 60, (int)run_time % 60,
					((double)usage.ru_utime.tv_sec + (double)usage.ru_utime.tv_usec / 1000000.0 + 
					 (double)usage.ru_stime.tv_sec + (double)usage.ru_stime.tv_usec / 1000000.0) * 100.0 / run_time,
					 (double)total_wakeups / run_time);
		}

		mydoupdate();

		if (wait_for_keypress(-1, popup_refresh_interval, mywin, 0) != -1)
			break;
	}
#else
	mydoupdate();
	wait_for_keypress(-1, 0, mywin, 0);
#endif

	delete_popup(mywin);
}

void reset_counters(statistics_t *ps)
{
	memset(ps, 0x0, sizeof(*ps));

	ps -> sccfirst = 1;
}

void statistics_popup(int f_index, proginfo *cur)
{
	NEWWIN *popup = create_popup(16, 68);
	const char *title = "Statistics for ";
	char *abbr_fname = shorten_filename(cur -> filename, 54 - strlen(title));
	char buffer[54 + 1];

	snprintf(buffer, sizeof(buffer), "%s%s", title, abbr_fname);

	for(;;)
	{
		dtime_t time_running = get_ts() - cur -> statistics.start_ts;
		time_t start_ts = (time_t)cur -> statistics.start_ts;
		char *start_ts_str = mystrdup(ctime(&start_ts));
		time_t lastevent = (time_t)cur -> statistics.lastevent;
		char *last_ts_str  = mystrdup(ctime(&lastevent));
		char *dummy;
		char *vmsize_str = NULL;
		char *fsize_str = NULL;
		char *total_data_processed_str = amount_to_str(cur -> statistics.bytes_processed);
		char *buffer_kb;
		off64_t fsize = -1;
		int c;
		int total_re = 0;
		int loop;

		if (cur -> wt == WT_COMMAND)
		{
			vmsize_str = amount_to_str(get_vmsize(cur -> pid));
		}
		else if (cur -> wt == WT_FILE)
		{
			(void)file_info(cur -> filename, &fsize, 0, NULL, NULL);
			fsize_str = amount_to_str(fsize);
		}

		dummy = strchr(start_ts_str, '\n');
		if (dummy) *dummy = 0x00;
		dummy = strchr(last_ts_str, '\n');
		if (dummy) *dummy = 0x00;

		werase(popup -> win);
		win_header(popup, buffer);

		ui_inverse_on(popup);
		mvwprintw(popup -> win, 3, 2, "# lines       :");
		mvwprintw(popup -> win, 3, 27, "#l/s :");
		mvwprintw(popup -> win, 3, 44, "Avg len:");
		mvwprintw(popup -> win, 4, 2, "Data interval :");
		if (cur -> wt == WT_COMMAND)
			mvwprintw(popup -> win, 5, 2, "VM size       :");
		else if (cur -> wt == WT_FILE)
			mvwprintw(popup -> win, 5, 2, "File size     :");
		mvwprintw(popup -> win, 9, 2, "Data processed:");
		mvwprintw(popup -> win, 9, 27, "Bps  :");
		mvwprintw(popup -> win, 6, 2, "Started at    :");
		mvwprintw(popup -> win, 7, 2, "Last event    :");
		mvwprintw(popup -> win, 8, 2, "Next expected@:");
		mvwprintw(popup -> win, 10, 2, "# matched r.e.:");
		mvwprintw(popup -> win, 11, 2, "Buffered lines:");
		mvwprintw(popup -> win, 11, 27, "Bytes:");
		mvwprintw(popup -> win, 11, 44, "Limit  :");
		mvwprintw(popup -> win, 12, 2, "# of beeps:    ");
		if (cur -> wt == WT_COMMAND)
		{
			mvwprintw(popup -> win, 13, 2, "Number of runs:");
			mvwprintw(popup -> win, 13, 27, "Last rc:");
		}
		ui_inverse_off(popup);

		mvwprintw(popup -> win, 3, 18, "%d", cur -> statistics.n_events);
		mvwprintw(popup -> win, 6, 18, "%s", start_ts_str);
		if (cur -> statistics.lastevent != (dtime_t)0.0)
			mvwprintw(popup -> win, 7, 18, "%s", last_ts_str);
		else
			mvwprintw(popup -> win, 7, 18, "---");

		if (cur -> statistics.n_events == 0)
		{
			mvwprintw(popup -> win, 4, 18, "Not yet available");
		}
		else
		{
			double avg = cur -> statistics.med / (double)cur -> statistics.n_events;
			double dev = sqrt((cur -> statistics.dev / (double)cur -> statistics.n_events) - pow(avg, 2.0));

			/* serial correlation coefficient */
			double scct1 = cur -> statistics.scct1 + cur -> statistics.scclast * cur -> statistics.sccu0;
			double med = cur -> statistics.med * cur -> statistics.med;
			double scc = (double)cur -> statistics.n_events * cur -> statistics.dev - med;
			if (scc != 0.0)
			{
				scc = ((double)cur -> statistics.n_events * scct1 - med) / scc;
				mvwprintw(popup -> win, 4, 18, "average: %.2f, std.dev.: %.2f, SCC: %1.6f", avg, dev, scc);
			}
			else
				mvwprintw(popup -> win, 4, 18, "average: %.2f, std.dev.: %.2f, not correlated", avg, dev);

			if (avg)
			{
				double dummy_d = (double)(time(NULL) - cur -> statistics.lastevent) / avg;
				time_t next_event = cur -> statistics.lastevent + (ceil(dummy_d) * avg);
				char *ne_str = mystrdup(ctime(&next_event));
				char *dummy_str = strchr(ne_str, '\n');

				if (dummy_str) *dummy_str = 0x00;
				mvwprintw(popup -> win, 8, 18, "%s", ne_str); 
				myfree(ne_str);
			}

			mvwprintw(popup -> win, 3, 53, "%.1f", (double)cur -> statistics.bytes_processed / (double)cur -> statistics.n_events);
		}
		if (cur -> wt == WT_COMMAND)
			mvwprintw(popup -> win, 5, 18, "%s", vmsize_str);
		else if (cur -> wt == WT_STDIN || cur -> wt == WT_SOCKET)
			mvwprintw(popup -> win, 5, 18, "n.a.");
		else if (cur -> wt == WT_FILE)
			mvwprintw(popup -> win, 5, 18, "%s", fsize_str);
		myfree(vmsize_str);
		myfree(fsize_str);
		mvwprintw(popup -> win, 9, 18, "%s", total_data_processed_str);
		myfree(total_data_processed_str);
		if (time_running > 0)
		{
			char *bps_str = amount_to_str((double)cur -> statistics.bytes_processed / (double)time_running);
			mvwprintw(popup -> win, 9, 34, "%s", bps_str);
			myfree(bps_str);

			mvwprintw(popup -> win, 3, 34, "%.4f", (double)cur -> statistics.n_events / (double)time_running);
		}
		buffer_kb = amount_to_str(lb[f_index].curbytes);
		mvwprintw(popup -> win, 11, 18, "%d", lb[f_index].curpos);
		mvwprintw(popup -> win, 11, 34, "%s", buffer_kb);
		myfree(buffer_kb);
		mvwprintw(popup -> win, 12, 18, "%d", cur -> beep.did_n_beeps);
		escape_print(popup, 14, 2, "Press ^r^ to reset counters, ^q^ to exit");
		myfree(start_ts_str);
		myfree(last_ts_str);
		for(loop=0; loop<cur -> n_re; loop++)
			total_re += (cur -> pre)[loop].match_count;
		if (cur -> statistics.n_events)
			mvwprintw(popup -> win, 10, 18, "%d (%.2f%%)", total_re, (total_re * 100.0) / (double)cur -> statistics.n_events);
		else
			mvwprintw(popup -> win, 10, 18, "%d", total_re);
		if (cur -> wt == WT_COMMAND)
		{
			mvwprintw(popup -> win, 13, 18, "%d", cur -> n_runs);
			mvwprintw(popup -> win, 13, 36, "%d", cur -> last_exit_rc);
		}
		if (lb[f_index].maxnlines > 0)
		{
			mvwprintw(popup -> win, 11, 53, "%d lines", lb[f_index].maxnlines);
		}
		else if (lb[f_index].maxbytes > 0)
		{
			char *str = amount_to_str(lb[f_index].maxbytes);
			mvwprintw(popup -> win, 11, 53, "%s", str);
			myfree(str);
		}
		draw_border(popup);
		mydoupdate();

		c = toupper(wait_for_keypress(HELP_STATISTICS_POPUP, popup_refresh_interval, popup, 0));

		if (c == 'Q' || c == abort_key)
		{
			break;
		}
		else if (c == 'R')
		{
			reset_counters(&cur -> statistics);
		}
		else if (c !=  -1)
		{
			wrong_key();
		}
	}

	delete_popup(popup);
}

void statistics_menu(void)
{
	NEWWIN *mywin = create_popup(23, 65);
	int offset = 0, cur_line = 0;

	for(;;)
	{
		int c;
		int vmsize = get_vmsize(getpid());
		time_t now = time(NULL);
		struct tm *tmnow = localtime(&now);
		proginfo **plist = NULL;
		char     *issub = NULL;
		int      *winnr = NULL;
		int loop, nwin = 0;

		/* create list of (sub-)windows */
		for(loop=0; loop<nfd; loop++)
		{
			proginfo *cur = &pi[loop];

			while(cur)
			{
				plist = (proginfo **)myrealloc(plist, (nwin + 1) * sizeof(proginfo *));
				issub = (char *)     myrealloc(issub, (nwin + 1) * sizeof(char)      );
				winnr = (int *)      myrealloc(winnr, (nwin + 1) * sizeof(int)       );

				plist[nwin] = cur;
				issub[nwin] = (cur != &pi[loop]);
				winnr[nwin] = loop;
				nwin++;

				cur = cur -> next;
			}
		}

		werase(mywin -> win);
		win_header(mywin, "Statistics");

		for(loop=0; loop<18; loop++)
		{
			int cur_index = loop + offset;
			int is_sub_indent;

			if (cur_index >= nwin) break;

			is_sub_indent = issub[cur_index];

			if (loop == cur_line) ui_inverse_on(mywin);
			if (is_sub_indent)
				mvwprintw(mywin -> win, 2 + loop, 7, "%s", shorten_filename(plist[cur_index] -> filename, 54));
			else
				mvwprintw(mywin -> win, 2 + loop, 2, "[%02d] %s", winnr[cur_index], shorten_filename(plist[cur_index] -> filename, 56));
			if (loop == cur_line) ui_inverse_off(mywin);
		}

		mvwprintw(mywin -> win, 20, 2, "Run-time: %.2f hours  %02d:%02d", (get_ts() - mt_started) / 3600.0, tmnow -> tm_hour, tmnow -> tm_min);
		if (vmsize != -1)
		{
			char *vmsize_str = amount_to_str(vmsize);
			mvwprintw(mywin -> win, 20, 35, "Memory usage: %s", vmsize_str);
			myfree(vmsize_str);
		}
		escape_print(mywin, 21, 2, "Press ^r^ to reset counters, ^q^ to exit");

		draw_border(mywin);
		mydoupdate();

		c = toupper(wait_for_keypress(HELP_STATISTICS, popup_refresh_interval, mywin, 1));

		if (c == 'R')
		{
			for(loop=0; loop<nfd; loop++)
			{
				proginfo *cur = &pi[loop];

				while(cur)
				{
					reset_counters(&cur -> statistics);

					cur = cur -> next;
				}
			}
		}
		else if (c == KEY_UP)
		{
			if (cur_line)
				cur_line--;
			else if (offset)
				offset--;
			else
				wrong_key();
		}
		else if (c == KEY_DOWN)
		{
			if ((cur_line + offset) < (nwin - 1))
			{
				if (cur_line < (18 - 1))
					cur_line++;
				else
					offset++;
			}
			else
				wrong_key();

		}
		else if (c == 13 || c == ' ')
		{
			statistics_popup(winnr[cur_line + offset], plist[cur_line + offset]);
		}
		else if (c == 'Q' || c == abort_key)
		{
			myfree(plist);
			myfree(issub);
			myfree(winnr);
			break;
		}
		else if (c != -1)
		{
			wrong_key();
		}

		myfree(plist);
		myfree(issub);
		myfree(winnr);
	}

	delete_popup(mywin);
}

void heartbeat(void)
{
	time_t now = time(NULL);
	struct tm *ptm = localtime(&now);
	static int x = 0, y = 0, dx = 1, dy = 1;
	static NEWWIN *hb_win = NULL;

	x += dx;
	y += dy;

	if (x >= (max_x - 8))
	{
		dx = -(myrand(1) + 1);
		x = max_x - (8 + 1);
	}
	else if (x < 0)
	{
		dx = (myrand(2) + 1);
		x = 0;
	}

	if (y >= max_y)
	{
		dy = -(myrand(2) + 1);
		y = max_y - 1;
	}
	else if (y < 0)
	{
		dy = (myrand(2) + 1);
		y = 0;
	}

	if (dx == 0 && dy == 0)
	{
		dy = 1;
		dy = -1;
	}

	if (!hb_win)
	{
		hb_win = create_popup(1, 8);
	}

	move_panel(hb_win -> pwin, y, x);
	ui_inverse_on(hb_win);
	mvwprintw(hb_win -> win, 0, 0, "%02d:%02d:%02d", ptm -> tm_hour, ptm -> tm_min, ptm -> tm_sec);
	ui_inverse_off(hb_win);
	mydoupdate();
}

void do_check_for_mail(dtime_t time)
{
	if (check_for_mail > 0 && mail_spool_file != NULL)
	{
		/* get current filesize */
		if (stat64(mail_spool_file, &msf_info) == -1)
		{
			if (errno != ENOENT) 
			{
				check_for_mail = 0;
				error_popup("Check for new e-mail", -1, "Error doing stat64() on file %s.\ne-Mail check disabled.\n", mail_spool_file);
			}
		}

		/* filesize changed? */
		if (msf_info.st_size != msf_prev_size)
		{
			/* file became bigger: new mail
			 * if it became less, the file changed because
			 * mail was deleted or so
			 */
			if (msf_info.st_size > msf_prev_size)
			{
				mail = 1;

				redraw_statuslines();

				if (get_do_refresh() != 2)
					set_do_refresh(1);
			}

			msf_prev_size = msf_info.st_size;
		}
	}
}

void store_statistics(proginfo *cur, dtime_t now)
{
	if (cur -> statistics.lastevent)
	{
		dtime_t cur_deltat = now - cur -> statistics.lastevent;

		if (cur -> statistics.n_events == 1)
		{
			cur -> statistics.total_deltat += (cur_deltat - cur -> statistics.prev_deltat);
			cur -> statistics.prev_deltat = cur_deltat;
		}

		cur -> statistics.med += cur_deltat;
		cur -> statistics.dev += pow(cur_deltat, 2.0);
		cur -> statistics.n_events++;

		/* Update calculation of serial correlation coefficient */
		/* (also uses cur -> med/dev) */
		if (cur -> statistics.sccfirst)
		{
			cur -> statistics.sccfirst = 0;
			cur -> statistics.scclast = 0;
			cur -> statistics.sccu0 = cur_deltat;
		}
		else
			cur -> statistics.scct1 = cur -> statistics.scct1 + cur -> statistics.scclast * cur_deltat;
		cur -> statistics.scclast = cur_deltat;
	}

	cur -> statistics.lastevent = now;
}
