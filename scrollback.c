#define _LARGEFILE64_SOURCE	/* required for GLIBC to enable stat64 and friends */
#include <ctype.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "error.h"
#include "my_pty.h"
#include "utils.h"
#include "term.h"
#include "help.h"
#include "mem.h"
#include "ui.h"
#include "misc.h"
#include "globals.h"

int scrollback_search_to_new_window(buffer *pbuf, char *org_title, char *find_str, mybool_t case_insensitive);

int find_string(buffer *pbuf, char *find, int offset, char direction, mybool_t case_insensitive)
{
	int loop, index = -1, rc;
	regex_t regex;

	/* compile the searchstring (which can be a regular expression) */
	if ((rc = regcomp(&regex, find, REG_EXTENDED | (case_insensitive == MY_TRUE?REG_ICASE:0))))
	{
		regexp_error_popup(rc, &regex);

		return -1;	/* failed -> not found */
	}

	if (direction == 1)
	{
		for(loop=offset; loop<pbuf -> curpos; loop++)
		{
			if ((pbuf -> be)[loop].Bline == NULL)
				continue;

			if (regexec(&regex, (pbuf -> be)[loop].Bline, 0, NULL, 0) == 0)
			{
				index = loop;
				break;
			}
		}
	}
	else if (direction == (char)-1)
	{
		for(loop=offset; loop>=0; loop--)
		{
			if ((pbuf -> be)[loop].Bline == NULL)
				continue;

			if (regexec(&regex, (pbuf -> be)[loop].Bline, 0, NULL, 0) == 0)
			{
				index = loop;
				break;
			}
		}
	}

	regfree(&regex);

	return index;
}

void scrollback_find_popup(char **find_str, mybool_t *pcase_insensitive)
{
	char *dummy;
	NEWWIN *mywin = create_popup(5, 40);

	win_header(mywin, "Find");

	dummy = edit_string(mywin, 3, 2, 40, 80, 0, reuse_searchstring?*find_str:NULL, HELP_SCROLLBACK_EDIT_SEARCH_STRING, -1, &search_h, pcase_insensitive);
	myfree(*find_str);
	*find_str = dummy;

	delete_popup(mywin);
}

void scrollback_savefile(buffer *pbuf)
{
	char *file = NULL;
	NEWWIN *mywin = create_popup(8, 40);

	win_header(mywin, "Save buffer to file");

	mvwprintw(mywin -> win, 4, 2, "Select file");
	file = edit_string(mywin, 5, 2, 40, find_path_max(), 0, NULL, HELP_SCROLLBACK_SAVEFILE_ENTER_FILENAME, -1, &cmdfile_h, NULL);
	if (file)
	{
		FILE *fh = fopen(file, "w");
		if (fh)
		{
			int loop;

			for(loop=0; loop<pbuf -> curpos; loop++)
			{
				if ((pbuf -> be)[loop].Bline)
				{
					char display = 1;
					char *error;
					int dummy = -1;
					regmatch_t *pmatch = NULL;

					/* check filter */
					if (!IS_MARKERLINE((pbuf -> be)[loop].pi))
					{
						(void)check_filter((pbuf -> be)[loop].pi, (pbuf -> be)[loop].Bline, &pmatch, &error, &dummy, 0, &display);
						if (error)
						{
							fprintf(fh, "%s\n", error);
							myfree(error);
						}
					}
					if (display)
					{
						fprintf(fh, "%s\n", USE_IF_SET((pbuf -> be)[loop].Bline, ""));
					}

					if (pmatch) myfree(pmatch);
				}
			}

			fclose(fh);
		}
		else
		{
			error_popup("Save scrollback buffer", -1, "Cannot write to file, reason: %s", strerror(errno));
		}
	}

	delete_popup(mywin);
}

int get_lines_needed(char *string, int terminal_width)
{
	if (string)
		return (strlen(string) + terminal_width - 1) / terminal_width;
	else
		return 1;
}

