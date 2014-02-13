#define _LARGEFILE64_SOURCE	/* required for GLIBC to enable stat64 and friends */
#include <ctype.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <regex.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pwd.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#if defined(sun) || defined(__sun)
#include <stropts.h>
#endif
#ifndef AIX
#include <sys/termios.h> /* needed on Solaris 8 */
#endif
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "error.h"
#include "mem.h"
#include "term.h"
#include "color.h"
#include "utils.h"
#include "help.h"
#include "globals.h"
#include "history.h"
#include "ui.h"


void wrong_key(void)
{
	if (beep_method == BEEP_FLASH)
		flash();
	else if (beep_method == BEEP_BEEP)
		beep();
	else if (beep_method == BEEP_POPUP)
	{
		NEWWIN *beep_win = create_popup(5, 9);

		color_on(beep_win, find_colorpair(COLOR_GREEN, -1, 0));
		mvwprintw(beep_win -> win, 3, 2, "Beep!");
		color_off(beep_win, find_colorpair(COLOR_GREEN, -1, 0));

		(void)wait_for_keypress(-1, beep_popup_length, beep_win, 0);

		delete_popup(beep_win);
	}
	else if (beep_method == BEEP_NONE)
	{
		/* do nothing */
	}
	flushinp();
}

void draw_border(NEWWIN *mywin)
{
	wborder(mywin -> win, box_left_side, box_right_side, box_top_side, box_bottom_side,
			      box_top_left_hand_corner, box_top_right_hand_corner, box_bottom_left_hand_corner, box_bottom_right_hand_corner);
}

int ask_yes_no(int what_help, NEWWIN *popup)
{
	for(;;)
	{
		int c = toupper(wait_for_keypress(what_help, 0, popup, 0));

		if (c == abort_key) return -1;

		switch(c)
		{
		case 'Y':
		case 'J':
			return 1;

		case 'N':
			return 0;

		case 'Q':
			return -1;
		}

		wrong_key();
	}
}

void color_on(NEWWIN *win, int colorpair_index)
{
	if (use_colors && colorpair_index != -1)
		wattron(win -> win, COLOR_PAIR(colorpair_index));
}

void color_off(NEWWIN *win, int colorpair_index)
{
	if (use_colors && colorpair_index != -1)
		wattroff(win -> win, COLOR_PAIR(colorpair_index));
}

void myattr_on(NEWWIN *win, myattr_t attrs)
{
	color_on(win, attrs.colorpair_index);

	if (attrs.attrs != -1)
		wattron(win -> win, attrs.attrs);
}

void myattr_off(NEWWIN *win, myattr_t attrs)
{
	color_off(win, attrs.colorpair_index);

	if (attrs.attrs != -1)
		wattroff(win -> win, attrs.attrs);
}

void ui_inverse_on(NEWWIN *win)
{
	wattron(win -> win, A_REVERSE);
}

void ui_inverse_off(NEWWIN *win)
{
	wattroff(win -> win, A_REVERSE);
}

void draw_line(NEWWIN *win, linepos_t where)
{
	int mx = getmaxx(win -> win), my = getmaxy(win -> win);

	if (where == LINE_LEFT)
		mvwvline(win -> win, 0, 0, ' ', my);
	else if (where == LINE_RIGHT)
		mvwvline(win -> win, 0, mx-1, ' ', my);
	else if (where == LINE_TOP)
		mvwhline(win -> win, 0, 0, ' ', mx);
	else if (where == LINE_BOTTOM)
		mvwhline(win -> win, my-1, 0, ' ', mx);
}

