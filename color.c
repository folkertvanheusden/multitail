#define _LARGEFILE64_SOURCE     /* required for GLIBC to enable stat64 and friends */
#include "doassert.h"
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "error.h"
#include "mem.h"
#include "term.h"
#include "exec.h"
#include "utils.h"
#include "globals.h"
#include "ui.h"

color_offset_in_line *realloc_color_offset_in_line(color_offset_in_line *oldp, int n_entries)
{
	assert(n_entries > 0);

	return (color_offset_in_line *)myrealloc(oldp, n_entries * sizeof(color_offset_in_line));
}

void add_color_scheme(int_array_t *schemes, int cur_scheme)
{
	assert(cur_scheme >= 0);

	add_to_iat(schemes, cur_scheme);
}

int gen_color(char *start, char *end)
{
	char *loop;
	int chk = 0;

	assert(end != NULL);

	for(loop=start; loop<end; loop++)
	{
		chk ^= *loop;
	}

	return abs(chk) % cp.n_def;
}

myattr_t gen_syslog_progname_color(char *string)
{
	myattr_t cdev = { -1, -1 };

	if (use_colors)
	{
		if (strlen(string) >= 16)
		{
			string = strchr(&string[16], ' ');
		}

		if (string)
		{
			char *end1, *end2, *end3, *end = NULL;

			while(isspace(*string)) string++;

			end1 = strchr(string, '[');
			end2 = strchr(string, ' ');
			end3 = strchr(string, ':');

			end = end1;
			if ((end2 && end2 < end) || (end == NULL))
				end = end2;
			if ((end3 && end3 < end) || (end == NULL))
				end = end3;

			if (end)
				cdev.colorpair_index = gen_color(string, end);
			else
				cdev.colorpair_index = gen_color(string, &string[strlen(string)]);
		}
	}

	return cdev;
}

myattr_t gen_color_from_field(char *string, char *field_del, int field_nr)
{
	myattr_t cdev = { -1, -1 };

	assert(field_nr >= 0);

	if (use_colors)
	{
		int field_del_len = strlen(field_del), loop;
		char *dummy = NULL;

		for(loop=0; loop<field_nr; loop++)
		{
			string = strstr(string, field_del);
			if (!string)
				break;

			string += field_del_len;
		}

		if (string)
			dummy = strstr(string, field_del);

		if (dummy != NULL && string != NULL)
			cdev.colorpair_index = gen_color(string, dummy);
		else if (string)
			cdev.colorpair_index = gen_color(string, &string[strlen(string)]);
		else
			cdev.colorpair_index = 0;
	}

	return cdev;
}

void color_script(char *line, script *pscript, int *cur_n_cmatches, color_offset_in_line **cmatches, int *n_cmatches)
{
	char finished = 0;
	int line_len = min(SCRIPT_IO_BUFFER_SIZE - 2, strlen(line));
	char *iobuffer = (char *)mymalloc(SCRIPT_IO_BUFFER_SIZE);
	char *line_buf = (char *)mymalloc(line_len + 1 + 1);

	exec_script(pscript);

	memcpy(line_buf, line, line_len);
	line_buf[line_len] = '\n';
	line_buf[line_len + 1] = 0x00;
	WRITE(pscript -> fd_w, line_buf, line_len + 1, pscript -> script);
	myfree(line_buf);

	for(;!finished;)
	{
		char *workpnt = iobuffer;
		int rc = READ(pscript -> fd_r, iobuffer, SCRIPT_IO_BUFFER_SIZE - 1, pscript -> script);

		if (rc <= 1)
			break;
		iobuffer[rc] = 0x00;

		for(;;)
		{
			char *komma;
			char *lf = strchr(workpnt, '\n');
			if (!lf)
				break;

			*lf = 0x00;

			if (workpnt[0] == 0x00)
			{
				finished = 1;
				break;
			}

			if (*cur_n_cmatches == *n_cmatches)
			{
				*cur_n_cmatches = (*cur_n_cmatches + 8) * 2;
				*cmatches = realloc_color_offset_in_line(*cmatches, *cur_n_cmatches);
			}

			komma = strchr(workpnt, ',');
			if (!komma)
			{
				error_popup("Color scheme script", -1, "Malformed line returned by color selection script '%s': first ',' missing.\n", pscript -> script);
				break;
			}

			memset(&(*cmatches)[*n_cmatches], 0x00, sizeof(color_offset_in_line));

			(*cmatches)[*n_cmatches].start = atoi(workpnt);
			(*cmatches)[*n_cmatches].end   = atoi(komma + 1);

			komma = strchr(komma + 1, ',');
			if (!komma)
			{
				error_popup("Color scheme script", -1, "Malformed line returned by color selection script '%s': second ',' missing.\n", pscript -> script);
				break;
			}

			(*cmatches)[*n_cmatches].attrs = parse_attributes(komma + 1);
			(*n_cmatches)++;

			workpnt = lf + 1;
		}
	}

	myfree(iobuffer);
}