void scrollback_displayline(int window_nr, NEWWIN *win, buffer *pbuf, int buffer_offset, int terminal_offset, int offset_in_line, mybool_t force_to_winwidth, char show_winnr)
{
	char *cur_line = (pbuf -> be)[buffer_offset].Bline;

	wmove(win -> win, terminal_offset, 0);

	if (cur_line)
	{
		proginfo *cur_line_meta = (pbuf -> be)[buffer_offset].pi;
		double ts = (pbuf -> be)[buffer_offset].ts;
		char old_color_settings = 0;

		if (scrollback_no_colors && cur_line_meta != NULL)
		{
			old_color_settings = cur_line_meta -> cdef.colorize;
			cur_line_meta -> cdef.colorize = 0;
		}
		if (IS_MARKERLINE(cur_line_meta))
		{
			color_print(window_nr, win, cur_line_meta, cur_line, NULL, -1, MY_FALSE, 0, 0, ts, show_winnr);
		}
		else /* just a buffered line */
		{
			char *error = NULL;
			regmatch_t *pmatch = NULL;
			int matching_regex = -1;
			char display;
			(void)check_filter(cur_line_meta, cur_line, &pmatch, &error, &matching_regex, 0, &display);
			if (error)
			{
				color_print(window_nr, win, cur_line_meta, error, NULL, -1, MY_FALSE, 0, 0, ts, show_winnr);
				myfree(error);
			}
			if (display)
			{
				if (offset_in_line)
				{
					int line_len = strlen(cur_line);
					int new_size = 0;

					if (offset_in_line < line_len)
						new_size = min(win -> width, line_len - offset_in_line);

					color_print(window_nr, win, cur_line_meta, cur_line, pmatch, matching_regex, force_to_winwidth, offset_in_line, offset_in_line + new_size, ts, show_winnr);
				}
				else
				{
					color_print(window_nr, win, cur_line_meta, cur_line, pmatch, matching_regex, force_to_winwidth, 0, 0, ts, show_winnr);
				}
			}

			myfree(pmatch);
		}
		if (scrollback_no_colors && cur_line_meta != NULL)
		{
			cur_line_meta -> cdef.colorize = old_color_settings;
		}
	}
	else /* an empty line */
	{
		/* do nothing */
	}
}

