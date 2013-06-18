#define _LARGEFILE64_SOURCE     /* required for GLIBC to enable stat64 and friends */
#include <sys/types.h>
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "term.h"
#include "mem.h"
#include "utils.h"
#include "globals.h"


/** create_subwindow_list
 * - in:      int f_index     window number
 *            char ***swlist  pointer to an array of strings
 * - returns: int             number of elements in the array of strings
 * this function creates for a given window (f_index) a list of subwindows:
 * subwindows are created when you're merging the output of several files/
 * commands
 */
int create_subwindow_list(int f_index, char ***swlist)
{
	char **list = NULL;
	int n = 0;
	proginfo *cur = &pi[f_index];
	int subwin = 0;

	do
	{
		list = (char **)myrealloc(list, (n + 1) * sizeof(char *));

		if (show_subwindow_id)
		{
			int len = strlen(cur -> filename);
			list[n] = (char *)mymalloc(len + 1);
			memcpy(list[n], cur -> filename, len + 1);
		}
		else
			list[n] = mystrdup(cur -> filename);

		n++;

		cur = cur -> next;
		subwin++;
	}
	while(cur);

	*swlist = list;

	return n;
}

char generate_string(char *whereto, void **list, selbox_type_t type, int wcols, int index)
{
	char invert = 0;

	if (type == SEL_WIN)
	{
		snprintf(whereto, wcols + 1, "%02d %s", index, shorten_filename(((proginfo *)list)[index].filename, wcols));
		invert = ((proginfo *)list)[index].hidden | ((proginfo *)list)[index].paused;
	}
	else if (type == SEL_SUBWIN)
		snprintf(whereto, wcols + 1, "%02d %s", index, shorten_filename(((char **)list)[index], wcols));
	else if (type == SEL_FILES)
		strncpy(whereto, ((char **)list)[index], min(strlen(((char **)list)[index]), wcols) + 1);
	else if (type == SEL_CSCHEME)
		strncpy(whereto, ((color_scheme *)list)[index].name, min(strlen(((color_scheme *)list)[index].name), wcols) + 1);
	else if (type == SEL_HISTORY)
		strncpy(whereto, ((char **)list)[index], min(strlen(((char **)list)[index]), wcols) + 1);

	whereto[min(strlen(whereto), wcols - 1)] = 0x00;

	return invert;
}

int find_sb_string(void **list, selbox_type_t type, int nentries, char *compare_string)
{
	int index, len = strlen(compare_string);

	for(index=0; index<nentries; index++)
	{
		char buffer[4096];

		(void)generate_string(buffer, list, type, sizeof(buffer) - 1, index);

		if (strncmp(buffer, compare_string, len) == 0)
			return index;
	}

	return -1;
}