char * edit_string(NEWWIN *win, int win_y, int win_x, int win_width, int max_width, char numbers_only, char *input_string, int what_help, char first_char, history_t *ph, mybool_t *pcase_insensitive)
{
	char *string = (char *)mymalloc(max_width + 1);
	int str_pos = 0, x = 0;
	int line_width = win_width;

	if (pcase_insensitive)
		mvwprintw(win -> win, win_y + 1, win_x - 1, "[%c] case insensitive (press TAB)", *pcase_insensitive?'X':' ');

	if (input_string)
	{
		int input_string_len = strlen(input_string), copy_len = min(min(win_width, max_width), input_string_len);
		int dummy = max(0, str_pos - line_width);

		memcpy(string, input_string, copy_len);
		string[copy_len] = 0x00;

		str_pos = dummy;
		mvwprintw(win -> win, win_y, win_x, &string[dummy]);
		x = strlen(string) - dummy;
	}
	else
	{
		string[0] = 0x00;
	}
	wmove(win -> win, win_y, win_x + x);

	mydoupdate();

	for(;;)
	{
		char force_redraw = 0;
		int prev_str_pos = str_pos;
		int c;

		if (first_char != (char)-1)
		{
			c = first_char;
			first_char = -1;
		}
		else
		{
			c = wait_for_keypress(what_help, 0, NULL, 1);
		}

		/* confirm */
		if (c == KEY_ENTER || c == 13 || c == 10 )
			break;

		/* abort */
		if (c == abort_key || c == 17 || c == 24) /* ^g / ^q / ^x */
		{
			string[0] = 0x00;
			break;
		}

		switch(c)
		{
		case 1:			/* ^A */
			str_pos = x = 0;
			break;
		case 5:			/* ^E */
			{
				int dummy = strlen(string);

				if (dummy > line_width)
				{
					str_pos = dummy - (line_width / 2);
					x = (line_width / 2);
				}
				else
				{
					str_pos = 0;
					x = dummy;
				}
			}
			break;
		case 9:			/* tab (filename completion/switch to case insensitve field) */
			if (pcase_insensitive)
			{
				ask_case_insensitive(pcase_insensitive);
				force_redraw = 1;
			}
			else if (numbers_only)
			{
				wrong_key();
			}
			else
			{
				int dummy;
				char *file = select_file(string, -1);

				if (file)
				{
					strncpy(string, file, max_width);
					string[max_width] = 0x00;
					myfree(file);
				}

				dummy = strlen(string);
				if (dummy > line_width)
				{
					str_pos = dummy - (line_width / 2);
					x = (line_width / 2);
				}
				else
				{
					str_pos = 0;
					x = dummy;
				}

				force_redraw = 1;
			}
			break;
		case 21:		/* ^U */
			string[0] = 0x00;
			str_pos = x = 0;
			force_redraw = 1;
			break;
		case 23:		/* ^W delete word */
			{
				int spos = str_pos + x;
				int dpos = spos;

				/* remove spaces upto the first word */
				while(dpos > 0 && string[dpos] == ' ')
					dpos --;

				/* remove that word we found */
				while(dpos > 0 && string[dpos] != ' ')
					dpos--;

				memmove(&string[dpos], &string[spos], (max_width - spos) + 1);

				str_pos = max(0, dpos - (line_width / 2));
				x = dpos - str_pos;

				force_redraw = 1;
			}
			break;

		case 127:		/* DEL */
		case 4:			/* ^D */
			{
				int spos = str_pos + x;
				int n_after = strlen(&string[spos]);

				if (n_after > 0)
				{
					memmove(&string[spos], &string[spos + 1], n_after);
					force_redraw = 1;
				}
			}
			break;

		case KEY_DOWN:		/* cursor down */
		case 18:		/* ^R */
			if (ph == NULL || ph -> history_size <= 0 || ph -> history_file == NULL)
			{
				wrong_key();
			}
			else
			{
				int dummy;
				char *hs = search_history(ph, string);

				if (hs)
				{
					strncpy(string, hs, max_width);
					string[max_width] = 0x00;
					myfree(hs);
				}

				dummy = strlen(string);
				if (dummy > line_width)
				{
					str_pos = dummy - (line_width / 2);
					x = (line_width / 2);
				}
				else
				{
					str_pos = 0;
					x = dummy;
				}

				force_redraw = 1;
			}
			break;

		case KEY_BACKSPACE:
			{
				int spos = str_pos + x;

				if (spos > 0)
				{
					memmove(&string[spos - 1], &string[spos], (max_width - spos) + 1);
					if (x > 0)
					{
						x--;
					}
					else
					{
						str_pos--;
					}

					force_redraw = 1;
				}
			}
			break;
		case KEY_LEFT:
			if (x > 0)
			{
				x--;
			}
			else if (str_pos > 0)
			{
				str_pos--;
			}
			break;
		case KEY_RIGHT:
			if ((x + str_pos) < strlen(string))
			{
				if (x < line_width)
					x++;
				else
					str_pos++;
			}
			else
			{
				wrong_key();
			}
			break;
		default:
			{
				int len = strlen(string);

				/* only allow valid ASCII */
				if (c < 32)
				{
					wrong_key();
					break;
				}

				if (numbers_only && (c < '0' || c > '9'))
				{
					wrong_key();
					break;
				}

				if (len == max_width)
				{
					wrong_key();
					break;
				}

				/* cursor at end of string? */
				if (str_pos == len)
				{
					string[str_pos + x] = c;
					string[str_pos + x + 1] = 0x00;
					waddch(win -> win, c);
				}
				else /* add character to somewhere IN the string */
				{
					memmove(&string[str_pos + x + 1], &string[str_pos + x], strlen(&string[str_pos + x]) + 1);
					string[str_pos + x] = c;
					force_redraw = 1;
				}

				if ((x + str_pos) < max_width)
				{
					if (x < line_width)
						x++;
					else
						str_pos++;
				}
				else
				{
					wrong_key();
				}
			}
			break;
		}

		if (str_pos != prev_str_pos || force_redraw)
		{
			int loop;
			char *dummy = mystrdup(&string[str_pos]);
			dummy[min(strlen(dummy), line_width)] = 0x00;
			for(loop=strlen(dummy); loop<line_width; loop++)
				mvwprintw(win -> win, win_y, win_x + loop, " ");
			mvwprintw(win -> win, win_y, win_x, "%s", dummy);
			myfree(dummy);
			if (pcase_insensitive)
				mvwprintw(win -> win, win_y + 1, win_x, "%c", *pcase_insensitive?'X':' ');
			force_redraw = 0;
		}
		wmove(win -> win, win_y, win_x + x);
		mydoupdate();
	}

	if (string[0] == 0x00)
	{
		myfree(string);
		string = NULL;
	}
	else if (ph != NULL)
	{
		history_add(ph, string);
	}

	return string;
}

