#define _LARGEFILE64_SOURCE     /* required for GLIBC to enable stat64 and friends */
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "help.h"
#include "mem.h"
#include "error.h"
#include "term.h"
#include "utils.h"
#include "selbox.h"
#include "globals.h"


int edit_stripper_edit_regexp(NEWWIN *win, char **old_re, mybool_t *case_insensitive)
{
	int changed = 0;
	char *new_re;

	/* ask new reg exp */
	mvwprintw(win -> win, 15, 2, "Edit regular expression:          ");
	new_re = edit_string(win, 16, 2, 58, 128, 0, *old_re, HELP_ENTER_REGEXP, -1, &search_h, case_insensitive);

	if (new_re)
	{
		myfree(*old_re);
		*old_re = new_re;
		changed = 1;
	}

	return changed;
}

int edit_stripper_edit_offsets(NEWWIN *win, int *offset_start, int *offset_end)
{
	char *str_start = NULL, *str_end = NULL;
	char buffer[32] = { 0 };
	char linebuf[68 + 1];
	int changed = 0;

	if (*offset_start != -1)
		snprintf(buffer, sizeof(buffer), "%d", *offset_start);

	mvwprintw(win -> win, 15, 2, "Enter start offset:          ");
	str_start = edit_string(win, 16, 2, 5, 5, 1, buffer, HELP_STRIPPER_START_OFFSET, -1, NULL, NULL);
	if (str_start)
	{
		if (*offset_end != -1)
			snprintf(buffer, sizeof(buffer), "%d", *offset_end);
		else
			buffer[0] = 0x00;

		mvwprintw(win -> win, 15, 2, "Enter end offset:            ");
		memset(linebuf, ' ', sizeof(linebuf));
		linebuf[sizeof(linebuf) - 1] = 0x00;
		mvwprintw(win -> win, 16, 1, "%s", linebuf);
		str_end = edit_string(win, 16, 2, 5, 5, 1, buffer, HELP_STRIPPER_END_OFFSET, -1, NULL, NULL);
		if (str_end)
		{
			*offset_start = atoi(str_start);
			*offset_end   = atoi(str_end);

			changed = 1;
		}
	}

	myfree(str_start);
	myfree(str_end);

	return changed;
}

int edit_stripper_edit_field(NEWWIN *win, char **del, int *col_nr)
{
	int changed = 0;
	char *new_del = NULL;
	char *new_col_nr = NULL;
	char buffer[32] = { 0 };
	char linebuf[58 + 1];

	mvwprintw(win -> win, 15, 2, "Enter delimiter:              ");
	new_del = edit_string(win, 16, 2, 58, 128, 0, *del, HELP_STRIPPER_DELIMITER, -1, NULL, NULL);
	if (new_del)
	{

		mvwprintw(win -> win, 15, 2, "Enter column number:              ");
		memset(linebuf, ' ', sizeof(linebuf));
		linebuf[sizeof(linebuf) - 1] = 0x00;
		mvwprintw(win -> win, 16, 1, "%s", linebuf);

		if (*col_nr != -1)
			snprintf(buffer, sizeof(buffer), "%d", *col_nr);

		new_col_nr = edit_string(win, 16, 2, 5, 5, 1, buffer, HELP_STRIPPER_COL_NR, -1, NULL, NULL);
	}

	if (new_del != NULL && new_col_nr != NULL)
	{
		*del = mystrdup(new_del);
		*col_nr = atoi(new_col_nr);
		changed = 1;
	}

	myfree(new_col_nr);
	myfree(new_del);

	return changed;
}