void get_colors_from_colorscheme(char *line, int_array_t *color_schemes, color_offset_in_line **cmatches, int *n_cmatches, mybool_t *has_merge_colors)
{
	int cs_index, loop;
	regmatch_t colormatches[MAX_N_RE_MATCHES];
	int cur_n_cmatches = 0;
	int len = strlen(line);

	*n_cmatches = 0;
	*cmatches = NULL;
	*has_merge_colors = MY_FALSE;

	for(loop=0; loop<get_iat_size(color_schemes); loop++)
	{
		if (cschemes[get_iat_element(color_schemes, loop)].color_script.script)
		{
			color_script(line, &cschemes[get_iat_element(color_schemes, loop)].color_script, &cur_n_cmatches, cmatches, n_cmatches);
			continue;
		}

		for(cs_index=0; cs_index<cschemes[get_iat_element(color_schemes, loop)].n; cs_index++)
		{
			csreflag_t flags = cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].flags;
			int offset = 0;
			int re_start, re_end;

			if (flags)
			{
				re_start = 1;
				re_end = MAX_N_RE_MATCHES;
			}
			else
			{
				re_start = 0;
				re_end = 1;
			}

			do
			{
				int match_index;
				int cur_offset = offset;

				/* FIXME: what to do with regexp errors? */
				if (regexec(&cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].regex, &line[offset], MAX_N_RE_MATCHES, colormatches, offset?REG_NOTBOL:0) != 0)
					break;

				for(match_index=re_start; match_index<re_end; match_index++)
				{
					int this_start_offset, this_end_offset;

					this_start_offset = colormatches[match_index].rm_so + cur_offset;
					this_end_offset   = colormatches[match_index].rm_eo + cur_offset;

					offset = max(offset + 1, this_end_offset);

					if (colormatches[match_index].rm_so == -1)
						break;

					if (flags == CSREFLAG_CMP_VAL_LESS || flags == CSREFLAG_CMP_VAL_BIGGER || flags == CSREFLAG_CMP_VAL_EQUAL)
					{
						int val_size = this_end_offset - this_start_offset;
						char *valstr = (char *)mymalloc(val_size + 1);
						double value = 0.0;
						char value_match = 0;

						if (val_size > 0)
							memcpy(valstr, &line[this_start_offset], val_size);
						valstr[val_size] = 0x00;
						value = atof(valstr);
						myfree(valstr);

						if (flags == CSREFLAG_CMP_VAL_LESS && value < cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].cmp_value)
							value_match = 1;
						else if (flags == CSREFLAG_CMP_VAL_BIGGER && value > cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].cmp_value)
							value_match = 1;
						else if (flags == CSREFLAG_CMP_VAL_EQUAL && value == cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].cmp_value)
							value_match = 1;

						if (!value_match)
							continue;
					}

					if (cur_n_cmatches == *n_cmatches)
					{
						cur_n_cmatches = (cur_n_cmatches + 8) * 2;
						*cmatches = realloc_color_offset_in_line(*cmatches, cur_n_cmatches);
					}

					memset(&(*cmatches)[*n_cmatches], 0x00, sizeof(color_offset_in_line));

					(*cmatches)[*n_cmatches].start = this_start_offset;
					(*cmatches)[*n_cmatches].end   = this_end_offset;

					if (cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].cdef.ac_index == 0)
						(*cmatches)[*n_cmatches].attrs  = cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].cdef.attrs1;
					else
						(*cmatches)[*n_cmatches].attrs  = cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].cdef.attrs2;

					if (cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].merge_color == MY_TRUE)
						*has_merge_colors = MY_TRUE;

					(*cmatches)[*n_cmatches].merge_color = cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].merge_color;

					(*n_cmatches)++;
				} /* iterate all substringmatches or just the first which is the globl one */

				/* in case we have alternating colors, alternate them */
				if (cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].cdef.use_alternating_colors == MY_TRUE)
				{
					cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].cdef.ac_index = 1 - cschemes[get_iat_element(color_schemes, loop)].pentries[cs_index].cdef.ac_index;
				}
			}
			while(offset < len); /* do the whole line */
		} /* go through all lines of the current colorscheme */
	} /* go through all colorschemes for this (sub-)window */
}