NEWWIN * create_popup(int n_lines, int n_colls)
{
	NEWWIN *newwin;
	int ocols  = (max_x/2) - (n_colls/2);
	int olines = (max_y/2) - (n_lines/2);

	/* create new window */
	newwin = mynewwin(n_lines, n_colls, olines, ocols);

	werase(newwin -> win);
	draw_border(newwin);

	show_panel(newwin -> pwin);

	return newwin;
}

void delete_popup(NEWWIN *mywin)
{
	if (mywin)
	{
		mydelwin(mywin);

		update_panels();
		doupdate();

		myfree(mywin);
	}
}

void mydelwin(NEWWIN *win)
{
	bottom_panel(win -> pwin);

	if (ERR == del_panel(win -> pwin))
		error_exit(FALSE, FALSE, "del_panel() failed\n");

	if (ERR == delwin(win -> win))
		error_exit(FALSE, FALSE, "delwin() failed\n");
}

void mydoupdate()
{
	update_panels();
	doupdate();
}

NEWWIN * mynewwin(int nlines, int ncols, int begin_y, int begin_x)
{
	NEWWIN *nwin = (NEWWIN *)mymalloc(sizeof(NEWWIN));
	/*	nwin -> win = subwin(stdscr, nlines, ncols, begin_y, begin_x); */
	nwin -> win = newwin(nlines, ncols, begin_y, begin_x);
	if (!nwin -> win)
		error_exit(FALSE, FALSE, "Failed to create window with dimensions %dx%d at offset %d,%d (terminal size: %d,%d)\n", ncols, nlines, begin_x, begin_y, COLS, LINES);

	nwin -> pwin = new_panel(nwin -> win);
	if (!nwin -> pwin)
		error_exit(FALSE, FALSE, "Failed to create panel.\n");

	nwin -> x_off = begin_x;
	nwin -> y_off = begin_y;
	nwin -> width = ncols;
	nwin -> height = nlines;

	if (bright_colors)
		wattron(nwin -> win, A_BOLD);

	if (default_bg_color != -1)
		wbkgdset(nwin -> win, COLOR_PAIR(find_or_init_colorpair(-1, default_bg_color, 1)));

	return nwin;
}