int scrollback_do(int window_nr, buffer *pbuf, int *winnrs, char *header)
{
	int rc = 0;
	char *find = NULL;
	NEWWIN *mywin1, *mywin2;
	int nlines = max_y - 6, ncols = max_x - 6;
	int offset = max(0, pbuf -> curpos - nlines); // FIXME: aantal regels laten afhangen van lengte
	char redraw = 2;
	int line_offset = 0;
	char show_winnr = default_sb_showwinnr;
	mybool_t case_insensitive = re_case_insensitive;
	buffer cur_lb;
	int loop;

	memset(&cur_lb, 0x00, sizeof(cur_lb));

	for(loop=0; loop<pbuf -> curpos; loop++)
	{
		if ((pbuf -> be)[loop].Bline == NULL)
			continue;

		cur_lb.be = myrealloc(cur_lb.be, (cur_lb.curpos + 1) * sizeof(buffered_entry));
		cur_lb.be[cur_lb.curpos].pi    = (pbuf -> be)[loop].pi;
		if ((pbuf -> be)[loop].pi != NULL && (!IS_MARKERLINE((pbuf -> be)[loop].pi)) && (pbuf -> be)[loop].pi -> cdef.term_emul != TERM_IGNORE)
		{
			color_offset_in_line *cmatches;
			int n_cmatches;
			cur_lb.be[cur_lb.curpos].Bline = emulate_terminal((pbuf -> be)[loop].Bline, &cmatches, &n_cmatches);
			myfree(cmatches);
		}
		else
			cur_lb.be[cur_lb.curpos].Bline = strdup((pbuf -> be)[loop].Bline);
		cur_lb.be[cur_lb.curpos].ts    = (pbuf -> be)[loop].ts;
		cur_lb.curpos++;
	}

	LOG("---\n");
	if (global_highlight_str)
	{
		find = mystrdup(global_highlight_str);
	}

	mywin1 = create_popup(max_y - 4, max_x - 4);

	mywin2 = create_popup(nlines, ncols);
	scrollok(mywin2 -> win, FALSE); /* supposed to always return OK, according to the manpage */

	for(;;)
	{
		int c, uc;

		if (redraw == 2)
		{
			int index = 0;
			int lines_used = 0;

			ui_inverse_on(mywin1);
			mvwprintw(mywin1 -> win, nlines + 1, 1, "%s - %d buffered lines", shorten_filename(header, max(24, ncols - 24)), cur_lb.curpos);
			ui_inverse_off(mywin1);

			if (!no_linewrap) ui_inverse_on(mywin1);
			mvwprintw(mywin1 -> win, nlines + 1, ncols - 8, "LINEWRAP");
			if (!no_linewrap) ui_inverse_off(mywin1);

			werase(mywin2 -> win);

			if (!no_linewrap && line_offset > 0)
			{
				int temp_line_offset = line_offset;
				int n_chars_to_display_left = strlen((cur_lb.be)[offset].Bline) - temp_line_offset;

				while(lines_used < nlines && n_chars_to_display_left > 0)
				{
					scrollback_displayline(winnrs?winnrs[offset]:window_nr, mywin2, &cur_lb, offset, lines_used, temp_line_offset, 1, show_winnr);

					temp_line_offset += ncols;
					n_chars_to_display_left -= ncols;

					lines_used++;
				}

				index++;
			}

			for(;(offset + index) < cur_lb.curpos && lines_used < nlines;)
			{
				int lines_needed = get_lines_needed((cur_lb.be)[offset + index].Bline, ncols);

				if (no_linewrap || lines_needed == 1)
				{
					scrollback_displayline(winnrs?winnrs[offset + index]:window_nr, mywin2, &cur_lb, offset + index, lines_used, no_linewrap?line_offset:0, no_linewrap, show_winnr);
					lines_used++;
				}
				else
				{
					int cur_line_offset = 0;

					while(lines_used < nlines && lines_needed > 0)
					{
						scrollback_displayline(winnrs?winnrs[offset + index]:window_nr, mywin2, &cur_lb, offset + index, lines_used, cur_line_offset, 1, show_winnr);
						cur_line_offset += ncols;
						lines_used++;
						lines_needed--;
					}
				}

				index++;
			}

			redraw = 1;
		}

		if (redraw == 1)
		{
			mydoupdate();

			redraw = 0;
		}

		c = wait_for_keypress(HELP_SCROLLBACK_HELP, 0, NULL, 1);
		uc = toupper(c);

		if (c == 'q' || uc == 'X' || c == abort_key || c == KEY_CLOSE || c == KEY_EXIT)
		{
			break;
		}
		else if (c == 'Q' || c == -1)		/* Q: close whole stack of scrollbackwindows, -1: something got closed */
		{
			rc = -1;
			break;
		}
		else if (c == 20 && winnrs != NULL)	/* ^t */
		{
			show_winnr = 1 - show_winnr;
			redraw = 2;
		}
		else if (c == 'Y')
		{
			no_linewrap = !no_linewrap;
			redraw = 2;
			line_offset = 0;
		}
		else if (c == 't')
		{
			statistics_menu();
		}
		else if ((c == KEY_LEFT ||
					c == KEY_BACKSPACE)
				&& no_linewrap)
		{
			if (line_offset > 0)
				line_offset--;

			redraw = 2;
		}
		else if (c == KEY_SLEFT && no_linewrap)
		{
			if (line_offset >= (ncols / 2))
				line_offset -= (ncols / 2);
			else
				line_offset = 0;

			redraw = 2;
		}
		else if (c == KEY_SRIGHT && no_linewrap)
		{
			line_offset += (ncols / 2);

			redraw = 2;
		}
		else if (c == KEY_BEG && no_linewrap)
		{
			if (line_offset)
			{
				line_offset = 0;
				redraw = 2;
			}
		}
		else if (c == KEY_BTAB)
		{
			if (line_offset >= 4)
				line_offset -= 4;
			else
				line_offset = 0;

			redraw = 2;
		}
		else if (c == KEY_RIGHT && no_linewrap)
		{
			line_offset++;
			redraw = 2;
		}
		else if ((c == KEY_UP ||
					c == 'y' ||
					c == 25  || /* ^y */
					c == 'k' ||
					/* c == 11  || *//* ^k */
					c == 16)    /* ^p */
				&& (offset > 0 || (!no_linewrap && line_offset > 0)))
		{
			if (no_linewrap)
			{
				offset--;
			}
			else if (line_offset > 0)
			{
				line_offset = max(0, line_offset - ncols);
			}
			else
			{
				offset--;

				line_offset = (get_lines_needed((cur_lb.be)[offset].Bline, ncols) - 1) * ncols;
			}

			wmove(mywin2 -> win, 0, 0);
			winsdelln(mywin2 -> win, 1);

			scrollback_displayline(winnrs?winnrs[offset]:window_nr, mywin2, &cur_lb, offset, 0, line_offset, no_linewrap, show_winnr);

			redraw = 1;
		}
		else if ((c == KEY_DOWN ||
					c == 'e' ||
					c == 5   || /* ^e */
					c == 'j' ||
					c == 14  || /* ^n */
					c == 13  ||
					c == KEY_ENTER)
				&& offset < (cur_lb.curpos - 1))
		{
			if (no_linewrap)
			{
				offset++;
			}
			else if (strlen((cur_lb.be)[offset].Bline) > (line_offset + ncols))
			{
				line_offset += ncols;
			}
			else if (offset < (cur_lb.curpos - 1))
			{
				if (strlen((cur_lb.be)[offset].Bline) > (line_offset + ncols))
					line_offset += ncols;
				else
				{
					line_offset = 0;
					offset++;
				}
			}

			redraw = 2;
		}
		else if ((c == KEY_NPAGE ||
					c == 'f' ||
					c == 6   || /* ^f */
					c == ('V' - 65 + 1) || /* ^v */
					c == ' ' ||
					c == 'z' ||
					c == 'u' ||
					c == ('U' - 65 + 1))   /* ^u */
				&& offset < (cur_lb.curpos - 1))
		{
			if (no_linewrap)
			{
				offset += nlines;
				if (offset >= cur_lb.curpos)
					offset = cur_lb.curpos - 1;
			}
			else
			{
				int n_lines_to_move = nlines;

				while(n_lines_to_move > 0 && offset < (cur_lb.curpos))
				{
					if (line_offset > 0)
					{
						if (line_offset + ncols >= strlen((cur_lb.be)[offset].Bline))
						{
							line_offset = 0;
							offset++;
							n_lines_to_move--;
						}
						else
						{
							line_offset += ncols;
							n_lines_to_move--;
						}
					}
					else
					{
						n_lines_to_move -= get_lines_needed((cur_lb.be)[offset].Bline, ncols);
						offset++;
					}
				}

				if (n_lines_to_move < 0)
					line_offset = (-n_lines_to_move) * ncols;
			}

			redraw = 2;
		}
		else if ((c == KEY_PPAGE ||
					c == 'b' ||
					c == 2   ||     /* ^b */
					c == 'w' ||
					c == 'd' ||
					c == 4)         /* ^d */
				&& offset > 0)
		{
			if (no_linewrap)
			{
				offset -= nlines;
				if (offset < 0)
					offset = 0;
			}
			else
			{
				int n_lines_to_move = nlines;

				if (line_offset)
					n_lines_to_move -= line_offset / ncols;

				while(n_lines_to_move > 0 && offset > 0)
				{
					offset--;

					n_lines_to_move -= get_lines_needed((cur_lb.be)[offset].Bline, ncols);

					if (n_lines_to_move < 0)
					{
						line_offset = (get_lines_needed((cur_lb.be)[offset].Bline, ncols) + n_lines_to_move) * ncols;
					}
				}
			}

			redraw = 2;
		}
		else if ((c == KEY_HOME ||
					c == 'g' ||
					c == '<' ||
					c == KEY_SBEG)
				&& offset > 0)
		{
			line_offset = offset = 0;
			redraw = 2;
		}
		else if ((c == KEY_END ||
					c == 'G' ||
					c == '>' ||
					c == KEY_SEND)
				&& offset < (cur_lb. curpos - 1))
		{
			offset = cur_lb. curpos - 1;
			redraw = 2;
		}
		else if (uc == 'R' || c == ('R' - 65 + 1) || c == ('L' - 65 + 1) || c == KEY_REFRESH)
		{
			redraw = 2;
		}
		else if (c == ('K' - 65 + 1) || c == KEY_MARK)
		{
			scrollback_find_popup(&find, &case_insensitive);

			if (find)
			{
				int rc;

				regfree(&global_highlight_re);
				myfree(global_highlight_str);
				global_highlight_str = NULL;

				if ((rc = regcomp(&global_highlight_re, find, (case_insensitive == MY_TRUE?REG_ICASE:0) | REG_EXTENDED)))
				{
					regexp_error_popup(rc, &global_highlight_re);
					myfree(find);
				}
				else
				{
					global_highlight_str = find;
				}

				redraw = 2; /* force redraw */
			}

		}
		else if (c == 'f' || c == '/' || c == '?' || c == KEY_FIND || c == KEY_SFIND)
		{
			char direction = (c == '?' || c == KEY_SFIND) ? -1 : 1;

			scrollback_find_popup(&find, &case_insensitive);

			if (find)
			{
				if (scrollback_search_new_window)
				{
					if (scrollback_search_to_new_window(&cur_lb, header, find, case_insensitive) == -1)
					{
						/* cascaded close */
						rc = -1;
						break;
					}
				}
				else
				{
					int new_f_index;

					redraw = 2; /* force redraw */

					regfree(&global_highlight_re);
					myfree(global_highlight_str);
					global_highlight_str = NULL;

					new_f_index = find_string(&cur_lb, find, 0, direction, case_insensitive);
					if (new_f_index == -1)
					{
						wrong_key();
					}
					else
					{
						offset = new_f_index;
						line_offset = 0;
					}
				}
			}
		}
		else if (uc == 'N' || c == KEY_NEXT || c == KEY_PREVIOUS || c == KEY_SNEXT)
		{
			if (find != NULL)
			{
				char direction = (c == 'n' || c == KEY_NEXT) ? 1 : -1;
				int start_offset = offset + direction;
				int new_f_index = find_string(&cur_lb, find, start_offset, direction, case_insensitive);
				if (new_f_index == -1)
				{
					wrong_key();
				}
				else
				{
					redraw = 2; /* force redraw */
					offset = new_f_index;
					line_offset = 0;
				}
			}
			else
			{
				wrong_key();
			}
		}
		else if (c == 's' || c == KEY_SAVE)
		{
			scrollback_savefile(&cur_lb);
			redraw = 2;	/* force redraw */
		}
		else if (c == 'h')
		{
			show_help(HELP_SCROLLBACK_HELP);
		}
		else if (c == 'c')
		{
			toggle_colors();
			redraw = 2;	/* force redraw */
		}
		else if (c == 'i')
		{
			info();
		}
		else if (c == 'T')
		{
			statistics_menu();
		}
		else if (c == 20)
		{
			toggle_subwindow_nr();
			redraw = 2;	/* force redraw */
		}
		else
		{
			wrong_key();
		}
	}

	delete_popup(mywin2);
	delete_popup(mywin1);

	myfree(find);

	delete_be_in_buffer(&cur_lb);

	return rc;
}