int edit_strippers(void)
{
	int changed = 0;
	NEWWIN *mywin;
	proginfo *cur;
	int f_index = 0;
	int cur_strip = 0;
	mybool_t case_insensitive = re_case_insensitive;

	/* select window */
	if (nfd > 1)
		f_index = select_window(HELP_ENTER_STRIPPER_SELECT_WINDOW, "Select window (stripper editing)");

	if (f_index == -1)	/* user pressed Q/X */
		return 0;

	/* select subwindow */
	cur = &pi[f_index];
	if (cur -> next)
		cur = select_subwindow(f_index, HELP_ENTER_STRIPPER_SELECT_SUBWINDOW, "Select subwindow (stripper editing)");
	if (!cur)
		return 0;

	/* create window */
	mywin = create_popup(23, 70);

	for(;;)
	{
		int c, key;
		int loop;
		char linebuf[68 + 1];

		werase(mywin -> win);
		draw_border(mywin);
		win_header(mywin, "Edit strip regular expressions");
		mvwprintw(mywin -> win, 2, 2, "%s", cur -> filename);
		escape_print(mywin, 3, 2, "^a^dd, ^e^dit, ^d^elete, ^q^uit");

		/* clear */
		memset(linebuf, ' ', sizeof(linebuf) - 1);
		linebuf[sizeof(linebuf) - 1] = 0x00;
		for(loop=4; loop<22; loop++)
			mvwprintw(mywin -> win, loop, 1, linebuf);

		/* display them lines */
		for(loop=0; loop<cur -> n_strip; loop++)
		{
			if (loop == cur_strip) ui_inverse_on(mywin);

			switch((cur -> pstrip)[loop].type)
			{
				case STRIP_KEEP_SUBSTR:
				case STRIP_TYPE_REGEXP:
					strncpy(linebuf, (cur -> pstrip)[loop].regex_str, 55);
					linebuf[55] =  0x00;
					mvwprintw(mywin -> win, 4 + loop, 1, "RE: %s", linebuf);
					break;

				case STRIP_TYPE_RANGE:
					mvwprintw(mywin -> win, 4 + loop, 1, "Range: %d - %d", (cur -> pstrip)[loop].start, (cur -> pstrip)[loop].end);
					break;

				case STRIP_TYPE_COLUMN:
					mvwprintw(mywin -> win, 4 + loop, 1, "Column: %d (delimiter: '%s')", (cur -> pstrip)[loop].col_nr, (cur -> pstrip)[loop].del);
					break;
			}

			if (loop == cur_strip) ui_inverse_off(mywin);
			mvwprintw(mywin -> win, 4 + loop, 60, "%d", (cur -> pstrip)[loop].match_count);
		}
		mydoupdate();

		/* wait for key */
		for(;;)
		{
			key = wait_for_keypress(HELP_REGEXP_MENU, 2, mywin, 1);
			c = toupper(key);

			/* convert return to 'E'dit */
			if (key == 13)
				key = c = 'E';

			/* any valid keys? */
			if (c == 'Q' || c == abort_key || c == 'X' || c == 'A' || c == 'E' || c == 'D' || key == KEY_DOWN || key == 13 || key == KEY_UP)
				break;

			if (key == -1) /* T/O waiting for key? then update counters */
			{
				for(loop=0; loop<cur -> n_strip; loop++)
					mvwprintw(mywin -> win, 4 + loop, 60, "%d", (cur -> pstrip)[loop].match_count);

				mydoupdate();
			}
			else
				wrong_key();
		}
		/* exit this screen? */
		if (c == 'Q' || c == abort_key || c == 'X')
			break;

		if (key == KEY_UP)
		{
			if (cur_strip > 0)
				cur_strip--;
			else
				wrong_key();
		}
		else if (key == KEY_DOWN || key == 13)
		{
			if (cur_strip < (cur -> n_strip -1))
				cur_strip++;
			else
				wrong_key();
		}

		/* add or edit */
		if (c == 'A')
		{
			int rc;
			char *regex_str = NULL;
			int offset_start = -1, offset_end = -1;
			char *del = NULL;
			int col_nr = -1;
			int c;
			int cur_changed = 0;

			/* max. 10 regular expressions */
			if (cur -> n_strip == 10)
			{
				wrong_key();
				continue;
			}

			/* ask type */
			escape_print(mywin, 15, 2, "reg.^e^xp., ^r^ange, ^c^olumn, ^S^ubstrings");
			mydoupdate();
			for(;;)
			{
				c = toupper(wait_for_keypress(HELP_STRIPPER_TYPE, 0, mywin, 0));

				if (c == 'E' || c == 'R' || c == 'C' || c == 'S' ||
						c == abort_key || c == 'Q' || c == 'X')
					break;

				wrong_key();
			}
			if (c == abort_key || c == 'Q' || c == 'X') continue;

			if (c == 'E' || c == 'S')
			{
				/* user did not abort edit? */
				if (edit_stripper_edit_regexp(mywin, &regex_str, &case_insensitive))
					cur_changed = 1;
			}
			else if (c == 'R')
			{
				if (edit_stripper_edit_offsets(mywin, &offset_start, &offset_end))
					cur_changed = 1;
			}
			else if (c == 'C')
			{
				if (edit_stripper_edit_field(mywin, &del, &col_nr))
					cur_changed = 1;
			}

			if (!cur_changed) continue;
			changed = 1;

			cur_strip = cur -> n_strip++;
			cur -> pstrip = (strip_t *)myrealloc(cur -> pstrip, cur -> n_strip * sizeof(strip_t));
			memset(&(cur -> pstrip)[cur_strip], 0x00, sizeof(strip_t));

			/* compile */
			if (c == 'E' || c == 'S')
			{
				if ((rc = regcomp(&(cur -> pstrip)[cur_strip].regex, regex_str, REG_EXTENDED)))
				{
					regexp_error_popup(rc, &(cur -> pre)[cur_strip].regex);

					cur -> n_strip--;
					if (cur -> n_strip == cur_strip) cur_strip--;

					myfree(regex_str);
				}
				else
				{
					/* compilation went well, remember everything */
					(cur -> pstrip)[cur_strip].regex_str = regex_str;
					(cur -> pstrip)[cur_strip].type = (c == 'E' ? STRIP_TYPE_REGEXP : STRIP_KEEP_SUBSTR);
				}
			}
			else if (c == 'R')
			{
				(cur -> pstrip)[cur_strip].type  = STRIP_TYPE_RANGE;
				(cur -> pstrip)[cur_strip].start = offset_start;
				(cur -> pstrip)[cur_strip].end   = offset_end;
			}
			else if (c == 'C')
			{
				(cur -> pstrip)[cur_strip].type   = STRIP_TYPE_COLUMN;
				(cur -> pstrip)[cur_strip].col_nr = col_nr;
				(cur -> pstrip)[cur_strip].del    = del;
			}

			/* reset counter (as it is a different regexp which has not matched anything at all) */
			(cur -> pstrip)[cur_strip].match_count = 0;
		}
		/* delete entry */
		else if (c == 'D')
		{
			if (cur -> n_strip > 0)
			{
				changed = 1;

				/* delete entry */
				myfree((cur -> pstrip)[cur_strip].regex_str);

				/* update administration */
				if (cur -> n_strip == 1)
				{
					myfree(cur -> pstrip);
					cur -> pstrip = NULL;
				}
				else
				{
					int n_to_move = (cur -> n_strip - cur_strip) - 1;

					if (n_to_move > 0)
						memmove(&(cur -> pstrip)[cur_strip], &(cur -> pstrip)[cur_strip+1], sizeof(strip_t) * n_to_move);
				}
				cur -> n_strip--;

				/* move cursor */
				if (cur_strip > 0 && cur_strip == cur -> n_strip)
				{
					cur_strip--;
				}
			}
			else
				wrong_key();
		}
		else if (c == 'E')
		{
			if (cur -> n_strip > 0)
			{
				if ((cur -> pstrip)[cur_strip].type == STRIP_TYPE_REGEXP ||
				    (cur -> pstrip)[cur_strip].type == STRIP_KEEP_SUBSTR)
				{
					if (edit_stripper_edit_regexp(mywin, &(cur -> pstrip)[cur_strip].regex_str, &case_insensitive))
						changed = 1;
				}
				else if ((cur -> pstrip)[cur_strip].type == STRIP_TYPE_RANGE)
				{
					if (edit_stripper_edit_offsets(mywin, &(cur -> pstrip)[cur_strip].start, &(cur -> pstrip)[cur_strip].end))
						changed = 1;
				}
				else if ((cur -> pstrip)[cur_strip].type == STRIP_TYPE_COLUMN)
				{
					if (edit_stripper_edit_field(mywin, &(cur -> pstrip)[cur_strip].del, &(cur -> pstrip)[cur_strip].col_nr))
						changed = 1;
				}
			}
			else
				wrong_key();
		}
	}

	delete_popup(mywin);

	return changed;
}

