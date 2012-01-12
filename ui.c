#define _LARGEFILE64_SOURCE     /* required for GLIBC to enable stat64 and friends */
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "help.h"
#include "error.h"
#include "term.h"
#include "mem.h"
#include "utils.h"
#include "selbox.h"
#include "color.h"
#include "exec.h"
#include "globals.h"
#include "scrollback.h"
#include "cv.h"

void error_popup(char *title, int help, char *message, ...)
{
	va_list ap;
	NEWWIN *mywin;
	char buffer[4096];
	int len;
	myattr_t cdev;

	cdev.colorpair_index = find_colorpair(COLOR_RED, -1, 1);
	cdev.attrs = A_BLINK;

	va_start(ap, message);
	vsnprintf(buffer, sizeof(buffer), message, ap);
	va_end(ap);

	len = strlen(buffer);

	mywin = create_popup(9, max(33, find_char_offset(buffer, '\n')) + 4);

	win_header(mywin, title);
	myattr_on(mywin, cdev);
	mvwprintw(mywin -> win, 3, 2, "%s", buffer);
	myattr_off(mywin, cdev);
	escape_print(mywin, 7, 2, "_Press any key to exit this screen_");
	mydoupdate();

	wrong_key();

	wait_for_keypress(help, 0, mywin, 0);

	delete_popup(mywin);
}

void edit_color(int index)
{
	int example_color = COLOR_RED;
	NEWWIN *mywin = create_popup(15, 40);
	int colorbar = find_or_init_colorpair(example_color, example_color, 1);
	short old_r, old_g, old_b;
	double scale = 255.0 / 1000.0;
	int cursor_y = 0;
	short cur_r, cur_g, cur_b;

	color_content(example_color, &old_r, &old_g, &old_b);
	color_content(index, &cur_r, &cur_g, &cur_b);
	init_color(example_color, cur_r, cur_g, cur_b);

	for(;;)
	{
		int c;

		color_content(example_color, &cur_r, &cur_g, &cur_b);

		werase(mywin -> win);
		win_header(mywin, "Edit color");

		mvwprintw(mywin -> win, 3, 2, "Color to edit:");
		wattron(mywin -> win, COLOR_PAIR(colorbar));
		mvwprintw(mywin -> win, 3, 17, "     ");
		wattroff(mywin -> win, COLOR_PAIR(colorbar));
		mvwprintw(mywin -> win, 3, 23, "%s", color_names[index]);

		mvwprintw(mywin -> win, 5, 17, "   Red   Green Blue");
		mvwprintw(mywin -> win, 6, 15, "%c %4d  %4d  %4d", cursor_y == 0?'>':' ', cur_r, cur_g, cur_b);
		mvwprintw(mywin -> win, 7, 15, "%c %3d   %3d   %3d", cursor_y == 1?'>':' ', (int)((double)cur_r * scale), (int)((double)cur_g * scale), (int)((double)cur_b * scale));
		mvwprintw(mywin -> win, 8, 15, "%c %2x    %2x    %2x ", cursor_y == 2?'>':' ', (int)((double)cur_r * scale), (int)((double)cur_g * scale), (int)((double)cur_b * scale));

		escape_print(mywin, 10, 2, "^r^   edit red");
		escape_print(mywin, 11, 2, "^g^   edit green");
		escape_print(mywin, 12, 2, "^b^   edit blue");
		escape_print(mywin, 13, 2, "^c^   change colorname");
		mvwprintw(mywin -> win, 14, 2, "Press ^g to exit this screen");
		draw_border(mywin);
		mydoupdate();

		c = wait_for_keypress(HELP_EDIT_COLOR, 0, mywin, 1);
		if (c == abort_key)
			break;
		else if (c == KEY_UP)
		{
			if (cursor_y)
				cursor_y--;
			else
				wrong_key();
		}
		else if (c == KEY_DOWN)
		{
			if (cursor_y < 2)
				cursor_y++;
			else
				wrong_key();
		}
		else if (c == 'c')
		{
			char *newname = edit_string(mywin, 13, 23, 10, 10, 0, USE_IF_SET(color_names[index], ""), HELP_EDIT_COLOR_CHANGE_NAME, 0, NULL, NULL);
			if (newname)
			{
				myfree(color_names[index]);
				color_names[index] = newname;
			}
		}
		else if (c == 'r' || c == 'g' || c == 'b')
		{
			int y = -1;
			char val_str[5] = { 0 };
			int val = -1;
			char *newval = NULL;

			if (c == 'r')
			{
				y = 0;
				val = cur_r;
			}
			else if (c == 'g')
			{
				y = 1;
				val = cur_g;
			}
			else if (c == 'b')
			{
				y = 2;
				val = cur_b;
			}

			mvwprintw(mywin -> win, 10 + y, 19, ">");
			if (cursor_y == 0)
				snprintf(val_str, sizeof(val_str), "%d", val);
			else if (cursor_y == 1)
				snprintf(val_str, sizeof(val_str), "%d", (int)((double)val * scale));
			else if (cursor_y == 2)
				snprintf(val_str, sizeof(val_str), "%2x", (int)((double)val * scale));

			newval = edit_string(mywin, 10 + y , 20, 5, 5, (cursor_y < 2)?1:0, val_str, HELP_EDIT_COLOR_EDIT, -1, NULL, NULL);

			if (newval)
			{
				if (cursor_y == 0)
					val = max(0, min(atoi(newval), 1000));
				else if (cursor_y == 1)
					val = max(0, (double)min(atoi(newval) / scale, 255));
				else if (cursor_y == 2)
					val = max(0, (double)min(255, strtol(newval, NULL, 16) / scale));

				if (c == 'r')
					cur_r = val;
				else if (c == 'b')
					cur_g = val;
				else if (c == 'g')
					cur_b = val;

				myfree(newval);
			}

			init_color(example_color, cur_r, cur_g, cur_b);
		}
	}

	init_color(example_color, old_r, old_g, old_b);

	delete_popup(mywin);
}

int color_management(myattr_t *org, myattr_t *new)
{
	NEWWIN *mywin = create_popup(21, 64);
	short cur_fg = -1, cur_bg = -1;
	int cur_attr = 0;
	int offset = 0, cursor_y = 0, cursor_x = 0;
	int changed = 0;
	char *attrs[] = { "bold", "blink", "reverse", "underline", "normal", "dim" };
	int  attr_x[] = { A_BOLD, A_BLINK, A_REVERSE, A_UNDERLINE, A_NORMAL, A_DIM };
	const int n_attrs = sizeof(attr_x) / sizeof(int);

	if (new != NULL && org != NULL)
	{
		if (org -> attrs != -1) cur_attr = org -> attrs;
		if (org -> colorpair_index != -1) pair_content(org -> colorpair_index, &cur_fg, &cur_bg);
	}

	for(;;)
	{
		int loop, c;

		werase(mywin -> win);
		if (new)
			win_header(mywin, "Select colors and attributes");
		else
			win_header(mywin, "Select color to edit");
		if (can_change_color())
			mvwprintw(mywin -> win, 20, 2, "Press ^g to abort, 'e' to edit and 'a' to add a color");
		else
			mvwprintw(mywin -> win, 20, 2, "Press ^g to abort");

		wattron(mywin -> win, A_UNDERLINE);
		mvwprintw(mywin -> win, 2,  2, "foreground");
		if (new)
		{
			mvwprintw(mywin -> win, 2, 22, "background");
			mvwprintw(mywin -> win, 2, 42, "attributes");
		}
		wattroff(mywin -> win, A_UNDERLINE);
		for(loop=0; loop<15; loop++)
		{
			int cur_offset = offset + loop;

			if (cur_offset >= COLORS) break;

			if (new)
			{
				mvwprintw(mywin -> win, 3 + loop, 2, "[%c] %s",  cur_fg == cur_offset?'X':' ', color_names[cur_offset]?color_names[cur_offset]:"<undefined>");
				mvwprintw(mywin -> win, 3 + loop, 22, "[%c] %s", cur_bg == cur_offset?'X':' ', color_names[cur_offset]?color_names[cur_offset]:"<undefined>");
			}
			else
				mvwprintw(mywin -> win, 3 + loop, 2, "%s",  USE_IF_SET(color_names[cur_offset], "<undefined>"));
		}
		if (new)
		{
			for(loop=0; loop<n_attrs; loop++)
			{
				int cur_offset = offset + loop;

				if (cur_offset >= n_attrs) break;

				mvwprintw(mywin -> win, 3 + loop, 42, "[%c] %s", cur_attr & attr_x[cur_offset]?'X':' ', attrs[loop]);
			}
		}

		draw_border(mywin);
		wmove(mywin -> win, 3 + cursor_y, 3 + cursor_x * 20);
		mydoupdate();

		c = wait_for_keypress(HELP_SELECT_COLOR_AND_ATTRIBUTES, 0, mywin, 1);
		if (c == abort_key || c == 'Q' || c == 'X')
		{
			break;
		}
		else if (c == 'a' && can_change_color())
		{
			/* find empty slot */
			for(loop=0; loop<COLORS; loop++)
			{
				if (!color_names[loop])
					break;
			}
			
			if (loop < COLORS)
			{
				char *name;
				mvwprintw(mywin -> win, 19, 2, "Enter color name: ");
				name = edit_string(mywin, 19, 20, 10, 10, 0, "", -1, -1, NULL, NULL);
				if (name)
				{
					color_names[loop] = name;
					edit_color(loop);
					/* no free! */
				}
			}
			else
				error_popup("Create new color", -1, "Too many defined colors (%d max)", COLORS);
		}
		else if (c == KEY_UP)
		{
			if (cursor_y)
				cursor_y--;
			else if (offset)
				offset--;
			else
				wrong_key();
		}
		else if (c == KEY_DOWN)
		{
			if (cursor_x == 2)
			{
				if ((offset + cursor_y) < (n_attrs - 1))
				{
					if (cursor_y < (n_attrs - offset - 1))
						cursor_y++;
					else
						offset++;
				}
				else
					wrong_key();
			}
			else
			{
				if ((offset + cursor_y) < (n_colors_defined - 1))
				{
					if (cursor_y < (n_colors_defined - offset - 1))
						cursor_y++;
					else
						offset++;
				}
				else
					wrong_key();
			}
		}
		else if (new != NULL && c == KEY_LEFT)
		{
			if (cursor_x)
				cursor_x--;
			else
				wrong_key();
		}
		else if (new != NULL && c == KEY_RIGHT)
		{
			if (cursor_x == 0)
				cursor_x++;
			else if ((offset + cursor_y) < n_attrs && cursor_x == 1)
				cursor_x++;
			else
				wrong_key();
		}
		else if (c == 13)
			break;
		else if (c == 'e' && (cursor_x == 0 || cursor_x == 1))
		{
			edit_color(offset + cursor_y);
		}
		else if (new != NULL && c == ' ')
		{
			int sel = offset + cursor_y;

			changed = 1;

			if (cursor_x == 0)
				cur_fg = sel;
			else if (cursor_x == 1)
				cur_bg = sel;
			else if (cursor_x == 2)
			{
				if (sel >= n_attrs) error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Internal error.\n");

				if (cur_attr & attr_x[sel])
					cur_attr -= attr_x[sel];
				else
					cur_attr |= attr_x[sel];
			}
			else
				error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "Internal error.\n");
		}
	}

	delete_popup(mywin);

	if (new != NULL && changed)
	{
		new -> colorpair_index = find_or_init_colorpair(cur_fg, cur_bg, 1);
		new -> attrs = cur_attr;
	}

	return changed;
}