int selection_box(void **list, char *needs_mark, int nlines, selbox_type_t type, int what_help, char *heading)
{
	NEWWIN *mywin;
	int wlines = min(nlines, (max_y - 1) - 4);
	int total_win_size = wlines + 4;
	int win_width = max(32, max_x / 3);
	int wcols = win_width - 4;
	int pos = 0, ppos = -1, offs = 0, poffs = -1;
	int loop = 0, sel = -1;
	char first = 1;
	char *dummy = (char *)mymalloc(wcols + 1);
	int path_max = find_path_max();
	char *selstr = (char *)mymalloc(path_max + 1), selfound = 0;

	selstr[0] = 0x00;

	mywin = create_popup(total_win_size, win_width);

	for(;;)
	{
		int c;

		/* draw list */
		if (pos != ppos)
		{
			int entries_left = (nlines - pos);

			werase(mywin -> win);

			if (heading)
				win_header(mywin, heading);
			else if (type == SEL_WIN)
				win_header(mywin, "Select window");
			else if (type == SEL_SUBWIN)
				win_header(mywin, "Select subwindow");
			else if (type == SEL_FILES)
				win_header(mywin, "Select file");
			else if (type == SEL_CSCHEME)
				win_header(mywin, "Select color scheme");
			else if (type == SEL_HISTORY)
				win_header(mywin, "Select string from history");

			for(loop=0; loop<min(entries_left, wlines); loop++)
			{
				char invert = generate_string(dummy, list, type, wcols, loop + pos);
				if (loop == offs)
					ui_inverse_on(mywin);
				if (invert) color_on(mywin, find_colorpair(COLOR_YELLOW, -1, 0));
				if (needs_mark && needs_mark[loop + pos])
					mvwprintw(mywin -> win, loop + 2, 1, "*");
				mvwprintw(mywin -> win, loop + 2, 2, "%s", dummy);
				if (invert) color_off(mywin, find_colorpair(COLOR_YELLOW, -1, 0));
				if (loop == offs)
					ui_inverse_off(mywin);
			}

			draw_border(mywin);

			ppos = pos;
			poffs = offs;
		}
		else if (poffs != offs)
		{
			int yellow_cp = find_colorpair(COLOR_YELLOW, -1, 0);
			char invert = generate_string(dummy, list, type, wcols, poffs + pos);
			if (invert) color_on(mywin, yellow_cp);
			mvwprintw(mywin -> win, poffs + 2, 2, "%s", dummy);
			if (invert) color_off(mywin, yellow_cp);

			invert = generate_string(dummy, list, type, wcols, offs + pos);

			ui_inverse_on(mywin);
			if (invert) color_on(mywin, yellow_cp);
			if (needs_mark && needs_mark[offs + pos])
				mvwprintw(mywin -> win, loop + 2, 1, "*");
			mvwprintw(mywin -> win, offs + 2, 2, "%s", dummy);
			if (invert) color_off(mywin, yellow_cp);
			ui_inverse_off(mywin);

			poffs = offs;
		}

		if (first)
		{
			first = 0;
			color_on(mywin, find_colorpair(COLOR_GREEN, -1, 0));
			mvwprintw(mywin -> win, total_win_size - 2, 2, "Press ^G to abort");
			color_off(mywin, find_colorpair(COLOR_GREEN, -1, 0));
		}
		else
		{
			int loop, len = strlen(selstr);

			for(loop=0; loop<wcols; loop++)
				mvwprintw(mywin -> win, total_win_size - 2, 1 + loop, " ");

			if (!selfound) color_on(mywin, find_colorpair(COLOR_RED, -1, 0));
			mvwprintw(mywin -> win, total_win_size - 2, 1, "%s", &selstr[max(0, len - wcols)]);
			if (!selfound) color_off(mywin, find_colorpair(COLOR_RED, -1, 0));
		}

		mydoupdate();

		c = wait_for_keypress(what_help, 0, mywin, 1);

		if (c == KEY_UP)
		{
			if ((offs + pos) > 0)
			{
				if (offs)
					offs--;
				else
					pos--;
			}
			else
			{
				wrong_key();
			}
		}
		else if (c == KEY_DOWN)
		{
			if ((pos + offs) < (nlines-1))
			{
				if (offs < (wlines-1))
					offs++;
				else
					pos++;
			}
			else
			{
				wrong_key();
			}
		}
		else if (c == KEY_NPAGE)
		{
			if ((pos + offs) < (nlines - 1))
			{
				pos += min(wlines, (nlines - 1) - (pos + offs));
			}
			else
			{
				wrong_key();
			}
		}
		else if (c == KEY_PPAGE)
		{
			if ((pos + offs - wlines) >= 0)
			{
				if (pos > wlines)
				{
					pos -= wlines;
				}
				else
				{
					pos -= (wlines - offs);
					offs = 0;
				}
			}
			else if (offs > 0)
			{
				offs = 0;
			}
			else if (pos > 0)
			{
				pos = 0;
			}
			else
			{
				wrong_key();
			}
		}
		else if (c == KEY_ENTER || c == 13 || c == 10)
		{
			sel = pos + offs;
			break;
		}
		else if (c == abort_key || c == -1)
		{
			break;
		}
		else if ((c > 31 && c != 127) || (c == KEY_BACKSPACE))
		{
			int index, curlen;

			curlen = strlen(selstr);
			if (c == KEY_BACKSPACE)
			{
				if (curlen > 0)
					selstr[curlen - 1] = 0x00;
				else
					wrong_key();
			}
			else if (curlen < path_max)
			{
				selstr[curlen] = c;
				selstr[curlen + 1] = 0x00;
			}
			else
				wrong_key();


			curlen = strlen(selstr);
			if (curlen > 0)
			{
				index = find_sb_string(list, type, nlines, selstr);
				if (index != -1)
				{
					ppos = -1;
					sel = pos = index;
					selfound = 1;
				}
				else
				{
					selfound = 0;
				}
			}
		}
		else
		{
			wrong_key();
		}
	}

	delete_popup(mywin);

	myfree(dummy);
	myfree(selstr);

	return sel;
}

int select_window(int what_help, char *heading)
{
	return selection_box((void **)pi, NULL, nfd, SEL_WIN, what_help, heading);
}

proginfo * select_subwindow(int f_index, int what_help, char *heading)
{
	proginfo *cur = NULL;
	char **list;
	int list_n, index, loop;

	if (f_index == -1)
		return NULL;

	list_n = create_subwindow_list(f_index, &list);

	index = selection_box((void **)list, NULL, list_n, SEL_SUBWIN, what_help, heading);

	if (index != -1)
	{
		cur = &pi[f_index];
		for(loop=0; loop<index; loop++)
		{
			cur = cur -> next;
		}
	}

	delete_array(list, list_n);

	return cur;
}

char * select_file(char *input, int what_help)
{
	char **list = NULL, *isdir = NULL, *path = NULL;
	char *new_fname = NULL;
	struct stat64 statbuf;
	int list_n, index;
	int strbufsize = find_path_max();

	list_n = match_files(input, &path, &list, &isdir);
	if (list_n == 0)
	{
		myfree(path);
		flash();
		return NULL;
	}

	index = selection_box((void **)list, isdir, list_n, SEL_FILES, what_help, NULL);
	if (index != -1)
	{
		new_fname = (char *)mymalloc(strbufsize + 1);

		snprintf(new_fname, strbufsize, "%s%s", path, list[index]);

		if (stat64(new_fname, &statbuf) == -1)
		{
			myfree(new_fname);
			new_fname = NULL;
			flash();
		}
		else
		{
			if (S_ISDIR(statbuf.st_mode))
			{
				strncat(new_fname, "/", strbufsize);
			}
		}
	}

	delete_array(list, list_n);

	myfree(isdir);
	myfree(path);

	return new_fname;
}