void escape_print(NEWWIN *win, int y, int x, char *str)
{
	int loop, index = 0, len = strlen(str);
	char inv = 0, ul = 0, bold = 0;

	for(loop=0; loop<len; loop++)
	{
		if (str[loop] == '^') /* ^^ is ^ */
		{
			if (str[loop + 1 ] == '^')
			{
				/* just print a _ */
				mvwprintw(win -> win, y, x + index++, "^");
				loop++;
			}
			else
			{
				if (!inv)
					ui_inverse_on(win);
				else
					ui_inverse_off(win);

				inv = 1 - inv;
			}
		}
		else if (str[loop] == '_')
		{
			if (str[loop + 1] == '_')	/* __ is _ */
			{
				/* just print a _ */
				mvwprintw(win -> win, y, x + index++, "_");
				loop++;
			}
			else
			{
				if (!ul)
					wattron(win -> win, A_UNDERLINE);
				else
					wattroff(win -> win, A_UNDERLINE);

				ul = 1 - ul;
			}
		}
		else if (str[loop] == '*')
		{
			if (str[loop + 1] == '*')
			{
				/* just print a * */
				mvwprintw(win -> win, y, x + index++, "*");
				loop++;
			}
			else
			{
				if (!bold)
					wattron(win -> win, A_BOLD);
				else
					wattroff(win -> win, A_BOLD);

				bold = 1 - bold;
			}
		}
		else
		{
			mvwprintw(win -> win, y, x + index++, "%c", str[loop]);
		}
	}

	if (inv) ui_inverse_off(win);
	if (ul)  wattroff(win -> win, A_UNDERLINE);
	if (bold)  wattroff(win -> win, A_BOLD);
}

void win_header(NEWWIN *win, char *str)
{
	wattron(win -> win, A_BOLD);
	mvwprintw(win -> win, 1, 2, str);
	wattroff(win -> win, A_BOLD);
}

void gui_window_header(char *string)
{
	if (term_type == TERM_XTERM)
	{
		fprintf(stderr, "\033]2;%s\007", string);

#if 0	/* this code gives problems */
		/* \033]0;%s\007 */
		putp("\033]0;");
		putp(string);
		putp("\007");
#endif
	}
}

int find_colorpair(int fgcolor, int bgcolor, char fuzzy)
{
	int loop;

	for(loop=0; loop<cp.n_def; loop++)
	{
		if (cp.fg_color[loop] == fgcolor && cp.bg_color[loop] == bgcolor)
		{
			return loop;
		}
	}

	if (fuzzy)
	{
		for(loop=cp.n_def-1; loop>=0; loop++)
		{
			if (cp.fg_color[loop] == fgcolor && cp.bg_color[loop] == -1)
				return loop;
			else if (cp.fg_color[loop] == -1 && cp.bg_color[loop] == bgcolor)
				return loop;
		}
	}

	return -1;
}

myattr_t find_attr(int fgcolor, int bgcolor, int attrs)
{
	myattr_t cdev;

	if (attrs == -1)
		attrs = A_NORMAL;
	cdev.attrs = attrs;
	cdev.colorpair_index = find_colorpair(fgcolor, bgcolor, 0);

	return cdev;
}

/* ignore errors when doing TERM-emulation */
int find_or_init_colorpair(int fgcolor, int bgcolor, char ignore_errors)
{
	int index;

	if (use_colors)
	{
		index = find_colorpair(fgcolor, bgcolor, 0);
		if (index != -1)
			return index;

		if (cp.n_def == cp.size && cp.size > 0)
		{
			if (ignore_errors)
			{
				index = find_colorpair(fgcolor, bgcolor, 1);
				if (index != -1) return index;

				return 0;
			}

			error_exit(FALSE, FALSE, "Too many (%d) colorpairs defined.\n", cp.n_def);
		}

		cp.fg_color[cp.n_def] = fgcolor;
		cp.bg_color[cp.n_def] = bgcolor;
		init_pair(cp.n_def, cp.fg_color[cp.n_def], cp.bg_color[cp.n_def]);
		cp.n_def++;
		return cp.n_def - 1;
	}

	return 0;
}

int colorstr_to_nr(char *str)
{
	int loop;

	if (str[0] == 0x00) return -1;

	for(loop=0; loop<n_colors_defined; loop++)
	{
		if (color_names[loop] && strcmp(color_names[loop], str) == 0)
			return loop;
	}

	if (use_colors)
		error_exit(FALSE, FALSE, "'%s' is not recognized as a color\n", str);

	return -1;
}