int swap_window(void)
{
	int swapped = 0;

	if (nfd > 1)
	{
		int f1_index, f2_index;

		f1_index = select_window(HELP_SWAP_WIN1, "Swap windows, select window 1");
		if (f1_index != -1)
		{
			f2_index = select_window(HELP_SWAP_WIN2, "Swap windows, select window 2");
			if (f2_index != -1)
			{
				proginfo dummy;
				buffer dummy2;

				buffer_replace_pi_pointers(f1_index, &pi[f1_index], &pi[f2_index]);
				buffer_replace_pi_pointers(f2_index, &pi[f2_index], &pi[f1_index]);

				memmove(&dummy, &pi[f1_index], sizeof(proginfo));
				memmove(&pi[f1_index], &pi[f2_index], sizeof(proginfo));
				memmove(&pi[f2_index], &dummy, sizeof(proginfo));

				memmove(&dummy2, &lb[f1_index], sizeof(buffer));
				memmove(&lb[f1_index], &lb[f2_index], sizeof(buffer));
				memmove(&lb[f2_index], &dummy2, sizeof(buffer));

				swapped = 1;
			}
		}
	}
	else
	{
		error_popup("Swap windows", -1, "There's only 1 window!");
	}

	return swapped;
}

void restart_window(void)
{
	int f_index = 0;

	if (nfd > 1)
		f_index = select_window(HELP_SELECT_RESTART_WINDOW, "Select window to restart");

	if (f_index != -1)
	{
		char all = 1;

		if (pi[f_index].next)
		{
			NEWWIN *mywin = create_popup(13, 24);

			win_header(mywin, "Restart window");
			escape_print(mywin, 3, 2, "Restart all? (^y^/^n^)");
			mydoupdate();

			all = ask_yes_no(HELP_SELECT_RESTART_WINDOW_ALL, mywin);

			delete_popup(mywin);
		}

		if (all == 1)	/* pressed 'Y' */
		{
			proginfo *cur = &pi[f_index];

			do
			{
				do_restart_window(cur);

				cur = cur -> next;
			}
			while(cur);
		}
		else
		{
			do_restart_window(&pi[f_index]);
		}
	}
}

int delete_window(void)
{
	int deleted = 0;
	int f_index = 0;

	/* only popup selectionbox when more then one window */
	if (nfd > 1)
		f_index = select_window(HELP_DELETE_SELECT_WINDOW, "Select window to delete");

	/* user did not press q/x? */
	if (f_index != -1)
	{
		char all = 1;

		/* multiple files for this window? then ask if delete all
		 * or just one
		 */
		if (pi[f_index].next)
		{
			NEWWIN *mywin = create_popup(13, 24);

			win_header(mywin, "Delete window");
			escape_print(mywin, 3, 2, "Delete all? (^y^/^n^)");
			mydoupdate();

			all = ask_yes_no(HELP_DELETE_WINDOW_DELETE_ALL_SUBWIN, mywin);

			delete_popup(mywin);
		}

		if (all == 1)	/* pressed 'Y' */
		{
			delete_entry(f_index, NULL);
			deleted = 1;
		}
		else if (all == 0) /* pressed 'N' */
		{
			proginfo *cur = select_subwindow(f_index, HELP_DELETE_SELECT_SUBWINDOW, "Select subwindow");

			if (cur)
			{
				delete_entry(f_index, cur);
				deleted = 1;
			}
		}
		/* otherwhise: pressed 'Q' */
	}

	return deleted;
}

int hide_window(void)
{
	int f_index = select_window(HELP_HIDE_WINDOW, "Select window to hide");

	if (f_index != -1)
		pi[f_index].hidden = 1 - pi[f_index].hidden;

	return f_index != -1 ? 1 : 0;
}

int hide_all_but(void)
{
	int f_index = select_window(HELP_HIDE_BUT_WINDOW, "Select window to keep open");
	int loop;

	if (f_index != -1)
	{
		for(loop=0; loop<nfd; loop++)
		{
			if (loop != f_index)
				pi[loop].hidden = 1;
			else
				pi[loop].hidden = 0;
		}
	}

	return 1;
}

int unhide_all_windows(void)
{
	int loop;
	int one_unhidden = 0;

	for(loop=0; loop<nfd; loop++)
	{
		if (pi[loop].hidden)
		{
			pi[loop].hidden = 0;
			one_unhidden = 1;
		}
	}

	return one_unhidden;
}

char ask_negate_regexp(NEWWIN *win, int line)
{
	escape_print(win, line, 2, "Negate regular expression? (^y^/^n^)");
	mydoupdate();

	return ask_yes_no(HELP_NEGATE_REGEXP, win);
}

int select_schemes(void *schemes_in, int n_schemes_in, scheme_t tscheme, int_array_t *schemes_out)
{
	NEWWIN *win;
	int loop, cur_offset = 0, cur_line = 0;
	char *selected_schemes = NULL;
	int c = 0;

	if (n_schemes_in == 0)
	{
		error_popup("Error selecting schemes", -1, "Cannot select %s schemes: none defined in configuration file!", tscheme == SCHEME_COLOR?"color":"conversion");
		return -1;
	}

	/* create array that says for each colorscheme if it is selected or not */
	selected_schemes = (char *)mymalloc(n_schemes_in, __FILE__, __PRETTY_FUNCTION__, __LINE__);
	memset(selected_schemes, 0x00, n_schemes_in);
	for(loop=0; loop<get_iat_size(schemes_out); loop++)
		selected_schemes[get_iat_element(schemes_out, loop)] = 1;

	/* draw gui */
	win = create_popup(20, 75);

	for(;;)
	{
		werase(win -> win);

		win_header(win, "Select scheme(s)");
		mvwprintw(win -> win, 2, 2, "Space to toggle, enter to proceed, ^g to abort");
		for(loop=0; loop<16; loop++)
		{
			char *descr = "", *name;

			if (cur_offset + loop >= n_schemes_in) break;

			if (tscheme == SCHEME_COLOR)
			{
				name  = ((color_scheme *)schemes_in)[cur_offset + loop].name;
				descr = ((color_scheme *)schemes_in)[cur_offset + loop].descr;
			}
			else
			{
				name  = ((conversion *)schemes_in)[cur_offset + loop].name;
			}

			mvwprintw(win -> win, 3 + loop, 26, "%s", descr);
			if (loop == cur_line) ui_inverse_on(win);
			mvwprintw(win -> win, 3 + loop, 2, "[%c] %s", selected_schemes[cur_offset + loop]?'X':' ', name);
			if (loop == cur_line) ui_inverse_off(win);
		}
		draw_border(win);
		mydoupdate();

		c = toupper(wait_for_keypress(tscheme == SCHEME_COLOR ? HELP_SELECT_COLORSCHEMES : HELP_SELECT_CONVERSIONSCHEMES, 0, win, 1));
		if (c == abort_key || c == 'Q' || c == 'X')
		{
			c = -1;
			break;
		}
		else if (c == KEY_HOME)
		{
			cur_line = cur_offset = 0;
		}
		else if (c == KEY_END)
		{
			cur_line = 0;
			cur_offset = n_schemes_in - 1;
		}
		else if (c == KEY_PPAGE)
		{
		 	int todo = 16 - cur_line;

			cur_line = 0;

			if (cur_offset > todo)
				cur_offset -= todo;
			else
				cur_offset = 0;
		}
		else if (c == KEY_NPAGE)
		{
			int todo = cur_line;

			cur_line = 15;

			if ((cur_offset + todo) < (n_schemes_in - 1))
				cur_offset += todo;
			else
				cur_offset = n_schemes_in - 1;
		}
		else if (c == KEY_UP)
		{
			if (cur_line > 0)
				cur_line--;
			else if (cur_offset > 0)
				cur_offset--;
			else
				wrong_key();
		}
		else if (c == KEY_DOWN)
		{
			if ((cur_line + cur_offset) < (n_schemes_in - 1))
			{
				if (cur_line < (16 - 1))
					cur_line++;
				else
					cur_offset++;
			}
			else
				wrong_key();
		}
		else if (c == ' ')
		{
			selected_schemes[cur_line + cur_offset] = 1 - selected_schemes[cur_line + cur_offset];
		}
		else if (c == 13)
		{
			break;
		}
		else
			wrong_key();
	}

	if (c != -1)
	{
		/* return result */
		free_iat(schemes_out);

		for(loop=0; loop<n_schemes_in; loop++)
		{
			if (selected_schemes[loop])
			{
				if (tscheme == SCHEME_COLOR)
					add_color_scheme(schemes_out, loop);
				else
					add_conversion_scheme(schemes_out, ((conversion *)schemes_in)[loop].name);
			}
		}
	}
	myfree(selected_schemes);

	delete_popup(win);

	return c != -1 ? 0 : -1;
}

int ask_terminal_emulation(term_t *term)
{
	int rc = 0;
	NEWWIN *popup = create_popup(9, 44);

	win_header(popup, "Select terminal emulation");
	mvwprintw(popup -> win, 2, 2, "1. ANSI/VT100");
	mvwprintw(popup -> win, 3, 2, "2. no emulation");
	mvwprintw(popup -> win, 4, 2, "0. abort");
	mvwprintw(popup -> win, 6, 2, "Please contact folkert@vanheusden.com if");
	mvwprintw(popup -> win, 7, 2, "you require other emulations as well.");
	mydoupdate();

	for(;;)
	{
		int c = wait_for_keypress(HELP_SELECT_TERMINAL_EMULATION, 0, popup, 1);

		if (c == '1')
		{
			*term = TERM_ANSI;
			break;
		}
		else if (c == '2')
		{
			*term = TERM_IGNORE;
			break;
		}
		else if (c == '0' || toupper(c) == 'Q' || c == abort_key)
		{
			rc = -1;
			break;
		}

		wrong_key();
	}

	delete_popup(popup);

	return rc;
}