void zero_str(char *what, int start, int end)
{
	memset(&what[start], 0x00, end-start);
}

char do_strip_re(regex_t *pre, int *match_count, char *in, char *strip_what)
{
	char changed = 0;
	int search_offset = 0;
	regmatch_t matches[MAX_N_RE_MATCHES];

	do
	{
		int new_offset = -1;

		if (regexec(pre, &in[search_offset], MAX_N_RE_MATCHES, matches, 0) != REG_NOMATCH)
		{
			int match_i;

			for(match_i=0; match_i<MAX_N_RE_MATCHES; match_i++)
			{
				if (matches[match_i].rm_so == -1 ||
						matches[match_i].rm_eo == -1)
					break;

				zero_str(strip_what, matches[match_i].rm_so + search_offset, matches[match_i].rm_eo + search_offset);
				changed = 1;

				new_offset = matches[match_i].rm_eo + search_offset;

				(*match_count)++;
			}
		}

		search_offset = new_offset;
	}
	while (search_offset != -1);

	return changed;
}

char do_keep_re(regex_t *pre, int *match_count, char *in, char *strip_what)
{
	char changed = 0;
	regmatch_t matches[MAX_N_RE_MATCHES];
	int last_offset = 0;

	if (regexec(pre, in, MAX_N_RE_MATCHES, matches, 0) != REG_NOMATCH)
	{
		int match_i;

		for(match_i=1; match_i<MAX_N_RE_MATCHES; match_i++)
		{
			if (matches[match_i].rm_so == -1 || matches[match_i].rm_eo == -1)
				break;

			zero_str(strip_what, last_offset, matches[match_i].rm_so);
			last_offset = matches[match_i].rm_eo;
		}

		if (last_offset != 0)
		{
			zero_str(strip_what, last_offset, strlen(in));
		}

		changed = 1;
	}

	return changed;
}