int attrstr_to_nr(char *str)
{
	int attr = 0;

	for(;;)
	{
		char *slash = strchr(str, '/');

		if (slash)
			*slash = 0x00;

		if (strcmp(str, "bold") == 0)
			attr |= A_BOLD;
		else if (strcmp(str, "reverse") == 0 || strcmp(str, "inverse") == 0)
			attr |= A_REVERSE;
		else if (strcmp(str, "underline") == 0)
			attr |= A_UNDERLINE;
		else if (strcmp(str, "normal") == 0)
			attr |= A_NORMAL;
		else if (strcmp(str, "dim") == 0)
			attr |= A_DIM;
		else if (strcmp(str, "blink") == 0)
			attr |= A_BLINK;
		else
			error_exit(FALSE, FALSE, "'%s' is not recognized as an attribute.\n", str);

		if (!slash)
			break;

		str = slash + 1;
	}

	return attr;
}

myattr_t parse_attributes(char *str)
{
	char *komma1 = strchr(str, ',');
	char *komma2 = NULL;
	int fgcolor = -1, bgcolor = -1, attr = 0;
	myattr_t ma;

	if (komma1)
	{
		komma2 = strchr(komma1 + 1, ',');
		*komma1 = 0x00;
	}
	if (komma2)
		*komma2 = 0x00;

	fgcolor = colorstr_to_nr(str);

	if (komma1)
		bgcolor = colorstr_to_nr(komma1 + 1);

	if (komma2)
		attr = attrstr_to_nr(komma2 + 1);

	ma.colorpair_index = find_or_init_colorpair(fgcolor, bgcolor, 0);
	ma.attrs = attr;

	return ma;
}

char *color_to_string(int nr)
{
	if (nr < 0 || nr >= COLORS)
		return "???";

	return color_names[nr];
}

void attr_to_str_helper(char *to, char *what)
{
	int len = strlen(to);

	if (len)
		sprintf(&to[len], "/%s", what);
	else
		sprintf(&to[len], "%s", what);
}

char *attr_to_str(int attr)
{
	char buffer[128] = { 0 };

	if (attr & A_BOLD)
		attr_to_str_helper(buffer, "bold");

	if (attr & A_BLINK)
		attr_to_str_helper(buffer, "blink");

	if (attr & A_REVERSE)
		attr_to_str_helper(buffer, "inverse");

	if (attr & A_UNDERLINE)
		attr_to_str_helper(buffer, "underline");

	if (attr & A_DIM)
		attr_to_str_helper(buffer, "dim");

	return mystrdup(buffer);
}

void determine_terminal_size(int *max_y, int *max_x)
{
	struct winsize size;

	*max_x = *max_y = 0;

	/* changed from 'STDIN_FILENO' as that is incorrect: we're
	 * outputting to stdout!
	 */
	if (ioctl(1, TIOCGWINSZ, &size) == 0)
	{
		*max_y = size.ws_row;
		*max_x = size.ws_col;
	}

	if (!*max_x || !*max_y)
	{
		char *dummy = getenv("COLUMNS");
		if (dummy)
			*max_x = atoi(dummy);
		else
			*max_x = 80;

		dummy = getenv("LINES");
		if (dummy)
			*max_x = atoi(dummy);
		else
			*max_x = 24;
	}
}

int ansi_code_to_color(int value)
{
	switch(value)
	{
		case 30:
			return COLOR_BLACK;

		case 31:
			return COLOR_RED;

		case 32:
			return COLOR_GREEN;

		case 33:
			return COLOR_YELLOW;

		case 34:
			return COLOR_BLUE;

		case 35:
			return COLOR_MAGENTA;

		case 36:
			return COLOR_CYAN;

		case 37:
			return COLOR_WHITE;
	}

	return -1;
}