char ask_colors(NEWWIN *win, int line, char cur, char *fieldnr, char **fielddel, int_array_t *color_schemes, myattr_t *attrs, term_t *term_emul)
{
	char ok = 0;

	escape_print(win, line, 2, "Colors? (^s^yslog/^m^isc/^f^ield/^n^one/^S^cheme/^l^ist/^t^erm)");

	mydoupdate();
	for(;;)
	{
		int c = wait_for_keypress(HELP_ASK_COLORS, 0, win, 0);

		if (c == abort_key) return -1;

		switch(c)
		{
			case 't':
				if (ask_terminal_emulation(term_emul) == 0)
					return 0;

				return -1;

			case 'q':
				return -1;

			case 'S':
				if (select_schemes(cschemes, n_cschemes, SCHEME_COLOR, color_schemes) == -1)
					return -1;
				return 'S';

			case 's':
			case 'm':
				return 'm';

			case 'n':
				return 0;

			case 'l':
				{
					myattr_t new_attrs = { -1, -1 };

					if (color_management(attrs, &new_attrs))
					{
						*attrs = new_attrs;
						return 'i';
					}
				}
				break;

			case 'f':
				{
					int loop, fld_dummy;
					char *dummy, nr[5]; /* xxx\0 and a little :-) */

					snprintf(nr, sizeof(nr), "%d", *fieldnr);

					mvwprintw(win -> win, line + 2, 1, "Enter field number: ");
					dummy = edit_string(win, line + 3, 2, 42, 39, 1, nr, HELP_COLORS_FIELD_NR, -1, NULL, NULL);
					if (dummy)
					{
						fld_dummy = atoi(dummy);
						myfree(dummy);

						mvwprintw(win -> win, line + 4, 1, "Enter delimiter (eg a space):");
						dummy = edit_string(win, line + 5, 2, 42, 39, 0, *fielddel, HELP_COLORS_FIELD_DELIMITER, -1, NULL, NULL);

						if (dummy)
						{
							myfree(*fielddel);
							*fielddel = dummy;
							*fieldnr = fld_dummy;
							ok = 1;
						}
					}

					for(loop=0; loop<4; loop++)
						mvwprintw(win -> win, line + 1 + loop, 1, "                              ");
				}

				if (ok)
					return 'f';
				break;

			case 13:
				if (cur != (char)-1)
					return cur;

			default:
				wrong_key();
		}
	}
}

char ask_regex_type(NEWWIN *win, int line)
{
	escape_print(win, line+0, 2, "Usage of regexp? (^m^atch, ^v^ do not match");
	escape_print(win, line+1, 2, "^C^olor, ^B^ell, ^b^ell + colorize, e^x^ecute)");
	mydoupdate();

	for(;;)
	{
		int c = wait_for_keypress(HELP_REGEXP_USAGE, 0, win, 0);

		if (c == 'm' || toupper(c) == 'C' || toupper(c) == 'B' || toupper(c) == 'X' || c == 'v')
			return c;

		if (toupper(c) == 'Q' || c == abort_key)
			return -1;

		wrong_key();
	}
}

int add_window(void)
{
	int added = 0;
	char *fname;
	NEWWIN *mywin = create_popup(12, 53);
	int col, fc;
	proginfo *cur = NULL;
	proginfo *set_next_pointer = NULL;
	char ask_add_to;
	int what_help = -1;

	if (nfd)
		ask_add_to = 1;
	else
		ask_add_to = 0;

	for(;;)
	{
		int_array_t color_schemes = { NULL, 0, 0 };
		char field_nr = 0;
		char *field_del = NULL;
		char follow_filename = 0;
		char see_difference = 0;
		myattr_t cdev = { -1, -1 };
		term_t term_emul = TERM_IGNORE;

		win_header(mywin, "Add window");

		if (ask_add_to)
		{
			int rc;

			ask_add_to = 0;
			mvwprintw(mywin -> win, 2, 2, "Add (merge) to existing window?");
			mydoupdate();

			rc = ask_yes_no(HELP_ADD_WINDOW_MERGE_SUBWIN, mywin);

			if (rc == 1)	/* pressed 'Y' */
			{
				int index = 0;

				if (nfd > 1)
					index = select_window(HELP_ADD_WINDOW_SELECT_MERGE_WINDOW, NULL);

				if (index == -1)
					break;

				cur = &pi[index];
			}
			else if (rc == -1)	/* pressed 'Q' */
			{
				break;
			}
		}

		/* check if this extra window will still fit */
		if (!cur)
		{
			if ((max_y / (nfd + 1)) < 3)
			{
				error_popup("Add window", -1, "More windows would not fit.");
				break;
			}
		}

		escape_print(mywin, 2, 2, "File or command? (^f^/^c^)         ");
		mydoupdate();
		for(;;)
		{
			fc = toupper(wait_for_keypress(HELP_ADD_FILE_OR_CMD, 0, mywin, 0));

			if (fc == 'F' || fc == 'C' || fc == 'Q' || fc == abort_key)
				break;

			wrong_key();
		}

		if (fc == 'Q' || fc == abort_key)
			break;
		else if (fc == 'F')
		{
			mvwprintw(mywin -> win, 2, 2, "Enter filename:       ");
			what_help = HELP_ENTER_FILENAME_TO_MONITOR;
		}
		else
		{
			mvwprintw(mywin -> win, 2, 2, "Enter command:        ");
			what_help = HELP_ENTER_CMD_TO_MONITOR;
		}

		fname = edit_string(mywin, 3, 2, 41, find_path_max(), 0, NULL, what_help, -1, &cmdfile_h, NULL);
		if (!fname)
			break;

		if (fc == 'F')
		{
			escape_print(mywin, 4, 2, "Follow filename? (^y^/^n^)");
			mydoupdate();
			follow_filename = ask_yes_no(HELP_ADD_FILE_FOLLOW_FILENAME, mywin);
		}

		col = ask_colors(mywin, 5, -1, &field_nr, &field_del, &color_schemes, &cdev, &term_emul);
		if (col == -1)
			break;

		if (cur == NULL)
		{
			create_new_win(&cur, NULL);

			added = 1;
		}
		else
		{
			/* skip to end of chain */
			while (cur -> next)
				cur = cur -> next;
			/* the last entry in the chain will point to this newly created structure
			 * don't set this pointer yet as this will give problems as soon as we
			 * call wait_for_keypress()
			 */
			set_next_pointer = cur;

			/* allocate new entry */
			cur = (proginfo *)mymalloc(sizeof(proginfo), __FILE__, __PRETTY_FUNCTION__, __LINE__);
		}
		memset(cur, 0x00, sizeof(proginfo));

		cur -> restart.restart = -1;
		if (fc == 'C')
		{
			char *dummy;

			mvwprintw(mywin -> win, 6, 2, "Repeat command interval: (empty for don't)");
			dummy = edit_string(mywin, 7, 2, 10, 10, 1, NULL, HELP_ADD_WINDOW_REPEAT_INTERVAL, -1, NULL, NULL);
			if (dummy)
			{
				cur -> restart.restart = atoi(dummy);
				myfree(dummy);

				mvwprintw(mywin -> win, 8, 2, "Do you whish to only see the difference?");
				mydoupdate();
				see_difference = ask_yes_no(HELP_ADD_FILE_DISPLAY_DIFF, mywin);
			}
		}
		cur -> filename = fname;
		cur -> status = cur -> data = NULL;
		cur -> wt = (fc == 'F' ? WT_FILE : WT_COMMAND);
		cur -> retry_open = 0;
		cur -> follow_filename = follow_filename;
		cur -> restart.do_diff = see_difference;
		cur -> next = NULL;
		cur -> restart.first = 1;
		cur -> line_wrap = 'a';
		cur -> win_height = -1;
		cur -> statistics.sccfirst = 1;
		cur -> cdef.colorize = col;
		cur -> cdef.field_nr = field_nr;
		cur -> cdef.field_del = field_del;
		cur -> cdef.color_schemes = color_schemes;
		cur -> cdef.attributes = cdev;
		cur -> cdef.term_emul = term_emul;

		init_iat(&cur -> conversions);

		cur -> statistics.start_ts = time(NULL);

		/* start tail-process */
		if (start_proc(cur, max_y / (nfd + 1)) == -1)
		{
			mvwprintw(mywin -> win, 8, 2, "error opening file!");
			mydoupdate();
			wait_for_keypress(HELP_FAILED_TO_START_TAIL, 0, mywin, 0);
			nfd--;
			break;
		}

		/* and set pointer to the newly allocated entry */
		if (set_next_pointer)
			set_next_pointer -> next = cur;

		mvwprintw(mywin -> win, 10, 2, "Add another to this new window?");
		mydoupdate();
		if (ask_yes_no(HELP_MERGE_ANOTHER_WINDOW, mywin) <= 0)
			break;

		werase(mywin -> win);
		draw_border(mywin);
	}

	delete_popup(mywin);

	return added;
}

int toggle_colors(void)
{
	int changed = 0;
	int f_index = 0;

	if (nfd > 1)
		f_index = select_window(HELP_TOGGLE_COLORS_SELECT_WINDOW, "Toggle colors: select window");

	if (f_index != -1)
	{
		char col;
		proginfo *cur = &pi[f_index];
		NEWWIN *mywin = NULL;
		char *dummy = NULL;

		if (cur -> next)
			cur = select_subwindow(f_index, HELP_TOGGLE_COLORS_SELECT_SUBWINDOW, "Toggle colors: select subwindow");

		if (cur)
		{
			mywin = create_popup(11, 53);
			win_header(mywin, "Toggle colors");

			dummy = mystrdup(cur -> filename, __FILE__, __PRETTY_FUNCTION__, __LINE__);
			dummy[min(strlen(dummy), 40)] = 0x00;
			mvwprintw(mywin -> win, 3, 1, dummy);

			col = ask_colors(mywin, 4, cur -> cdef.colorize, &cur -> cdef.field_nr, &cur -> cdef.field_del, &cur -> cdef.color_schemes, &cur -> cdef.attributes, &cur -> cdef.term_emul);
			if (col != (char)-1)
			{
				changed = 1;
				cur -> cdef.colorize = col;
			}

			delete_popup(mywin);
			myfree(dummy);
		}
	}

	return changed;
}