void scrollback(void)
{
	int window = 0;

	if (nfd > 1)
	{
		window = select_window(HELP_SCROLLBACK_SELECT_WINDOW, NULL);
	}

	if (window != -1)
	{
		if (lb[window].bufferwhat == 0)
			error_popup("Scrollback", HELP_SCROLLBACK_NO_MARK, "Cannot scrollback: buffering is disabled.");
	}

	if (window != -1 && lb[window].bufferwhat != 0)
	{
		int header_size = strlen(pi[window].filename) + 4;
		char *header = (char *)mymalloc(header_size + 1);
		snprintf(header, header_size, "%02d] %s", window, pi[window].filename);
		scrollback_do(window, &lb[window], NULL, header);
		myfree(header);
	}
}

void merged_scrollback_with_search(char *search_for, mybool_t case_insensitive)
{
	int lc_size = nfd * sizeof(int);
	int *last_check = (int *)mymalloc(lc_size);
	int *winnr = NULL;
	buffer buf;
	regex_t reg;
	int rc;

	memset(last_check, 0x00, lc_size);
	memset(&buf, 0x00, sizeof(buf));

	/* compile the search string which is supposed to be a valid regular
	 * expression
	 */
	if (search_for)
	{
		if ((rc=regcomp(&reg, search_for, REG_EXTENDED | (case_insensitive == MY_TRUE?REG_ICASE:0))))
		{
			regexp_error_popup(rc, &reg);
			free(last_check);
			return;
		}
	}

	/* merge all windows into one */
	for(;;)
	{
		int loop;
		double chosen_ts = (double)(((long long int)1) << 62);
		int chosen_win = -1;
		int curline;
		char *string;
		int checked_all = 0;

		for(loop=0; loop<nfd; loop++)
		{
			if (lb[loop].curpos == last_check[loop])
			{
				checked_all++;
				continue;
			}

			if (search_for != NULL && lb[loop].be[last_check[loop]].Bline != NULL)
			{
				rc = regexec(&reg, lb[loop].be[last_check[loop]].Bline, 0, NULL, 0);

				/* did not match? don't add and continue */
				if (rc == REG_NOMATCH)
				{
					continue;
				}

				/* is it an error? then popup and abort */
				if (rc != 0)
				{
					regexp_error_popup(rc, &reg);
					break;
				}
			}

			if (lb[loop].be[last_check[loop]].ts <= chosen_ts)
			{
				chosen_ts = lb[loop].be[last_check[loop]].ts;
				chosen_win = loop;
			}
		}

		if (chosen_win == -1)
		{
			if (checked_all == nfd)
				break;

			for(loop=0; loop<nfd; loop++)
			{
				if (lb[loop].curpos > last_check[loop])
					last_check[loop]++;
			}

			continue;
		}

		if (!IS_MARKERLINE(lb[chosen_win].be[last_check[chosen_win]].pi))
		{
			/*** ADD LINE TO BUFFER ***/
			buf.be = (buffered_entry *)myrealloc(buf.be, sizeof(buffered_entry) * (buf.curpos + 1));
			winnr  = (int *)myrealloc(winnr, sizeof(int) * (buf.curpos + 1));
			curline = buf.curpos++;
			/* add the logline itself */
			string = lb[chosen_win].be[last_check[chosen_win]].Bline;
			if (string)
				buf.be[curline].Bline = mystrdup(string);
			else
				buf.be[curline].Bline = NULL;
			/* remember pointer to subwindow (required for setting colors etc.) */
			buf.be[curline].pi = lb[chosen_win].be[last_check[chosen_win]].pi;
			buf.be[curline].ts = lb[chosen_win].be[last_check[chosen_win]].ts;
			/* remember window nr. */
			winnr[curline] = chosen_win;
		}

		last_check[chosen_win]++;
	}

	if (buf.curpos == 0)
		error_popup("Search in all windows", -1, "Nothing found.");
	else
	{
		char *header;

		if (search_for)
		{
			char *help = "Searched for: ";
			int len = strlen(help) + strlen(search_for) + 1;
			header = mymalloc(len);
			snprintf(header, len, "%s%s", help, search_for);
		}
		else
		{
			char *help = "Merge view";
			header = mymalloc(strlen(help) + 1);
			sprintf(header, "%s", help);
		}

		scrollback_do(-1, &buf, winnr, header);

		myfree(header);
	}

	delete_be_in_buffer(&buf);

	myfree(winnr);

	myfree(last_check);
}