char do_strip_column(char *delimiter, int col_nr, char *in, char *strip_what)
{
	char changed = 0;
	char *p = in;
	int del_len = strlen(delimiter);
	int col_i;

	while(strncmp(p, delimiter, del_len) == 0) p += del_len;
	if (!*p) return 0;
	for(col_i=0; col_i<col_nr; col_i++)
	{
		p = strstr(p, delimiter);
		if (!p) break;
		while(strncmp(p, delimiter, del_len) == 0) p += del_len;
	}
	if (!p) return 0;

	changed = 1;

	/* strip the column */
	while(strncmp(p, delimiter, del_len) != 0 && *p != 0x00)
	{
		strip_what[(int)(p - in)] = 0;
		p++;
	}
	/* and strip the delimiter(s) behind it */
	while(strncmp(p, delimiter, del_len) == 0)
	{
		int loop2;
		for(loop2=0; loop2<del_len; loop2++)
			strip_what[(int)(p - in) + loop2] = 0;
		p += del_len;
	}

	return changed;
}

char * do_strip(proginfo *cur, char *in)
{
	int loop, len;
	int offset = 0;
	char *strip_what;
	char *new_string;
	char changed = 0;

	if (!in || cur -> n_strip == 0)
		return NULL;

	len = strlen(in);
	if (len == 0)
		return NULL;

	strip_what = (char *)mymalloc(len);
	new_string = (char *)mymalloc(len + 1);

	memset(strip_what, 0x01, len);

	for(loop=0; loop<cur -> n_strip; loop++)
	{
		if ((cur -> pstrip)[loop].type == STRIP_TYPE_RANGE)
		{
			memset(&strip_what[(cur -> pstrip)[loop].start], 0x00, (cur -> pstrip)[loop].end - (cur -> pstrip)[loop].start);
			changed = 1;
		}
		else if ((cur -> pstrip)[loop].type == STRIP_TYPE_REGEXP)
		{
			changed |= do_strip_re(&(cur -> pstrip)[loop].regex, &(cur -> pstrip)[loop].match_count, in, strip_what);
		}
		else if ((cur -> pstrip)[loop].type == STRIP_TYPE_COLUMN)
		{
			changed |= do_strip_column((cur -> pstrip)[loop].del, (cur -> pstrip)[loop].col_nr, in, strip_what);
		}
		else if ((cur -> pstrip)[loop].type == STRIP_KEEP_SUBSTR)
		{
			changed |= do_keep_re(&(cur -> pstrip)[loop].regex, &(cur -> pstrip)[loop].match_count, in, strip_what);
		}
	}

	if (changed)
	{
		for(loop=0; loop<len; loop++)
		{
			if (strip_what[loop])
				new_string[offset++] = in[loop];
		}
		new_string[offset] = 0x00;

		myfree(strip_what);

		return new_string;
	}

	myfree(strip_what);
	myfree(new_string);

	return NULL;
}