void swap_re(re *a, re *b)
{
	re temp;

	memcpy(&temp, a, sizeof(temp));
	memcpy(a, b, sizeof(*a));
	memcpy(b, &temp, sizeof(*b));
}

int edit_regexp(void)
{
	int changed = 0;
	NEWWIN *mywin;
	proginfo *cur;
	int f_index = 0;
	int cur_re = 0;
	mybool_t case_insensitive = re_case_insensitive;

	/* select window */
	if (nfd > 1)
		f_index = select_window(HELP_ENTER_REGEXP_SELECT_WINDOW, "Select window (reg.exp. editing)");

	if (f_index == -1)	/* user pressed Q/X */
		return 0;

	/* select subwindow */
	cur = &pi[f_index];
	if (cur -> next)
		cur = select_subwindow(f_index, HELP_ENTER_REGEXP_SELECT_SUBWINDOW, "Select subwindow (reg.exp. editing)");
	if (!cur)
		return 0;

	/* create window */
	mywin = create_popup(23, 70);

	for(;;)
	{
		int c, key;
		int loop;
		char buffer[58 + 1];
		buffer[58] = 0x00;

		werase(mywin -> win);
		win_header(mywin, "Edit reg.exp.");
		mvwprintw(mywin -> win, 2, 2, "%s", cur -> filename);
		escape_print(mywin, 3, 2, "^a^dd, ^e^dit, ^d^elete, ^q^uit, move ^D^own, move ^U^p, ^r^eset counter");

		/* display them lines */
		for(loop=0; loop<cur -> n_re; loop++)
		{
			strncpy(buffer, (cur -> pre)[loop].regex_str, 34);
			if (loop == cur_re)
				ui_inverse_on(mywin);
			mvwprintw(mywin -> win, 4 + loop, 1, "%c%c %s", 
					zerotomin((cur -> pre)[loop].invert_regex),
					zerotomin((cur -> pre)[loop].use_regex),
					buffer);
			if (toupper((cur -> pre)[loop].use_regex) == 'X')
			{
				char dummy[18];
				strncpy(dummy, (cur -> pre)[loop].cmd, min(17, strlen((cur -> pre)[loop].cmd)));
				dummy[17]=0x00;
				mvwprintw(mywin -> win, 4 + loop, 42, dummy);
				wmove(mywin -> win, 4 + loop, 41);
			}
			if (loop == cur_re)
				ui_inverse_off(mywin);
			mvwprintw(mywin -> win, 4 + loop, 60, "%d", (cur -> pre)[loop].match_count);
		}
		draw_border(mywin);
		mydoupdate();

		/* wait for key */
		for(;;)
		{
			key = wait_for_keypress(HELP_REGEXP_MENU, 2, mywin, 1);
			c = key;

			/* convert return to 'E'dit */
			if (key == 13)
				key = c = 'E';

			/* any valid keys? */
			if (toupper(c) == 'Q' || c == abort_key || toupper(c) == 'X' || c == 'a' || c == 'e' || toupper(c) == 'D' || c == 'U' || key == KEY_DOWN || key == 13 || key == KEY_UP || c == 'r')
				break;

			if (key == -1) /* T/O waiting for key? then update counters */
			{
				for(loop=0; loop<cur -> n_re; loop++)
					mvwprintw(mywin -> win, 4 + loop, 60, "%d", (cur -> pre)[loop].match_count);

				mydoupdate();
			}
			else
				wrong_key();
		}
		/* exit this screen? */
		if (toupper(c) == 'Q' || c == abort_key || toupper(c) == 'X')
			break;

		if (key == KEY_UP)
		{
			if (cur_re > 0)
				cur_re--;
			else
				wrong_key();
		}
		else if (key == KEY_DOWN || key == 13)
		{
			if (cur_re < (cur -> n_re -1))
				cur_re++;
			else
				wrong_key();
		}
		/* add or edit */
		else if (c == 'a' || c == 'e')
		{
			int invert_regex = 0, regex_type;
			int rc;
			char *regex_str, *cmd_str = NULL;

			/* max. 10 regular expressions */
			if (c == 'a' && cur -> n_re == 10)
			{
				wrong_key();
				continue;
			}
			if (c == 'e' && cur -> n_re == 0)
			{
				wrong_key();
				continue;
			}

			/* ask new reg exp */
			mvwprintw(mywin -> win, 15, 2, "Edit regular expression:");
			if (c == 'e')
				regex_str = edit_string(mywin, 16, 2, 58, 128, 0, (cur -> pre)[cur_re].regex_str, HELP_ENTER_REGEXP, -1, &search_h, &case_insensitive);
			else
				regex_str = edit_string(mywin, 16, 2, 58, 128, 0, NULL, HELP_ENTER_REGEXP, -1, &search_h, &case_insensitive);

			/* user did not abort edit? */
			if (regex_str == NULL)
				continue;

			regex_type = ask_regex_type(mywin, 18);
			if (regex_type == -1)
				continue;

			if (regex_type != 'm' && regex_type != 'v')
			{
				invert_regex = ask_negate_regexp(mywin, 17);
				if (invert_regex == -1)	/* pressed 'Q' */
					continue;
			}

			if (toupper(regex_type) == 'X')
			{
				mvwprintw(mywin -> win, 20, 2, "Edit command:");
				if (c == 'e')
					cmd_str = edit_string(mywin, 21, 2, 58, 128, 0, (cur -> pre)[cur_re].cmd, HELP_ENTER_CMD, -1, &cmdfile_h, NULL);
				else
					cmd_str = edit_string(mywin, 21, 2, 58, 128, 0, NULL, HELP_ENTER_CMD, -1, &cmdfile_h, NULL);

				/* did the user cancel things? */
				if (!cmd_str)
				{
					/* then free the memory for the new regexp
					 * and continue with the menu
					 */
					myfree(regex_str);
					continue;
				}
			}

			changed = 1;

			/* edit: free previous */
			if (c == 'e')
			{
				regfree(&(cur -> pre)[cur_re].regex);
				myfree((cur -> pre)[cur_re].regex_str);
			}
			/* add: allocate new */
			else
			{
				cur_re = cur -> n_re++;
				cur -> pre = (re *)myrealloc(cur -> pre, cur -> n_re * sizeof(re), __FILE__, __PRETTY_FUNCTION__, __LINE__);
				memset(&(cur -> pre)[cur_re], 0x00, sizeof(re));
			}

			/* wether to negate this expression or not */
			(cur -> pre)[cur_re].invert_regex = invert_regex;

			/* compile */
			if ((rc = regcomp(&(cur -> pre)[cur_re].regex, regex_str, REG_EXTENDED | (case_insensitive == MY_TRUE?REG_ICASE:0))))
			{
				regexp_error_popup(rc, &(cur -> pre)[cur_re].regex);

				if (c == 'a')
				{
					cur -> n_re--;
					if (cur -> n_re == cur_re)
						cur_re--;
				}

				myfree(regex_str);
			}
			else
			{
				/* compilation went well, remember everything */
				(cur -> pre)[cur_re].regex_str = regex_str;
				(cur -> pre)[cur_re].use_regex = regex_type;
				(cur -> pre)[cur_re].cmd       = cmd_str;
			}

			/* reset counter (as it is a different regexp which has not matched anything at all) */
			(cur -> pre)[cur_re].match_count = 0;
		}
		/* delete entry */
		else if (c == 'd')
		{
			changed = 1;

			/* delete entry */
			free_re(&(cur -> pre)[cur_re]);

			/* update administration */
			if (cur -> n_re == 1)
			{
				myfree(cur -> pre);
				cur -> pre = NULL;
			}
			else
			{
				int n_to_move = (cur -> n_re - cur_re) - 1;

				if (n_to_move > 0)
					memmove(&(cur -> pre)[cur_re], &(cur -> pre)[cur_re+1], sizeof(re) * n_to_move);
			}
			cur -> n_re--;

			/* move cursor */
			if (cur_re > 0 && cur_re == cur -> n_re)
			{
				cur_re--;
			}
		}
		else if (c == 'D')
		{
			if (cur_re < (cur -> n_re - 1))
			{
				swap_re(&(cur -> pre)[cur_re], &(cur -> pre)[cur_re + 1]);
			}
			else
				wrong_key();
		}
		else if (c == 'U')
		{
			if (cur_re > 0)
			{
				swap_re(&(cur -> pre)[cur_re], &(cur -> pre)[cur_re - 1]);
			}
			else
				wrong_key();
		}
		else if (c == 'r')
		{
			(cur -> pre)[cur_re].match_count = 0;
		}
	}

	delete_popup(mywin);

	return changed;
}

int toggle_vertical_split(void)
{
	int changed = 0;

	if (split)
	{
		if ((max_y / nfd) < 3)
		{
			error_popup("Toggle vertical split", -1, "That is not possible: the new configuration won't fit.");
		}
		else
		{
			split = 0;
			changed = 1;
			myfree(vertical_split);
			vertical_split = NULL;
			myfree(n_win_per_col);
			n_win_per_col = NULL;
		}
	}
	else
	{
		if (nfd == 1)
			error_popup("Toggle vertical split", -1, "There's only 1 window!");
		else
		{
			char *str;
			NEWWIN *mywin = create_popup(7, 35);

			escape_print(mywin, 2, 2, "Enter number of columns");
			str = edit_string(mywin, 4, 2, 20, 3, 1, NULL, HELP_SET_VERTICAL_SPLIT_N_WIN, -1, NULL, NULL);
			if (str)
			{
				split = atoi(str);
				if (split > 1)
				{
					changed = 1;
				}
				else
				{
					split = 0;
				}

				myfree(str);
			}

			delete_popup(mywin);
		}
	}

	return changed;
}

void list_keybindings(void)
{
	NEWWIN *mywin;
	int prevpos = -1, curpos = 0;

	mywin = create_popup(20, 40);

	for(;;)
	{
		int c;

		if (curpos != prevpos)
		{
			int loop;

			werase(mywin -> win);

			win_header(mywin, "Keybindings");

			for(loop=0; loop<17; loop++)
			{
				if ((loop + curpos) >= n_keybindings)
					break;

				ui_inverse_on(mywin);
				mvwprintw(mywin -> win, 2 + loop, 2, "%2s", key_to_keybinding(keybindings[loop + curpos].key));
				ui_inverse_off(mywin);
				mvwprintw(mywin -> win, 2 + loop, 5, "%s", keybindings[loop + curpos].command);
			}

			draw_border(mywin);
			mydoupdate();

			prevpos = curpos;

		}

		c = wait_for_keypress(HELP_LIST_KEYBINDINGS, 0, mywin, 0);

		if (toupper(c) == 'Q' || c == abort_key)
			break;
		else if (c == KEY_UP && curpos > 0)
			curpos--;
		else if ((c == KEY_DOWN || c == 13) && curpos < (n_keybindings - 1))
			curpos++;
		else
			wrong_key();
	}

	delete_popup(mywin);
}