int scrollback_search_to_new_window(buffer *pbuf, char *org_title, char *find_str, mybool_t case_insensitive)
{
	int loop, rc;
	regex_t regex;
	buffer cur_lb;
	char *new_header;

	/* compile the searchstring (which can be a regular expression) */
	if ((rc = regcomp(&regex, find_str, REG_EXTENDED | (case_insensitive == MY_TRUE?REG_ICASE:0))))
	{
		regexp_error_popup(rc, &regex);
		return 0;
	}

	memset(&cur_lb, 0x00, sizeof(buffer));

	for(loop=0; loop<pbuf -> curpos; loop++)
	{
		if ((pbuf -> be)[loop].Bline == NULL)
			continue;

		if (regexec(&regex, (pbuf -> be)[loop].Bline, 0, NULL, 0) == 0)
		{
			cur_lb.be = myrealloc(cur_lb.be, (cur_lb.curpos + 1) * sizeof(buffered_entry));
			cur_lb.be[cur_lb.curpos].Bline = (pbuf -> be)[loop].Bline;
			cur_lb.be[cur_lb.curpos].pi    = (pbuf -> be)[loop].pi;
			cur_lb.be[cur_lb.curpos].ts    = (pbuf -> be)[loop].ts;
			cur_lb.curpos++;
		}
	}

	new_header = (char *)mymalloc(strlen(org_title) + 1 + strlen(find_str) + 1);
	sprintf(new_header, "%s %s", org_title, find_str);

	rc = scrollback_do(-1, &cur_lb, NULL, new_header);

	myfree(new_header);

	myfree(cur_lb.be);

	regfree(&regex);

	return rc;
}