char * emulate_terminal(char *string, color_offset_in_line **cmatches, int *n_cmatches)
{
	int len = strlen(string), new_offset = 0, loop;
	char *new_string = (char *)mymalloc(len + 1);
	int cur_n_cmatches = 0;

	*n_cmatches = 0;
	*cmatches = NULL;

	/* FIXME: this is ANSI/vt100-only */
	for(loop=0; loop<len; loop++)
	{
		if (string[loop] == 27)
		{
			myattr_t ci = { -1, -1 };

			if (string[loop + 1] == '[')
			{
				int find_cmd;

				for(find_cmd=loop + 2; find_cmd < len; find_cmd++)
				{
					char cur_char = string[find_cmd];
					if (cur_char >= 'a' && cur_char <= 'z')
						break;
				}

				if (string[find_cmd] == 'm')
				{
					int fg_i = -1, bg_i = -1, attr = 0;
					char *p = &string[loop + 2];

					string[find_cmd] = 0x00;

					while(p)
					{
						int dummy;
						char *newp = strchr(p, ';');
						if (newp) *newp = 0x00;

						dummy = atoi(p);

						if (dummy >= 40 && dummy <= 47)
							bg_i = ansi_code_to_color(dummy - 10);
						else if (dummy >= 30 && dummy <= 37)
							fg_i = ansi_code_to_color(dummy);
						else if (dummy == 1)
							attr = A_BOLD;
						else if (dummy == 4)
							attr = A_UNDERLINE;
						else if (dummy == 5)
							attr = A_BLINK;
						else if (dummy == 7)
							attr = A_REVERSE;

						p = newp;
						if (p) p++;
					}

					ci.colorpair_index = find_or_init_colorpair(fg_i, bg_i, 1);
					ci.attrs = attr;

					string[find_cmd] = 'm';
				}

				loop = find_cmd;
			}

			if (ci.colorpair_index != -1)
			{
				if (cur_n_cmatches == *n_cmatches)
				{
					cur_n_cmatches = (cur_n_cmatches + 8) * 2;

					*cmatches = realloc_color_offset_in_line(*cmatches, cur_n_cmatches);
				}

				memset(&(*cmatches)[*n_cmatches], 0x00, sizeof(color_offset_in_line));
				(*cmatches)[*n_cmatches].start = new_offset;
				(*cmatches)[*n_cmatches].end = -1;
				(*cmatches)[*n_cmatches].attrs = ci;
				(*n_cmatches)++;
			}
		}
		else
		{
			new_string[new_offset++] = string[loop];
		}
	}
	for(loop=0; loop<(*n_cmatches - 1); loop++)
		(*cmatches)[loop].end = (*cmatches)[loop + 1].start;
	if (*n_cmatches >= 1)
		(*cmatches)[*n_cmatches - 1].end = new_offset;

	new_string[new_offset] = 0x00;

	return new_string;
}

void get_terminal_type(void)
{
	char *dummy = getenv("TERM");

	if (dummy && strstr(dummy, "xterm") != NULL)
	{
		term_type = TERM_XTERM;
	}
}

void init_ncurses(void)
{
	initscr();
	if (use_colors)
		start_color(); /* don't care if this one failes */
	keypad(stdscr, TRUE);
	cbreak();
	intrflush(stdscr, FALSE);
	noecho();
	nonl();
	refresh();
	nodelay(stdscr, TRUE);
	meta(stdscr, TRUE);	/* enable 8-bit input */
	idlok(stdscr, TRUE);	/* may give a little clunky screenredraw */
	idcok(stdscr, TRUE);	/* may give a little clunky screenredraw */
	leaveok(stdscr, FALSE);

	max_y = LINES;
	max_x = COLS;
}

void init_colornames(void)
{
	int loop, dummy;

	dummy = min(256, COLORS);
	if (use_colors)
	{
		color_names = (char **)mymalloc(dummy * sizeof(char *));
		memset(color_names, 0x00, dummy * sizeof(char *));

		color_names[COLOR_RED]    = "red";
		color_names[COLOR_GREEN]  = "green";
		color_names[COLOR_YELLOW] = "yellow";
		color_names[COLOR_BLUE]   = "blue";
		color_names[COLOR_MAGENTA]= "magenta";
		color_names[COLOR_CYAN]   = "cyan";
		color_names[COLOR_WHITE]  = "white";
		color_names[COLOR_BLACK]  = "black";
	}

	/* FIXME: is this needed? or are COLOR_* always at position 0...7? */
	for(loop=dummy - 1; loop>=0; loop--)
	{
		if (color_names[loop])
		{
			n_colors_defined = loop + 1;
			break;
		}
	}
}