void write_script(void)
{
	FILE *fh = NULL;
	char *fname;
	NEWWIN *mywin = create_popup(12, 42);

	win_header(mywin, "Write script");

	mvwprintw(mywin -> win, 2, 2, "Enter filename:");

	fname = edit_string(mywin, 3, 2, 41, find_path_max(), 0, NULL, HELP_WRITE_SCRIPT, -1, &cmdfile_h, NULL);
	if (fname)
		fh = fopen(fname, "w");

	if (fh)
	{
		int loop;

		fprintf(fh, "#!/bin/sh\n\n");

		fprintf(fh, "multitail");

		if (heartbeat_interval != 0.0)
			fprintf(fh, " -H %f", heartbeat_interval);

		if (config_file)
		{
			fprintf(fh, " -F ");
			write_escape_str(fh, config_file);
		}

		/* markerline color */
		if (markerline_attrs.colorpair_index != -1 || markerline_attrs.attrs != -1)
		{
			fprintf(fh, " -Z ");
			emit_myattr_t(fh, markerline_attrs);
		}

		if (timestamp_in_markerline)
			fprintf(fh, " -T");

		if (show_subwindow_id)
			fprintf(fh, " -S");

		if (set_title)
		{
			fprintf(fh, " -x ");
			write_escape_str(fh, set_title);
		}

		if (tab_width != DEFAULT_TAB_WIDTH)
			fprintf(fh, " -b %d", tab_width);

		if (update_interval)
			fprintf(fh, " -u %d", update_interval);

		if (vertical_split)
		{
			fprintf(fh, " -sw ");

			for(loop=0; loop<split; loop++)
			{
				if (loop) fprintf(fh, ",");

				fprintf(fh, "%d", vertical_split[loop]);
			}
		}
		else if (split)
			fprintf(fh, " -s %d", split);

		if (n_win_per_col)
		{
			fprintf(fh, " -sn ");

			for(loop=0; loop<split; loop++)
			{
				if (loop) fprintf(fh, ",");

				fprintf(fh, "%d", n_win_per_col[loop]);
			}
		}

		if (!use_colors)
			fprintf(fh, " -w");

		if (mode_statusline == 0)
			fprintf(fh, " -d");
		else if (mode_statusline == -1)
			fprintf(fh, " -D");

		if (!warn_closed)
			fprintf(fh, " -z");


		for(loop=0; loop<nfd; loop++)
		{
			int conv_index;
			proginfo *cur = &pi[loop];

			if (lb[loop].maxnlines != -1)
				fprintf(fh, " -m %d", lb[loop].maxnlines);
			else if (lb[loop].maxbytes != -1)
				fprintf(fh, " -mb %d", lb[loop].maxbytes);

			do
			{
				int lloop;

				if (cur -> beep.beep_interval > 0)
					fprintf(fh, " --bi %d", cur -> beep.beep_interval);

				if (cur -> check_interval)
					fprintf(fh, " -iw %d", cur -> check_interval);

				if (cur -> cont)
					fprintf(fh, " --cont");

				/* terminal emulation */
				if (cur -> cdef.term_emul == TERM_ANSI)
					fprintf(fh, " -cT ANSI"); /* vt100 */

				/* add timestamp to lines */
				if (cur -> add_timestamp)
					fprintf(fh, " -ts");

				if (cur -> label)
					fprintf(fh, " --label %s", cur -> label);

				/* title in statusline */
				if (cur -> win_title)
				{
					fprintf(fh, " -t ");
					write_escape_str(fh, cur -> win_title);
				}

				if (cur -> mark_interval)
					fprintf(fh, " --mark-interval %d", cur -> mark_interval);

				if (cur -> repeat.suppress_repeating_lines)
					fprintf(fh, " --no-repeat");

				/* height of window */
				if (cur -> win_height != -1)
					fprintf(fh, " -wh %d", cur -> win_height);

				/* redirect output to a file or other command */
				for(lloop=0; lloop < cur -> n_redirect; lloop++)
				{
					if ((cur -> predir)[lloop].type == REDIRECTTO_PIPE || (cur -> predir)[lloop].type == REDIRECTTO_PIPE_FILTERED)
						fprintf(fh, " -g ");
					else if ((cur -> predir)[lloop].type == REDIRECTTO_FILE || (cur -> predir)[lloop].type == REDIRECTTO_FILE_FILTERED)
						fprintf(fh, " -a ");

					write_escape_str(fh, (cur -> predir)[lloop].redirect);
				}

				/* conversion scheme to use */
				for(conv_index=0; conv_index<get_iat_size(&cur -> conversions); conv_index++)
					fprintf(fh, " -cv %s", conversions[get_iat_element(&cur -> conversions, conv_index)].name);

				/* linewrap */
				if (cur -> line_wrap != 'a')
				{
					fprintf(fh, " -p %c", cur -> line_wrap);

					if (cur -> line_wrap == 'o')
						fprintf(fh, " %d", cur -> line_wrap_offset);
				}

				/* for commands: restart interval and do diff or not */
				if (cur -> restart.restart != -1)
				{
					char mode = 'r';

					if (cur -> restart.do_diff)
						mode = 'R';

					if (cur -> restart.restart_clear)
						fprintf(fh, " -%cc", mode);
					else
						fprintf(fh, " -%c", mode);

					fprintf(fh, " %d", cur -> restart.restart);
				}

				/* initial number of lines to tail */
				if (cur -> initial_n_lines_tail != -1)
					fprintf(fh, " -n %d", cur -> initial_n_lines_tail);

				/* folow filename or filedescriptor */
				if (cur -> follow_filename)
					fprintf(fh, " -f");

				/* if the file could not be openend, retry open? */
				if (cur ->  retry_open)
					fprintf(fh, " --retry");

				/* colors and colorschemes */
				if (cur -> cdef.colorize != 'n' && cur -> cdef.colorize != 0)
				{
					switch(cur -> cdef.colorize)
					{
						case 'a':
							fprintf(fh, " -ca ");
							emit_myattr_t(fh, cur -> cdef.alt_col_cdev1);
							fprintf(fh, " ");
							emit_myattr_t(fh, cur -> cdef.alt_col_cdev2);
							break;

						case 'i':
							fprintf(fh, " -ci ");
							emit_myattr_t(fh, cur -> cdef.attributes);
							break;

						case 's':
							fprintf(fh, " -cs");
							break;

						case 'm':
							fprintf(fh, " -cm");
							break;

						case 'f':
							fprintf(fh, " -cf %d ", cur -> cdef.field_nr);
							write_escape_str(fh, cur -> cdef.field_del);
							break;

						case 'S':
							{
								int cs_index;

								for(cs_index=0; cs_index<get_iat_size(&cur -> cdef.color_schemes); cs_index++)
									fprintf(fh, " -cS %s", cschemes[get_iat_element(&cur -> cdef.color_schemes, cs_index)].name);
							}
							break;
					}
				}

				/* regular expressions */
				for(lloop=0; lloop<cur -> n_re; lloop++)
				{
					if ((cur -> pre)[lloop].invert_regex)
						fprintf(fh, " -v");

					fprintf(fh, " -e%c ", (cur -> pre)[lloop].use_regex);
					write_escape_str(fh, (cur -> pre)[lloop].regex_str);

					if ((cur -> pre)[lloop].use_regex == 'x')
					{
						fprintf(fh, " ");
						write_escape_str(fh, (cur -> pre)[lloop].cmd);
					}
				}

				/* what to strip */
				for(lloop=0; lloop<cur -> n_strip; lloop++)
				{
					switch((cur -> pstrip)[lloop].type)
					{
						case STRIP_TYPE_REGEXP:
							fprintf(fh, " -ke ");
							write_escape_str(fh, (cur -> pstrip)[lloop].regex_str);
							break;

						case STRIP_KEEP_SUBSTR:
							fprintf(fh, " -kS ");
							write_escape_str(fh, (cur -> pstrip)[lloop].regex_str);
							break;

						case STRIP_TYPE_RANGE:
							fprintf(fh, " -kr %d %d", (cur -> pstrip)[lloop].start, (cur -> pstrip)[lloop].end);
							break;

						case STRIP_TYPE_COLUMN:
							fprintf(fh, " -kc ");
							write_escape_str(fh, (cur -> pstrip)[lloop].del);
							fprintf(fh, " %d", (cur -> pstrip)[lloop].col_nr);
							break;
					}
				}

				/* add filename/command */
				if (cur == &pi[loop])
				{
					if (cur -> wt == WT_COMMAND)
						fprintf(fh, " -l");
					else if (cur -> wt == WT_STDIN)
						fprintf(fh, " -j");
					else if (cur -> wt == WT_SOCKET)
						fprintf(fh, " --listen");
					else if (cur -> wt == WT_FILE)
					{
						if ((cur -> filename)[0] == '-')
							fprintf(fh, " -i");
					}
				}
				else
				{
					if (cur -> wt == WT_COMMAND)
						fprintf(fh, " -L");
					else if (cur -> wt == WT_STDIN)
						fprintf(fh, " -J");
					else if (cur -> wt == WT_SOCKET)
						fprintf(fh, " --Listen");
					else if (cur -> wt == WT_FILE)
						fprintf(fh, " -I");
				}
				fprintf(fh, " ");
				write_escape_str(fh, cur -> filename);

				cur = cur -> next;
			}
			while(cur);
		}

		if (fchmod(fileno(fh), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) == -1)
		{
			error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "write_script: error setting mode-bits on output file.\n");
		}

		fprintf(fh, "\n");

		fclose(fh);
	}
	else if (fname != NULL)
	{
		error_popup("Write script", HELP_ERROR_WRITE_SCRIPT_CREATE_FILE, "Error creating file: %s", strerror(errno));
	}

	myfree(fname);

	delete_popup(mywin);
}

void selective_pause()
{
	int selected_window = select_window(HELP_PAUSE_A_WINDOW, "Pause a window");

	if (selected_window != -1)
	{
		pi[selected_window].paused = 1 - pi[selected_window].paused;
		update_statusline(pi[selected_window].status, selected_window, &pi[selected_window]);
		mydoupdate();
	}
}