myattr_t choose_color(char *string, proginfo *cur, color_offset_in_line **cmatches, int *n_cmatches, mybool_t *has_merge_colors, char **new_string)
{
	myattr_t cdev = { -1, -1 };

	*new_string = NULL;

	if (cur -> cdef.term_emul != TERM_IGNORE)
	{
		*new_string = emulate_terminal(string, cmatches, n_cmatches);
		return find_attr(COLOR_WHITE, -1, -1);
	}

	/* find color */
	switch(cur -> cdef.colorize)
	{
		case 'i':
			cdev = cur -> cdef.attributes;
			break;

		case 'a':
			/* alternating colors */
			if (string[0] != 0x00)
			{
				if (cur -> cdef.alt_col)
					cdev = cur -> cdef.alt_col_cdev1;
				else
					cdev = cur -> cdef.alt_col_cdev2;

				cur -> cdef.alt_col = !cur -> cdef.alt_col;
			}
			break;

		case 's':
			/* use the program name for the color */
			cdev = gen_syslog_progname_color(string);
			break;

		case 'f':
			/* user selected field for coloring */
			cdev = gen_color_from_field(string, cur -> cdef.field_del, cur -> cdef.field_nr);
			break;

		case 'm':
			/* complete string */
			if (use_colors)
				cdev.colorpair_index = gen_color(string, &string[strlen(string)]);
			break;

		case 'S':
			/* colorscheme or colorscripts */
			get_colors_from_colorscheme(string, &cur -> cdef.color_schemes, cmatches, n_cmatches, has_merge_colors);
			break;

		default:
			assert(0);
			break;
	}

	if (cdev.colorpair_index == -1)
		cdev.colorpair_index = 0;
	if (cdev.attrs == -1)
		cdev.attrs = A_NORMAL;

	if (cur -> cdef.syslog_noreverse)
		cdev.colorpair_index %= DEFAULT_COLORPAIRS;

	return cdev;
}

int find_colorscheme(char *name)
{
	int loop;

	for(loop=0; loop<n_cschemes; loop++)
	{
		if (strcasecmp(cschemes[loop].name, name) == 0)
			return loop;
	}

	return -1;
}

void init_colors(void)
{
	/* add standard colors */
	cp.size = min(256, COLOR_PAIRS);
	if (cp.size)
	{
		cp.fg_color = (int *)mymalloc(cp.size * sizeof(int));
		cp.bg_color = (int *)mymalloc(cp.size * sizeof(int));
	}

	(void)find_or_init_colorpair(-1, -1, 1);		/* colorpair 0 is always default, cannot be changed */
	(void)find_or_init_colorpair(COLOR_RED, default_bg_color, 1);
	(void)find_or_init_colorpair(COLOR_GREEN, default_bg_color, 1);
	(void)find_or_init_colorpair(COLOR_YELLOW, default_bg_color, 1);
	(void)find_or_init_colorpair(COLOR_BLUE, default_bg_color, 1);
	(void)find_or_init_colorpair(COLOR_MAGENTA, default_bg_color, 1);
	(void)find_or_init_colorpair(COLOR_CYAN, default_bg_color, 1);
	(void)find_or_init_colorpair(COLOR_WHITE, default_bg_color, 1);
	/*	(void)find_or_init_colorpair(COLOR_BLACK, -1, 1); */

	if (use_colors && cp.n_def != DEFAULT_COLORPAIRS)
		error_exit(FALSE, FALSE, "Unexpected number of colors: %d (%d)\n", cp.n_def, DEFAULT_COLORPAIRS);
}