void do_pause(void)
{
	NEWWIN *mywin = create_popup(3, 8);

	color_on(mywin, 1);
	mvwprintw(mywin -> win, 1, 1, "Paused");
	draw_border(mywin);
	color_off(mywin, 1);

	wmove(mywin -> win, 1, 2);
	mydoupdate();

	(void)getch();

	delete_popup(mywin);
}

void set_buffering(void)
{
	char winchoice, whatlines = 'a', maxnlines = 0;
	NEWWIN *mywin = create_popup(13, 40);

	win_header(mywin, "Set buffer size");

	if (nfd > 1)
	{
		escape_print(mywin, 3, 2, "Set on ^a^ll or ^o^ne");
		mvwprintw(mywin -> win, 4, 2, "window(s)?");
		mydoupdate();

		for(;;)
		{
			winchoice = toupper(wait_for_keypress(HELP_SET_BUFFERING, 0, mywin, 0));

			if (winchoice == 'Q' || winchoice == 'X' || winchoice == abort_key)
			{
				winchoice = 'Q';
				break;
			}

			if (winchoice == 'A' || winchoice == 'O')
				break;

			wrong_key();
		}
	}
	else
	{
		winchoice = 'A';
	}

	/* ask wether to store all lines or just the ones matchine to the
	 * regular expression (if any)
	 */
	if (winchoice != 'Q' && winchoice != abort_key)
	{
		escape_print(mywin, 5, 2, "Store ^a^ll or ^m^atching regular");
		mvwprintw(mywin -> win, 6, 2, "expressions (if any)?");
		mydoupdate();
		for(;;)
		{
			whatlines = toupper(wait_for_keypress(HELP_SET_BUFFERING_STORE_WHAT, 0, mywin, 0));
			if (whatlines == 'Q' || whatlines == 'X' || whatlines == abort_key)
			{
				winchoice = 'Q';
				break;
			}

			if (whatlines == 'A' || whatlines == 'M')
				break;

			wrong_key();
		}
	}

	/* get number of lines to store */
	if (winchoice != 'Q' && winchoice != abort_key)
	{
		char *dummy;

		mvwprintw(mywin -> win, 7, 2, "Store how many lines? (0 for all)");

		dummy = edit_string(mywin, 8, 2, 40, 7, 1, NULL, HELP_ENTER_NUMBER_OF_LINES_TO_STORE, -1, NULL, NULL);

		if (!dummy)
		{
			winchoice = 'Q';
		}
		else
		{
			maxnlines = atoi(dummy);

			myfree(dummy);
		}
	}

	/* do set mark */
	if (winchoice != 'Q' && winchoice != abort_key)
	{
		if (winchoice == 'A')
		{
			int loop;

			for(loop=0; loop<nfd; loop++)
			{
				do_set_bufferstart(loop, whatlines == 'A' ? 'a' : 'm', maxnlines);
			}
		}
		else
		{
			int window = select_window(HELP_SET_BUFFERING_SELECT_WINDOW, NULL);

			if (window != -1)
			{
				do_set_bufferstart(window, whatlines == 'A' ? 'a' : 'm', maxnlines);
			}
		}
	}

	delete_popup(mywin);
}

int set_window_widths(void)
{
	int resized = 0;
	NEWWIN *mywin = create_popup(5 + MAX_N_COLUMNS, 35);
	int loop, y = 0;

	for(;;)
	{
		int key;

		werase(mywin -> win);
		escape_print(mywin, 1, 2, "^Manage columns^");
		escape_print(mywin, 2, 2, "^a^dd, ^d^elete, ^e^dit, ^q^uit");
		escape_print(mywin, 3, 2, "width           # windows");

		if (split < 2)
		{
			color_on(mywin, find_colorpair(COLOR_RED, -1, 0));
			escape_print(mywin, 5, 2, "No columns (no vertical split)");
			color_off(mywin, find_colorpair(COLOR_RED, -1, 0));
		}
		else
		{
			for(loop=0; loop<split; loop++)
			{
				int vs = 0;
				int nw = 0;

				if (vertical_split) vs = vertical_split[loop];
				if (n_win_per_col) nw = n_win_per_col[loop];

				if (loop == y) ui_inverse_on(mywin);
				mvwprintw(mywin -> win, 4 + loop, 2, "%2d              %2d", vs, nw);
				if (loop == y) ui_inverse_off(mywin);
			}
		}
		draw_border(mywin);
		mydoupdate();

		key = toupper(wait_for_keypress(HELP_MANAGE_COLS, 0, mywin, 1));
		if (key == 'Q' || key == abort_key)
			break;

		if (key == KEY_UP)
		{
			if (y)
				y--;
			else
				wrong_key();
		}
		else if (key == KEY_DOWN)
		{
			if (y < (split - 1))
				y++;
			else
				wrong_key();
		}
		else if (key == 'A')
		{
			if (split == MAX_N_COLUMNS)
			{
				error_popup("Add column", -1, "Maximum number of columns (%d) reached", MAX_N_COLUMNS);
			}
			else
			{
				int is_new = 0;

				if (split == 0)
				{
					is_new = 1;
					split = 2;
				}
				else
					split++;

				if (!vertical_split) is_new = 1;

				vertical_split = (int *)myrealloc(vertical_split, sizeof(int) * split, __FILE__, __PRETTY_FUNCTION__, __LINE__);
				n_win_per_col  = (int *)myrealloc(n_win_per_col,  sizeof(int) * split, __FILE__, __PRETTY_FUNCTION__, __LINE__);

				if (is_new)
				{
					memset(vertical_split, 0x00, sizeof(int) * split);
					memset(n_win_per_col , 0x00, sizeof(int) * split);
				}
				else
				{
					vertical_split[split - 1] = 0;
					n_win_per_col[split - 1] = 0;
				}

				resized = 1;
			}
		}
		else if (key == 'D')
		{
			if (split > 0)
			{
				resized = 1;

				if (split == 2)
				{
					split = 0;
					myfree(vertical_split);
					vertical_split = NULL;
					myfree(n_win_per_col);
					n_win_per_col = NULL;
				}
				else
				{
					int n_to_move = (split - y) - 1;
					if (vertical_split) memmove(&vertical_split[y], &vertical_split[y+1], sizeof(int) * n_to_move);
					if (n_win_per_col ) memmove(&n_win_per_col [y], &n_win_per_col [y+1], sizeof(int) * n_to_move);
					split--;
				}

				if (y >= split && split > 0)
					y--;

				resized = 1;
			}
			else
				wrong_key();
		}
		else if (key == 'E')
		{
			if (split > 0)
			{
				char oldval[64], *result;
				NEWWIN *editwin = create_popup(6, 33);

				escape_print(editwin, 1, 2, "Column width:");

				if (!vertical_split)
				{
					vertical_split = mymalloc(split * sizeof(int), __FILE__, __PRETTY_FUNCTION__, __LINE__);
					memset(vertical_split, 0x00, split * sizeof(int));
				}
				if (!n_win_per_col)
				{
					n_win_per_col = mymalloc(split * sizeof(int), __FILE__, __PRETTY_FUNCTION__, __LINE__);
					memset(n_win_per_col, 0x00, split * sizeof(int));
				}

				snprintf(oldval, sizeof(oldval), "%d", vertical_split[y]);
				result = edit_string(editwin, 2, 2, 20, 3, 1, oldval, HELP_COLUMN_WIDTH, -1, NULL, NULL);
				if (result)
				{
					int vs = atoi(result);
					myfree(result);

					escape_print(editwin, 3, 2, "Number of windows per column:");
					snprintf(oldval, sizeof(oldval), "%d", n_win_per_col[y]);
					result = edit_string(editwin, 4, 2, 20, 3, 1, oldval, HELP_N_WIN_PER_COL, -1, NULL, NULL);
					if (result)
					{
						vertical_split[y] = vs;
						n_win_per_col [y] = atoi(result);

						myfree(result);

						resized = 1;
					}
				}

				delete_popup(editwin);
			}
			else
				wrong_key();
		}
		else
			wrong_key();
	}

	delete_popup(mywin);

	return resized;
}

int set_window_sizes(void)
{
	int resized = 0;
	NEWWIN *mywin = create_popup(8, 35);

	for(;;)
	{
		int key;

		werase(mywin -> win);
		escape_print(mywin, 1, 2, "^m^ manage columns");
		escape_print(mywin, 2, 2, "^h^ set height of a window");
		escape_print(mywin, 3, 2, "^q^ quit this menu");
		draw_border(mywin);
		mydoupdate();

		key = toupper(wait_for_keypress(HELP_SET_WINDOWSIZES, 0, mywin, 0));
		if (key == 'Q' || key == abort_key)
			break;

		if (key == 'M')
		{
			resized |= set_window_widths();
		}
		else if (key == 'H')
		{
			if (nfd > 1)
			{
				int window = select_window(HELP_SET_WINDOW_HEIGHT_SELECT_WINDOW, NULL);
				if (window != -1)
				{
					char *str;
					char oldval[5] = { 0 };

					if (pi[window].win_height > -1)
						snprintf(oldval, sizeof(oldval), "%d", pi[window].win_height);
					else if (pi[window].data)
						snprintf(oldval, sizeof(oldval), "%d", pi[window].data -> height);

					mvwprintw(mywin -> win, 4, 2, "Enter number of lines:");
					mvwprintw(mywin -> win, 5, 2, "(0 = automatic)");
					mydoupdate();
					str = edit_string(mywin, 6, 2, 20, 3, 1, oldval, HELP_SET_WINDOW_HEIGHT, -1, NULL, NULL);
					if (str)
					{
						int dummy = atoi(str);

						myfree(str);

						if (dummy == 0)
							dummy = -1;

						if (dummy == -1 || dummy >= 2)
						{
							pi[window].win_height = dummy;
							resized = 1;
							break;
						}

						error_popup("Set window size", HELP_SET_WINDOW_HEIGHT, "Invalid height (%d) entered.", dummy);
					}
				}
			}
			else
			{
				mvwprintw(mywin -> win, 5, 2, "You need at least 2 windows.");
			}

			mydoupdate();
		}
		else
		{
			wrong_key();
		}
	}

	delete_popup(mywin);

	return resized;
}

void terminal_mode(void)
{
	proginfo *cur = NULL;
	int f_index = -1;

	if (nfd > 0)
	{
		NEWWIN *mywin = create_popup(8 + nfd, 40);
		win_header(mywin, "Enter terminal mode");
		mydoupdate();

		f_index = select_window(HELP_TERMINAL_MODE_SELECT_WINDOW, NULL);

		delete_popup(mywin);
	}

	if (f_index != -1)
	{
		cur = &pi[f_index];

		if (cur -> next)
			cur = select_subwindow(f_index, HELP_TOGGLE_COLORS_SELECT_SUBWINDOW, "Enter terminalmode: select subwindow");
	}

	if (cur)
	{
		if (cur -> wt != WT_COMMAND)
		{
			error_popup("Enter terminal mode", -1, "Terminal mode is only possible for commands.");
		}
		else
		{
			terminal_index = cur;
			terminal_main_index = f_index;

			prev_term_char = -1;

			redraw_statuslines();
		}
	}
}

void wipe_window(void)
{
	NEWWIN *mywin = create_popup(5, 35);

	escape_print(mywin, 1, 2, "^Clear window^");
	escape_print(mywin, 3, 2, "Press 0...9");
	mydoupdate();

	for(;;)
	{
		int c = wait_for_keypress(HELP_WIPE_WINDOW, 0, mywin, 0);

		if (c == abort_key || toupper(c) == 'Q')
			break;

		if (c < '0' || c > '9')
		{
			wrong_key();
			continue;
		}

		c -= '0';

		if (c >= nfd)
		{
			wrong_key();
			continue;
		}

		if (pi[c].hidden)
		{
			wrong_key();
			continue;
		}

		werase(pi[c].data -> win);
		mydoupdate();

		break;
	}

	delete_popup(mywin);
}

void wipe_all_windows(void)
{
	int loop;

	for(loop=0; loop<nfd; loop++)
	{
		if (pi[loop].data)
			werase(pi[loop].data -> win);
	}
}

void add_markerline(int index, proginfo *cur, proginfo *type, char *text)
{
	char buffer[TIMESTAMP_EXTEND_BUFFER] = { 0 };
	char *ml;
	char *curfname = cur ? cur -> filename : "";
	char *curtext = USE_IF_SET(text, "");
	double now = get_ts();
	int str_len;

	if (timestamp_in_markerline)
		get_now_ts(statusline_ts_format, buffer, sizeof(buffer));

	str_len = strlen(curfname) + 1 + strlen(buffer) + 1 + strlen(curtext) + 1 + 1;

	ml = (char *)mymalloc(str_len, __FILE__, __PRETTY_FUNCTION__, __LINE__);

	snprintf(ml, str_len, "%s%s%s%s%s", curtext, curtext[0]?" ":"", curfname, (curtext[0] || curfname[0])?" ":"", buffer);

	do_print(index, type, ml, NULL, -1, now);
	do_buffer(index, type, ml, 1, now);

	myfree(ml);

	update_panels();
}

void do_terminal(char c)
{
	if (prev_term_char == 1)	/* ^A */
	{
		if (c == 'd')
		{
			terminal_main_index = -1;
			terminal_index = NULL;
			redraw_statuslines();
			mydoupdate();
		}
		else
		{
			WRITE(terminal_index -> wfd, &prev_term_char, 1, "do_terminal");
			WRITE(terminal_index -> wfd, &c, 1, "do_terminal");
		}
	}
	else
	{
		if (c != 1)	/* ^A */
			WRITE(terminal_index -> wfd, &c, 1, "do_terminal");
	}

	prev_term_char = c;
}

void set_linewrap(void)
{
	NEWWIN *mywin = create_popup(6, 64);
	int f_index = 0;
	proginfo *cur = NULL;

	escape_print(mywin, 1, 2, "^Set linewrap^");
	escape_print(mywin, 3, 2, "^l^eft/^a^ll/^r^ight/^s^yslog/^S^yslog: no procname/^o^ffset/^w^ordwrap");
	mydoupdate();

	/* only popup selectionbox when more then one window */
	if (nfd > 1)
		f_index = select_window(HELP_DELETE_SELECT_WINDOW, "Select window");

	if (f_index != -1)
	{
		if (pi[f_index].next)
			cur = select_subwindow(f_index, HELP_DELETE_SELECT_SUBWINDOW, "Select subwindow");
		else
			cur = &pi[f_index];
	}

	if (cur)
	{
		for(;;)
		{
			int c = wait_for_keypress(HELP_SET_LINEWRAP, 0, mywin, 0);

			if (c == abort_key || toupper(c) == 'Q')
				break;

			if (c == 'l' || c == 'a' || c == 'r' || c == 's' || c == 'S' || c == 'o' || c == 'w')
			{
				if (c == 'o')
				{
					char offset[5], *new_offset;

					snprintf(offset, sizeof(offset), "%d", cur -> line_wrap_offset);

					mvwprintw(mywin -> win, 4, 2, "Offset:");
					new_offset = edit_string(mywin, 4, 10, 4, 4, 1, offset, -1, -1, NULL, NULL);

					if (!new_offset)
						break;

					cur -> line_wrap_offset = atoi(new_offset);
					myfree(new_offset);
				}

				cur -> line_wrap = c;

				break;
			}

			wrong_key();
		}
	}

	delete_popup(mywin);
}

void horizontal_scroll(int direction)
{
	proginfo *cur = &pi[0];

	do
	{
		if (cur->line_wrap == 'l')
		{
			if (direction == KEY_RIGHT)
			{
				cur->line_wrap_offset++;
			}
			else
			{
				if (cur->line_wrap_offset > 0)
					cur->line_wrap_offset--;
			}
		}

		cur= cur->next;
	} while(cur);
}

void regexp_error_popup(int rc, regex_t *pre)
{
	char popup_buffer[4096];
	char *error = convert_regexp_error(rc, pre);

	if (error)
	{
		int len = strlen(popup_buffer);

		if (strlen(error) > 58)
		{
			memcpy(&popup_buffer[len], error, 58);
			len += 58;
			popup_buffer[len++] = '\n';
			strncat(popup_buffer, &error[58], max(1, sizeof(popup_buffer) - len));
			popup_buffer[sizeof(popup_buffer) - 1] = 0x00;
		}
		else
		{
			strncat(popup_buffer, error, sizeof(popup_buffer)-strlen(popup_buffer)-1);
		}

		myfree(error);
	}

	error_popup("regexp_error_popup: failed to compile regular expression!", HELP_COMPILE_REGEXP_FAILED, popup_buffer);
}

void draw_gui_window_header(proginfo *last_changed_window)
{
	char *out = NULL;
	int out_size = 0;
	int out_fill_size = 0;
	int pos = 0;
	int str_len;
	static char loadstr[16];
	struct utsname uinfo;

	if (!set_title) return;

	str_len = strlen(set_title);

	for(;pos < str_len;)
	{
		if (set_title[pos] == '%')
		{
			char *repl_str = NULL;

			switch(set_title[++pos])
			{
				case 'f':	/* changed file */
					repl_str = last_changed_window?last_changed_window -> filename:"";
					break;

				case 'h':	/* hostname */
					if (uname(&uinfo) == -1)
						error_exit(__FILE__, __PRETTY_FUNCTION__, __LINE__, "uname() failed\n");

					repl_str = uinfo.nodename;
					break;

				case 'l':	/* current load */
					{
						double v1, v2, v3;

						get_load_values(&v1, &v2, &v3);
						snprintf(loadstr, sizeof(loadstr), "%f", v1);
						repl_str = loadstr;
					}
					break;

				case 'm':
					repl_str = mail?"New mail!":"";
					break;

				case 'u':	/* username */
					repl_str = getusername();
					break;

				case 't':	/* atime */
					if (last_changed_window)
					{
						char *dummy;
						time_t lastevent = last_changed_window -> statistics.lastevent;
						repl_str = ctime(&lastevent);
						dummy = strchr(repl_str, '\n');
						if (dummy) *dummy = 0x00;
					}
					else
					{
						repl_str = "";
					}
					break;

				case '%':	/* % */
					repl_str = "%";
					break;
			}

			if (repl_str)
			{
				int repl_str_len = strlen(repl_str); /* +1: 0x00 */
				int req_size = out_fill_size + repl_str_len;

				if (req_size > 0)
				{
					grow_mem_if_needed(&out, &out_size, req_size);
					memcpy(&out[out_fill_size], repl_str, repl_str_len);
					out_fill_size += repl_str_len;
				}
			}
		}
		else
		{
			grow_mem_if_needed(&out, &out_size, out_fill_size + 1);
			out[out_fill_size++] = set_title[pos];
		}

		pos++;
	}

	/* terminate string */
	grow_mem_if_needed(&out, &out_size, out_fill_size + 1);
	out[out_fill_size] = 0x00;

	gui_window_header(out);

	myfree(out);
}

void send_signal_failed(proginfo *cur)
{
	NEWWIN *mywin = create_popup(6, 60);

	color_on(mywin, find_colorpair(COLOR_RED, -1, 0));
	mvwprintw(mywin -> win, 2, 2, "Error sending signal to %s", cur -> filename);
	mvwprintw(mywin -> win, 3, 2, "%s", strerror(errno));
	color_off(mywin, find_colorpair(COLOR_RED, -1, 0));

	(void)wait_for_keypress(HELP_SEND_SIGNAL_FAILED, 0, mywin, 0);

	delete_popup(mywin);
}

int send_signal_select_signal(void)
{
	int sig = -1;
	NEWWIN *mywin = create_popup(22, 40);
	int cursor_index = 0;

	for(;;)
	{
		int loop, c;

		werase(mywin -> win);
		win_header(mywin, "Select signal to send");
		mvwprintw(mywin -> win, 20, 2, "Press ^g to abort");

		for(loop=1; loop<=31; loop++)
		{
			if (loop == (cursor_index + 1)) ui_inverse_on(mywin);

			if (loop > 16)
				mvwprintw(mywin -> win, 2 + loop - 16, 18, "%s", sigs[loop]);
			else
				mvwprintw(mywin -> win, 2 + loop     ,  2, "%s", sigs[loop]);

			if (loop == (cursor_index + 1)) ui_inverse_off(mywin);
		}

		draw_border(mywin);
		mydoupdate();

		c = toupper(wait_for_keypress(HELP_SELECT_SIGNAL, 0, mywin, 1));

		if (c == KEY_UP)
		{
			if (cursor_index)
				cursor_index--;
			else
				wrong_key();
		}
		else if (c == KEY_DOWN)
		{
			if (cursor_index < 30)
				cursor_index++;
			else
				wrong_key();
		}
		else if (c == KEY_LEFT)
		{
			if (cursor_index > 16)
				cursor_index -= 16;
			else
				wrong_key();
		}
		else if (c == KEY_RIGHT)
		{
			if (cursor_index < 16)
			{
				cursor_index += 16;
				if (cursor_index > 30)
					cursor_index = 30;
			}
			else
				wrong_key();
		}
		else if (c == 'Q' || c == 'X' || c == abort_key)
		{
			sig = -1;
			break;
		}
		else if (c == ' ' || c == 13)
		{
			sig = cursor_index + 1;
			break;
		}
		else
			wrong_key();
	}

	delete_popup(mywin);

	return sig;
}

void send_signal(void)
{
	int f_index = 0;
	proginfo *cur = NULL;

	if (nfd > 1)
		f_index = select_window(HELP_SEND_SIGNAL_SELECT_WINDOW, "Select window to send signal to");

	if (f_index != -1)
	{
		char all = 1;

		if (pi[f_index].next)
		{
			NEWWIN *mywin = create_popup(13, 24);

			win_header(mywin, "Send signal to window");
			escape_print(mywin, 3, 2, "To all? (^y^/^n^)");
			mydoupdate();

			all = ask_yes_no(HELP_SEND_SIGNAL_WINDOW_SEND_TO_ALL_SUBWIN, mywin);

			delete_popup(mywin);
		}

		if (all == 1)   /* pressed 'Y' */
		{
			int sig = send_signal_select_signal();
			cur = &pi[f_index];

			while(cur && sig != -1)
			{
				if (kill(cur -> pid, sig) == -1)
				{
					send_signal_failed(cur);
					break;
				}

				cur = cur -> next;
			}
		}
		else if (all == 0) /* pressed 'N' */
		{
			proginfo *cur = select_subwindow(f_index, HELP_SEND_SIGNAL_SELECT_SUBWINDOW, "Select subwindow");

			if (cur)
			{
				int sig = send_signal_select_signal();

				if (sig != -1 && kill(cur -> pid, sig) == -1)
					send_signal_failed(cur);
			}
		}
	}
}

void screendump_do(WINDOW *win)
{
	char *filename;
	NEWWIN *mywin = create_popup(7, 30);

	win_header(mywin, "Screendump");
	mvwprintw(mywin -> win, 3, 2, "Select file to write to:");

	filename = edit_string(mywin, 4, 2, 38, find_path_max(), 0, "screendump.txt", HELP_SCREENDUMP_SELECT_FILE, -1, &cmdfile_h, NULL);
	if (filename)
	{
		FILE *fh = fopen(filename, "wb");
		int maxy = 0, maxx = 0;
		int y, x;

		getmaxyx(win, maxy, maxx);

		for(y=0; y<maxy; y++)
		{
			int curmaxx;

			for(curmaxx=maxx-1; curmaxx>=0; curmaxx--)
			{
				if ((mvwinch(win, y, curmaxx) & A_CHARTEXT) != 32)
					break;
			}
			curmaxx++;
			for(x=0; x<curmaxx; x++)
			{
				fprintf(fh, "%c", (int)(mvwinch(win, y, x) & A_CHARTEXT));
			}

			fprintf(fh, "\n");
		}

		fclose(fh);

		myfree(filename);
	}

	delete_popup(mywin);
}

void screendump(void)
{
	int f_index = select_window(HELP_SCREENDUMP_SELECT_WIN, "Select window to dump to file");
	if (f_index != -1)
	{
		screendump_do(pi[f_index].data -> win);
	}
}

void truncate_file(void)
{
	int f_index = 0;
	proginfo *cur = NULL;

	if (nfd > 1)
		f_index = select_window(HELP_TRUNCATE_FILE_SELECT_WINDOW, "Select window to truncate");

	if (f_index != -1)
	{
		if (pi[f_index].next)
			cur = select_subwindow(f_index, HELP_TRUNCATE_FILE_SELECT_SUBWINDOW, "Select subwindow");
		else
			cur = &pi[f_index];

		if (cur)
		{
			if (cur -> wt == WT_FILE)
			{
				NEWWIN *mywin = create_popup(8, 50);

				color_on(mywin, find_colorpair(COLOR_RED, -1, 0));
				win_header(mywin, "Truncate file");
				mvwprintw(mywin -> win, 3, 2, "Truncate file %s", shorten_filename(cur -> filename, 31));
				mvwprintw(mywin -> win, 4, 2, "Are you sure? (y/n)");
				color_off(mywin, find_colorpair(COLOR_RED, -1, 0));
				mydoupdate();

				if (wait_for_keypress(HELP_TRUNCATE_AREYOUSURE, 0, mywin, 0) == 'y')
				{
					if (truncate(cur -> filename, 0) == -1)
						error_popup("Truncate failed", HELP_TRUNCATE_FAILED, "Error: %s", strerror(errno));
					else
					{
						color_on(mywin, find_colorpair(COLOR_YELLOW, -1, 0));
						escape_print(mywin, 5, 2, "_File truncated. Press any key._");
						color_off(mywin, find_colorpair(COLOR_YELLOW, -1, 0));
						mydoupdate();
						(void)wait_for_keypress(-1, 0, mywin, 0);
					}
				}

				delete_popup(mywin);
			}
			else
				error_popup("Truncate failed", HELP_TRUNCATE_ONLY_LOGFILES, "Only logFILES can be truncated.");
		}
	}
}

void draw_marker_line(NEWWIN *win, char *string, proginfo *marker_type)
{
	int mx = getmaxx(win -> win);
	int len = strlen(string);
	int left_dot = (mx / 2) - (len / 2);
	int loop;
	myattr_t attrs = { -1, -1 };
	char dot = '-';

	if (marker_type == MARKER_REGULAR)
	{
		attrs = markerline_attrs;
		dot = markerline_char;
	}
	else if (marker_type == MARKER_CHANGE)
	{
		attrs = changeline_attrs;
		dot = changeline_char;
	}
	else if (marker_type == MARKER_IDLE)
	{
		attrs = idleline_attrs;
		dot = idleline_char;
	}
	else if (marker_type == MARKER_MSG)
	{
		attrs = msgline_attrs;
		dot = msgline_char;
	}

	myattr_on(win, attrs);
	for(loop=0; loop<left_dot; loop++)
		waddch(win -> win, dot);
	if (len < mx)
		wprintw(win -> win, "%s", string);
	else
		wprintw(win -> win, "%s", shorten_filename(string, mx));
	for(loop=0; loop<mx - (left_dot + len); loop++)
		waddch(win -> win, dot);
	myattr_off(win, attrs);
}

void search_in_all_windows(void)
{
	char *find;
	NEWWIN *mywin = create_popup(8, 44);
	mybool_t case_insensitive = re_case_insensitive;

	win_header(mywin, "Find");

	mvwprintw(mywin -> win, 3, 2, "^u empty line, ^g abort");

	find = edit_string(mywin, 5, 2, 40, 80, 0, reuse_searchstring?global_find:NULL, HELP_SEARCH_IN_ALL_WINDOWS, -1, &search_h, &case_insensitive);
	delete_popup(mywin);

	myfree(global_find);
	global_find = find;

	if (find)
		merged_scrollback_with_search(find, case_insensitive);
}

void highlight_in_all_windows(void)
{
	char *find;
	NEWWIN *mywin = create_popup(5, 44);
	mybool_t case_insensitive = re_case_insensitive;

	win_header(mywin, "Global highlight");
	find = edit_string(mywin, 3, 2, 40, 80, 0, reuse_searchstring?global_highlight_str:NULL, HELP_HIGHLIGHT_IN_ALL_WINDOWS, -1, &search_h, &case_insensitive);
	delete_popup(mywin);

	if (global_highlight_str)
	{
		regfree(&global_highlight_re);
		myfree(global_highlight_str);
		global_highlight_str = NULL;
	}

	if (find)
	{
		int rc;

		if ((rc = regcomp(&global_highlight_re, find, REG_EXTENDED | (case_insensitive == MY_TRUE?REG_ICASE:0))))
		{
			regexp_error_popup(rc, &global_highlight_re);
			myfree(find);
		}
		else
		{
			global_highlight_str = find;
		}
	}
}

void toggle_regexp_case_insensitive(void)
{
	NEWWIN *mywin = create_popup(6, 40);

	re_case_insensitive = !re_case_insensitive;

	mvwprintw(mywin -> win, 2, 2, "Default searche case now is %s.", re_case_insensitive == MY_TRUE?"insensitive":"sensitive");
	escape_print(mywin, 4, 2, "_Press any key to exit this screen_");

	mydoupdate();
	(void)wait_for_keypress(-1, 0, mywin, 0);

	delete_popup(mywin);
}

void inverse_on(NEWWIN *win)
{
	wattron(win -> win, inverse_attrs);
}

void inverse_off(NEWWIN *win)
{
	wattroff(win -> win, inverse_attrs);
}

void select_conversionschemes(void)
{
	int f_index = 0;
	proginfo *cur = NULL;

	if (nfd > 1)
		f_index = select_window(HELP_TRUNCATE_FILE_SELECT_WINDOW, "Select window");

	if (f_index != -1)
	{
		if (pi[f_index].next)
			cur = select_subwindow(f_index, HELP_TRUNCATE_FILE_SELECT_SUBWINDOW, "Select subwindow");
		else
			cur = &pi[f_index];

		if (cur)
		{
			(void)select_schemes(conversions, n_conversions, SCHEME_CONVERSION, &cur -> conversions);
		}
	}
}

void toggle_subwindow_nr(void)
{
	show_subwindow_id = 1 - show_subwindow_id;
}

void clear_a_buffer(void)
{
	int f_index = 0;

	if (nfd > 1)
		f_index = select_window(HELP_CLEAR_BUFFER, "Select window");

	if (f_index != -1)
	{
		delete_be_in_buffer(&lb[f_index]);
		werase(pi[f_index].data -> win);
		mydoupdate();
	}
}

void clear_all_buffers(void)
{
	int loop;

	for(loop=0; loop<nfd; loop++)
	{
		delete_be_in_buffer(&lb[loop]);
		werase(pi[loop].data -> win);
		mydoupdate();
	}
}

void ask_case_insensitive(mybool_t *pcase_insensitive)
{
	NEWWIN *mywin = create_popup(6, 40);

	mvwprintw(mywin -> win, 2, 2, "Search case insensitive?");
	mvwprintw(mywin -> win, 3, 12, "(Y/N)");

	mydoupdate();
	for(;;)
	{
		int c = toupper(wait_for_keypress(-1, 0, mywin, 0));

		if (c == 'Q' || c == abort_key)
			break;

		if (c == 'Y')
		{
			*pcase_insensitive = MY_TRUE;
			break;
		}
		else if (c == 'N')
		{
			*pcase_insensitive = MY_FALSE;
			break;
		}

		wrong_key();
	}

	delete_popup(mywin);
}
