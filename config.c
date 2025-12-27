#include "doassert.h"
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "mem.h"
#include "error.h"
#include "utils.h"
#include "color.h"
#include "term.h"
#include "exec.h"
#include "globals.h"
#include "config.h"
#include "clipboard.h"

/* "local global" */
int cur_colorscheme_nr = -1;
int cur_filterscheme_nr = -1;
int cur_editscheme_nr = -1;

mybool_t config_yes_no(char *what)
{
	if (what[0] == '1' || strcasecmp(what, "yes") == 0 || strcasecmp(what, "y") == 0 || strcasecmp(what, "on") == 0)
	{
		return MY_TRUE;
	}

	return MY_FALSE;
}

long long int kb_str_to_value(char *field, char *str)
{
	char *mult;
	long long int bytes = atoll(str);
	if (bytes < -1)
		error_exit(FALSE, FALSE, "%s: value cannot be < -1\n", field);

	mult = &str[strlen(str) - 2];
	if (strcasecmp(mult, "kb") == 0)
		bytes *= 1024;
	else if (strcasecmp(mult, "mb") == 0)
		bytes *= 1024 * 1024;
	else if (strcasecmp(mult, "gb") == 0)
		bytes *= 1024 * 1024 * 1024;

	return bytes;
}

void config_error_exit(int linenr, char *format, ...)
{
        va_list ap;

	fprintf(stderr, version_str, VERSION);
	fprintf(stderr, "\n\n");

	if (linenr != -1)
		fprintf(stderr, "Error while processing configuration file at line %d:\n", linenr);

	va_start(ap, format);
	(void)vfprintf(stderr, format, ap);
	va_end(ap);

	exit(EXIT_FAILURE);
}

void add_cs_re(int linenr, char *incmd, char *par)
{
	if (use_colors)
	{
		char *re = NULL, *val = NULL;
		char *cmd = &incmd[5];
		char *colon;

		if (strncmp(cmd, "_val", 4) == 0)
		{
			val = find_next_par(par);
			if (!val)
				config_error_exit(linenr, "cs_re_val...-entry malformed: value missing.\n");

			re = find_next_par(val);
		}
		else
			re = find_next_par(par);

		if (re == NULL)
			config_error_exit(linenr, "cs_re-entry malformed: color or regular expression missing.\n");

		if (cur_colorscheme_nr == -1)
			config_error_exit(linenr, "For cs_re one needs to define a color scheme name first.\n");

		/* find colorscheme */
		if (cschemes[cur_colorscheme_nr].color_script.script)
			config_error_exit(linenr, "One cannot let a color script have the same name has a color scheme.");

		/* grow/create list */
		cschemes[cur_colorscheme_nr].pentries = (color_scheme_entry *)myrealloc(cschemes[cur_colorscheme_nr].pentries, (cschemes[cur_colorscheme_nr].n + 1) * sizeof(color_scheme_entry));

		/* add to list */
		if (cmd[0] == 0x00)
			cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].flags = 0;
		else if (strcmp(cmd, "_s") == 0)
			cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].flags = CSREFLAG_SUB;
		else if (strcmp(cmd, "_val_less") == 0)
			cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].flags = CSREFLAG_CMP_VAL_LESS;
		else if (strcmp(cmd, "_val_bigger") == 0)
			cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].flags = CSREFLAG_CMP_VAL_BIGGER;
		else if (strcmp(cmd, "_val_equal") == 0)
			cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].flags = CSREFLAG_CMP_VAL_EQUAL;

		/* sanity check */
		if (cmd[0] != 0x00 && strchr(re, '(') == NULL)
			config_error_exit(linenr, "%s is missing substring selections! ('(' and ')')\n", cmd);

		if (val) cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].cmp_value = atof(val);
		cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].merge_color = incmd[0] == 'm' ? MY_TRUE : MY_FALSE;

		cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].cdef.ac_index = 0;
		colon = strchr(par, '|');
		if (colon)
		{
			*colon = 0x00;
			cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].cdef.use_alternating_colors = MY_TRUE;
			cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].cdef.attrs2 = parse_attributes(colon + 1);
		}
		else
		{
			cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].cdef.use_alternating_colors = MY_FALSE;
		}
		cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].cdef.attrs1 = parse_attributes(par);

		/* compile regular expression */
		compile_re(&cschemes[cur_colorscheme_nr].pentries[cschemes[cur_colorscheme_nr].n].regex, re);

		cschemes[cur_colorscheme_nr].n++;
	}
}

void add_colorscheme(int linenr, char *cmd, char *par)
{
	if (use_colors)
	{
		char *descr = find_next_par(par);

		cur_colorscheme_nr = n_cschemes++;

		cschemes = (color_scheme *)myrealloc(cschemes, n_cschemes * sizeof(color_scheme));
		memset(&cschemes[cur_colorscheme_nr], 0x00, sizeof(color_scheme));

		cschemes[cur_colorscheme_nr].descr = mystrdup(USE_IF_SET(descr, ""));
		cschemes[cur_colorscheme_nr].name  = mystrdup(par);
	}
}

void add_colorscript(int linenr, char *cmd, char *par)
{
	char *dummy1, *dummy2;

	dummy1 = find_next_par(par); /* find script */
	dummy2 = find_next_par(dummy1); /* find description */

	cur_colorscheme_nr = n_cschemes++;

	cschemes = (color_scheme *)myrealloc(cschemes, n_cschemes * sizeof(color_scheme));
	memset(&cschemes[cur_colorscheme_nr], 0x00, sizeof(color_scheme));

	cschemes[cur_colorscheme_nr].name  = mystrdup(par);
	cschemes[cur_colorscheme_nr].descr = mystrdup(dummy2);
	cschemes[cur_colorscheme_nr].color_script.script = mystrdup(dummy1);

	cur_colorscheme_nr = -1;
}

void add_editscheme(int linenr, char *cmd, char *par)
{
	char *descr = find_next_par(par);

	pes = (editscheme *)myrealloc(pes, sizeof(editscheme) * (n_es + 1));
	memset(&pes[n_es], 0x00, sizeof(editscheme));

	pes[n_es].es_name = mystrdup(par);
	pes[n_es].es_desc = mystrdup(USE_IF_SET(descr, ""));
	cur_editscheme_nr = n_es++;
}

void add_editrule(int linenr, char *cmd, char *par)
{
	char *type_str = par;
	char *par1 = find_next_par(type_str);
	char *par2 = NULL;
	striptype_t type = STRIP_TYPE_REGEXP;
	int rule_index = -1;

	if (!par1)
		config_error_exit(linenr, "editrule:%s requires a parameter.\n", type_str);

	if (strcmp(type_str, "kr") == 0)
		type = STRIP_TYPE_RANGE;
	else if (strcmp(type_str, "ke") == 0)
		type = STRIP_TYPE_REGEXP;
	else if (strcmp(type_str, "kc") == 0)
		type = STRIP_TYPE_COLUMN;
	else if (strcmp(type_str, "kS") == 0)
		type = STRIP_KEEP_SUBSTR;
	else
		config_error_exit(linenr, "editrule requirs either ke, kr, kS or kc.\n");

	if (type == STRIP_TYPE_RANGE || type == STRIP_TYPE_COLUMN)
	{
		par2 = find_next_par(par1);
		if (!par2)
			config_error_exit(linenr, "editrule:%s requires another parameter.\n", type_str);
	}

	rule_index = pes[cur_editscheme_nr].n_strips;
	pes[cur_editscheme_nr].strips = (strip_t *)myrealloc(pes[cur_editscheme_nr].strips, (pes[cur_editscheme_nr].n_strips + 1) * sizeof(strip_t));
	memset(&pes[cur_editscheme_nr].strips[pes[cur_editscheme_nr].n_strips], 0x00, sizeof(strip_t));
	pes[cur_editscheme_nr].n_strips++;

	pes[cur_editscheme_nr].strips[rule_index].type = type;

	if (type == STRIP_TYPE_RANGE)
	{
		pes[cur_editscheme_nr].strips[rule_index].start = atoi(par1);
		pes[cur_editscheme_nr].strips[rule_index].end   = atoi(par2);
	}
	else if (type == STRIP_TYPE_REGEXP || type == STRIP_KEEP_SUBSTR)
	{
		pes[cur_editscheme_nr].strips[rule_index].regex_str = mystrdup(par1);
		compile_re(&pes[cur_editscheme_nr].strips[rule_index].regex, par1);
	}
	else if (type == STRIP_TYPE_COLUMN)
	{
		pes[cur_editscheme_nr].strips[rule_index].del = mystrdup(par1);
		pes[cur_editscheme_nr].strips[rule_index].col_nr = atoi(par2);
	}
}

void add_filterscheme(int linenr, char *cmd, char *par)
{
	char *descr = find_next_par(par);

	pfs = (filterscheme *)myrealloc(pfs, sizeof(filterscheme) * (n_fs + 1));
	memset(&pfs[n_fs], 0x00, sizeof(filterscheme));

	pfs[n_fs].fs_name = mystrdup(par);
	pfs[n_fs].fs_desc = mystrdup(USE_IF_SET(descr, ""));
	cur_filterscheme_nr = n_fs++;
}

void add_filterscheme_rule(int linenr, char *cmd, char *par)
{
	char *type = par;
	int use_regex = 0x00;
	char *re_cmd = NULL;
	char *re_str = find_next_par(par);
	if (!re_str)
		config_error_exit(linenr, "Missing regular expression in rule-line for scheme %s.\n", pfs[cur_filterscheme_nr].fs_name);

	if (type[0] != 'e')
		config_error_exit(linenr, "Regular expression type '%s' is not recognized.\n", type);

	if (type[1] == 0x00 || type[1] == 'm')
		use_regex = 'm';
	else if (type[1] == 'v')
		use_regex = 'v';
	else if (type[1] == 'c')
		use_regex = 'c';
	else if (type[1] == 'C')
		use_regex = 'C';
	else if (toupper(type[1]) == 'X')
	{
		char *dummy = find_next_par(re_str);
		if (!dummy)
			config_error_exit(linenr, "Missing command for rule of type 'e%c' for scheme %s.\n", type[1], pfs[cur_filterscheme_nr].fs_name);

		re_cmd = mystrdup(dummy);
		use_regex = type[1];

		if (use_regex == 'X' && (strchr(re_str, '(') == NULL || strchr(re_str, ')') == NULL))
			config_error_exit(linenr, "Filter scheme rule: -eX requires a regular expression which selects a substring using '(' and ')'.\n");
	}

	pfs[cur_filterscheme_nr].pre = (re *)myrealloc(pfs[cur_filterscheme_nr].pre, (pfs[cur_filterscheme_nr].n_re + 1) * sizeof(re));
	memset(&pfs[cur_filterscheme_nr].pre[pfs[cur_filterscheme_nr].n_re], 0x00, sizeof(re));
	pfs[cur_filterscheme_nr].pre[pfs[cur_filterscheme_nr].n_re].use_regex = use_regex;
	pfs[cur_filterscheme_nr].pre[pfs[cur_filterscheme_nr].n_re].regex_str = mystrdup(re_str);
	compile_re(&pfs[cur_filterscheme_nr].pre[pfs[cur_filterscheme_nr].n_re].regex, re_str);
	pfs[cur_filterscheme_nr].pre[pfs[cur_filterscheme_nr].n_re].cmd = re_cmd;
	pfs[cur_filterscheme_nr].n_re++;
}

void add_convert(int linenr, char *cmd, char *par)
{
	char *conv_name = par;
	char *conv_type = NULL;
	conversion_t type = 0;
	char *conv_script = NULL;
	char *conv_re = NULL;
	int loop;

	/* parse type */
	conv_type = find_next_par(conv_name);
	if (!conv_type)
		config_error_exit(linenr, "'convert'-entry malformed: conversion type missing.\n");

	if (strncmp(conv_type, "script:", 7) == 0)
	{
		conv_script = find_next_par(conv_type);
		if (conv_script)
			conv_re = find_next_par(conv_script);
		else
			config_error_exit(linenr, "Convert: script filename missing.\n");
	}
	else
		conv_re = find_next_par(conv_type);

	if (!conv_re)
		config_error_exit(linenr, "'convert'-entry malformed: type or regular expression missing.\n");

	/* find this conversion: is it from an already known group? */
	for(loop=0; loop<n_conversions; loop++)
	{
		if (strcmp(conversions[loop].name, conv_name) == 0)
			break;
	}
	/* create new group */
	if (loop == n_conversions)
	{
		n_conversions++;
		conversions = (conversion *)myrealloc(conversions, sizeof(conversion) * n_conversions);
		memset(&conversions[loop], 0x00, sizeof(conversion));
		conversions[loop].name = mystrdup(conv_name);
	}

	if (strcmp(conv_type, "ip4tohost") == 0)
		type = CONVTYPE_IP4TOHOST;
	else if (strcmp(conv_type, "epochtodate") == 0)
		type = CONVTYPE_EPOCHTODATE;
	else if (strcmp(conv_type, "errnotostr") == 0)
		type = CONVTYPE_ERRNO;
	else if (strcmp(conv_type, "hextodec") == 0)
		type = CONVTYPE_HEXTODEC;
	else if (strcmp(conv_type, "dectohex") == 0)
		type = CONVTYPE_DECTOHEX;
	else if (strcmp(conv_type, "tai64todate") == 0)
		type = CONVTYPE_TAI64NTODATE;
	else if (strcmp(conv_type, "script") == 0)
		type = CONVTYPE_SCRIPT;
	else if (strcmp(conv_type, "abbrtok") == 0)
		type = CONVTYPE_ABBRTOK;
	else if (strcmp(conv_type, "signrtostring") == 0)
		type = CONVTYPE_SIGNRTOSTRING;
	else
		config_error_exit(linenr, "Convert %s: '%s' is a not recognized conversion type.\n", conv_name, conv_type);

	conversions[loop].pcb = (conversion_bundle_t *)myrealloc(conversions[loop].pcb, sizeof(conversion_bundle_t) * (conversions[loop].n + 1));
	conversions[loop].pcs = (script *)myrealloc(conversions[loop].pcs, sizeof(script) * (conversions[loop].n + 1));

	conversions[loop].pcb[conversions[loop].n].type = type;


	memset(&conversions[loop].pcs[conversions[loop].n], 0x00, sizeof(script));
	conversions[loop].pcs[conversions[loop].n].script = conv_script?mystrdup(conv_script):NULL;

	compile_re(&conversions[loop].pcb[conversions[loop].n].regex, conv_re);
	conversions[loop].pcb[conversions[loop].n].match_count = 0;
	conversions[loop].n++;
}

void use_filter_edit_scheme(int linenr, char *par, filter_edit_scheme_t type)
{
	char *re, *cur_scheme = par;
	char *title = "Filter";

	if (type == SCHEME_TYPE_EDIT)
		title = "Edit";

	if ((re = find_next_par(par)) == NULL)		/* format: cs_re:color:regular expression */
		config_error_exit(linenr, "%s scheme entry malformed: scheme name or regular expression missing.\n", title);

	/* find colorschemes */
	for(;;)
	{
		int scheme_nr;
		char *colon = strchr(cur_scheme, ',');
		if (colon) *colon = 0x00;

		scheme_nr = find_filterscheme(cur_scheme);
		if (scheme_nr == -1)
			config_error_exit(linenr, "%s scheme '%s' is unknown: you must first define a filterscheme before you can use it.\n", title, par);

		if (type == SCHEME_TYPE_EDIT)
			add_pars_per_file(re, NULL, -1, -1, -1, -1, scheme_nr, NULL);
		else if (type == SCHEME_TYPE_FILTER)
			add_pars_per_file(re, NULL, -1, -1, -1, scheme_nr, -1, NULL);

		if (!colon)
			break;

		cur_scheme = colon + 1;
	}
}

void use_filterscheme(int linenr, char *cmd, char *par)
{
	use_filter_edit_scheme(linenr, par, SCHEME_TYPE_FILTER);
}

void use_editscheme(int linenr, char *cmd, char *par)
{
	use_filter_edit_scheme(linenr, par, SCHEME_TYPE_EDIT);
}

void set_default_convert(int linenr, char *cmd, char *par)
{
	char *re;
	char *cur_cv = par;

	if ((re = find_next_par(par)) == NULL)		/* format: cs_re:color:regular expression */
		config_error_exit(linenr, "default_convert entry malformed: scheme name or regular expression missing.\n");

	for(;;)
	{
		char *colon = strchr(cur_cv, ',');
		if (colon) *colon = 0x00;

		add_pars_per_file(re, NULL, -1, -1, -1, -1, -1, cur_cv);

		if (!colon)
			break;

		cur_cv = colon + 1;
	}
}

void scheme(int linenr, char *cmd, char *par)
{
	if (use_colors)
	{
		char *re;
		char *cur_cs = par;

		if ((re = find_next_par(par)) == NULL)		/* format: cs_re:color:regular expression */
			config_error_exit(linenr, "scheme-entry malformed: scheme name or regular expression missing.\n");

		/* find colorschemes */
		for(;;)
		{
			char *colon = strchr(cur_cs, ',');
			if (colon) *colon = 0x00;

			add_pars_per_file(re, cur_cs, -1, -1, -1, -1, -1, NULL);

			if (!colon)
				break;

			cur_cs = colon + 1;
		}
	}
}

void set_defaultcscheme(int linenr, char *cmd, char *par)
{
	myfree(defaultcscheme);
	defaultcscheme = mystrdup(par);
}

void bind_char(int linenr, char *cmd, char *par)
{
	char *proc;

	if ((proc = find_next_par(par)) == NULL)
		config_error_exit(linenr, "bind parameter malformed: key or binding missing.\n");

	if (strlen(par) > 2) config_error_exit(linenr, "bind parameter malformed: unknown keyselection.\n");

	keybindings = (keybinding *)myrealloc(keybindings, sizeof(keybinding) * (n_keybindings + 1));

	if (par[0] == '^')
		keybindings[n_keybindings].key = toupper(par[1]) - 'A' + 1;
	else
		keybindings[n_keybindings].key = par[0];

	keybindings[n_keybindings].command = mystrdup(proc);
	n_keybindings++;
}

void set_suppress_empty_lines(int linenr, char *cmd, char *par)
{
	suppress_empty_lines = config_yes_no(par);
}

void set_close_closed_windows(int linenr, char *cmd, char *par)
{
	do_not_close_closed_windows = !config_yes_no(par);
}

void set_follow_filename(int linenr, char *cmd, char *par)
{
	default_follow_filename = config_yes_no(par);
}

void set_default_linewrap(int linenr, char *cmd, char *par)
{
	default_linewrap = par[0];

	if (default_linewrap == 'o')
	{
		char *dummy = find_next_par(par);
		if (!dummy)
			config_error_exit(linenr, "default_linewrap: 'o' needs a wrap offset parameter.\n");

		default_line_wrap_offset = atoi(dummy);
	}
}

void set_umask(int linenr, char *cmd, char *par)
{
	def_umask = strtol(par, NULL, 0);
}

void set_shell(int linenr, char *cmd, char *par)
{
	myfree(shell);
	shell = mystrdup(par);
}

void set_statusline_above_data(int linenr, char *cmd, char *par)
{
	statusline_above_data = config_yes_no(par);
}

void set_caret_notation(int linenr, char *cmd, char *par)
{
	caret_notation = config_yes_no(par);
}

void set_searches_case_insensitive(int linenr, char *cmd, char *par)
{
	re_case_insensitive = MY_TRUE;
}

void set_beep_method(int linenr, char *cmd, char *par)
{
	if (strcmp(par, "beep") == 0)
		beep_method = BEEP_BEEP;
	else if (strcmp(par, "flash") == 0)
		beep_method = BEEP_FLASH;
	else if (strcmp(par, "popup") == 0)
		beep_method = BEEP_POPUP;
	else if (strcmp(par, "none") == 0)
		beep_method = BEEP_NONE;
	else
		config_error_exit(linenr, "'%s' is an unknown beep method.\n");
}

void set_beep_popup_length(int linenr, char *cmd, char *par)
{
	beep_popup_length = atof(par);
	if (beep_popup_length < 0.0)
		config_error_exit(linenr, "beep_popup_length must be >= 0.0\n");
}

void set_allow_8bit(int linenr, char *cmd, char *par)
{
	allow_8bit = config_yes_no(par);
}

void set_dsblwm(int linenr, char *cmd, char *par)
{
	no_linewrap = 1 - config_yes_no(par);
}

void set_warn_closed(int linenr, char *cmd, char *par)
{
	warn_closed = config_yes_no(par);
}

void set_basename(int linenr, char *cmd, char *par)
{
	filename_only = config_yes_no(par);
}

void set_bright(int linenr, char *cmd, char *par)
{
	bright_colors = config_yes_no(par);
}

void set_ts_format(int linenr, char *cmd, char *par)
{
	myfree(ts_format);
	ts_format = mystrdup(par);
}

void set_cnv_ts_format(int linenr, char *cmd, char *par)
{
	myfree(cnv_ts_format);
	cnv_ts_format = mystrdup(par);
}

void set_statusline_ts_format(int linenr, char *cmd, char *par)
{
	myfree(statusline_ts_format);
	statusline_ts_format = mystrdup(par);
}

void set_markerline_attrs(int linenr, char *cmd, char *par)
{
	markerline_attrs = parse_attributes(par);
}

void set_idleline_color(int linenr, char *cmd, char *par)
{
	idleline_attrs = parse_attributes(par);
}

void set_msgline_color(int linenr, char *cmd, char *par)
{
	msgline_attrs = parse_attributes(par);
}

void set_changeline_color(int linenr, char *cmd, char *par)
{
	changeline_attrs = parse_attributes(par);
}

void set_statusline_attrs(int linenr, char *cmd, char *par)
{
	statusline_attrs = parse_attributes(par);
}

void set_splitline_attrs(int linenr, char *cmd, char *par)
{
	splitline_attrs = parse_attributes(par);
}

void set_inverse_attrs(int linenr, char *cmd, char *par)
{
	inverse_attrs = attrstr_to_nr(par);
}

void set_splitline(int linenr, char *cmd, char *par)
{
	if (strcmp(par, "none") == 0)
		splitline_mode = SL_NONE;
	else if (strcmp(par, "regular") == 0)
		splitline_mode = SL_REGULAR;
	else if (strcmp(par, "attributes") == 0)
		splitline_mode = SL_ATTR;
	else
		config_error_exit(linenr, "Parameter '%s' for 'splitline' not recognized.\n", par);
}

void set_show_subwindow_id(int linenr, char *cmd, char *par)
{
	show_subwindow_id = config_yes_no(par);
}

void set_markerline_timestamp(int linenr, char *cmd, char *par)
{
	timestamp_in_markerline = config_yes_no(par);
}

void set_global_default_nlines(int linenr, char *cmd, char *par)
{
	default_maxnlines = atoi(par);
}

void set_global_default_nkb(int linenr, char *cmd, char *par)
{
	default_maxbytes = kb_str_to_value(cmd, par);
}

void set_default_nlines(int linenr, char *cmd, char *par)
{
	char *re;
	int n_lines;

	if ((re = find_next_par(par)) == NULL)		/* format: cs_re:color:regular expression */
		config_error_exit(linenr, "Scheme entry malformed: scheme name or regular expression missing.\n");

	/* find colorscheme */
	n_lines = atoi(par);
	if (n_lines < -1)
		config_error_exit(linenr, "default_nlines: value cannot be lower then -1.\n");

	add_pars_per_file(re, NULL, n_lines, -1, -1, -1, -1, NULL);
}

void set_default_mark_change(int linenr, char *cmd, char *par)
{
	char *re;

	if ((re = find_next_par(par)) == NULL)		/* format: cs_re:color:regular expression */
		config_error_exit(linenr, "Scheme entry malformed: scheme name or regular expression missing.\n");

	add_pars_per_file(re, NULL, -1, -1, config_yes_no(par), -1, -1, NULL);
}

void set_default_bytes(int linenr, char *cmd, char *par)
{
	int bytes;
	char *re;

	if ((re = find_next_par(par)) == NULL)		/* format: cs_re:color:regular expression */
		config_error_exit(linenr, "Scheme entry malformed: scheme name or regular expression missing.\n");

	bytes = kb_str_to_value(cmd, par);

	add_pars_per_file(re, NULL, -1, bytes, -1, -1, -1, NULL);
}

void set_check_mail(int linenr, char *cmd, char *par)
{
	check_for_mail = get_value_arg("check_mail", par, VAL_ZERO_POSITIVE);
}

void set_tab_stop(int linenr, char *cmd, char *par)
{
	tab_width = atoi(par);
	if (tab_width == 0)
		config_error_exit(linenr, "tab_stop: value cannot be 0.\n");
}

void set_tail(int linenr, char *cmd, char *par)
{
	myfree(tail);
	tail = mystrdup(par);
}

void set_titlebar(int linenr, char *cmd, char *par)
{
	myfree(set_title);
	set_title = mystrdup(par);
}

void set_abbreviate_filesize(int linenr, char *cmd, char *par)
{
	afs = config_yes_no(par);
}

void set_replace_by_markerline(int linenr, char *cmd, char *par)
{
	myfree(replace_by_markerline);
	replace_by_markerline = mystrdup(par);
}

void set_popup_refresh_interval(int linenr, char *cmd, char *par)
{
	popup_refresh_interval = get_value_arg("popup_refresh_interval", par, VAL_POSITIVE);
}

void set_msgline_char(int linenr, char *cmd, char *par)
{
	msgline_char = par[0];
}

void set_idleline_char(int linenr, char *cmd, char *par)
{
	idleline_char = par[0];
}

void set_changeline_char(int linenr, char *cmd, char *par)
{
	changeline_char = par[0];
}

void set_markerline_char(int linenr, char *cmd, char *par)
{
	markerline_char = par[0];
}

void set_global_mark_change(int linenr, char *cmd, char *par)
{
	global_marker_of_other_window = config_yes_no(par);
}

void set_default_bufferwhat(int linenr, char *cmd, char *par)
{
	char what = par[0];

	if (what != 'a' && what != 'f')
		config_error_exit(linenr, "default_bufferwhat expects either 'a' or 'f'. Got: '%c'.\n", what);

	default_bufferwhat = what;
}

void set_abort_key(int linenr, char *cmd, char *par)
{
	int dummy = atoi(par);

	if (dummy < 0 || dummy > 255)
		config_error_exit(linenr, "abort_key expects an ascii value which is >= 0 && <= 255.\n");

	abort_key = dummy;
}

void set_exit_key(int linenr, char *cmd, char *par)
{
	int dummy = atoi(par);

	if (dummy < 0 || dummy > 255)
		config_error_exit(linenr, "exit_key expects an ascii value which is >= 0 && <= 255.\n");

	exit_key = dummy;
}

void set_line_ts_format(int linenr, char *cmd, char *par)
{
	myfree(line_ts_format);
	line_ts_format = mystrdup(par);
}

void set_default_min_shrink(int linenr, char *cmd, char *par)
{
	default_min_shrink = get_value_arg("min_shrink", par, VAL_POSITIVE);
}

void set_scrollback_show_winnrs(int linenr, char *cmd, char *par)
{
	default_sb_showwinnr = config_yes_no(par);
}

void set_wordwrapmaxlength(int linenr, char *cmd, char *par)
{
	wordwrapmaxlength = get_value_arg("wordwrapmaxlength", par, VAL_POSITIVE);
}

void set_xclip(int linenr, char *cmd, char *par)
{
	if (file_exist(par) == -1)
		error_exit(TRUE, FALSE, "xclip binary '%s' does not exist");

	clipboard = strdup(par);
}

void set_pbcopy(int linenr, char *cmd, char *par)
{
	if (file_exist(par) == -1)
		error_exit(TRUE, FALSE, "pbcopy binary '%s' does not exist");

	clipboard = strdup(par);
}

void set_map_delete_as_backspace(int linenr, char *cmd, char *par)
{
	map_delete_as_backspace = config_yes_no(par);
}

void set_searchhistory_file(int linenr, char *cmd, char *par)
{
	if (par[0] == 0x00)
	{
		search_h.history_file = NULL;
		search_h.history_size = 0;
	}
	else
	{
		search_h.history_file = myrealpath(par);
	}
}

void set_searchhistory_size(int linenr, char *cmd, char *par)
{
	int hs = atoi(par);

	if (hs <= 0)
	{
		search_h.history_file = NULL;
		search_h.history_size = 0;
	}
	else
	{
		search_h.history_size = hs;
	}
}

void set_cmdfile_history_file(int linenr, char *cmd, char *par)
{
	if (par[0] == 0x00)
	{
		cmdfile_h.history_file = NULL;
		cmdfile_h.history_size = 0;
	}
	else
	{
		cmdfile_h.history_file = myrealpath(par);
	}
}

void set_cmdfile_history_size(int linenr, char *cmd, char *par)
{
	int hs = atoi(par);

	if (hs <= 0)
	{
		cmdfile_h.history_file = NULL;
		cmdfile_h.history_size = 0;
	}
	else
	{
		cmdfile_h.history_size = hs;
	}
}

void set_default_background_color(int linenr, char *cmd, char *par)
{
	if (use_colors)
	{
		default_bg_color = colorstr_to_nr(par);

		if (default_bg_color == -1)
			config_error_exit(linenr, "default_background_color: '%s' is not a recognized color.", par);
	}
}

void set_reuse_searchstring(int linenr, char *cmd, char *par)
{
	reuse_searchstring = config_yes_no(par);
}

void set_min_n_bufferlines(int linenr, char *cmd, char *par)
{
	min_n_bufferlines = atoi(par);
	if (min_n_bufferlines < 0)
		config_error_exit(linenr, "min_n_bufferlines must have a value >= 0.");
}

void set_box_bottom_left_hand_corner(int linenr, char *cmd, char *par)
{
	box_bottom_left_hand_corner = par[0];
}

void set_box_bottom_right_hand_corner(int linenr, char *cmd, char *par)
{
	box_bottom_right_hand_corner = par[0];
}

void set_box_bottom_side(int linenr, char *cmd, char *par)
{
	box_bottom_side = par[0];
}

void set_box_left_side(int linenr, char *cmd, char *par)
{
	box_left_side = par[0];
}

void set_box_right_side(int linenr, char *cmd, char *par)
{
	box_right_side = par[0];
}

void set_box_top_left_hand_corner(int linenr, char *cmd, char *par)
{
	box_top_left_hand_corner = par[0];
}

void set_box_top_right_hand_corner(int linenr, char *cmd, char *par)
{
	box_top_right_hand_corner = par[0];
}

void set_box_top_side(int linenr, char *cmd, char *par)
{
	box_top_side = par[0];
}

void set_window_number(int linenr, char *cmd, char *par)
{
	myfree(window_number);
	window_number = mystrdup(par);
}

void set_subwindow_number(int linenr, char *cmd, char *par)
{
	myfree(subwindow_number);
	subwindow_number = mystrdup(par);
}

void set_posix_tail(int linenr, char *cmd, char *par)
{
	posix_tail = config_yes_no(par);
}

void set_syslog_ts_format(int linenr, char *cmd, char *par)
{
	myfree(syslog_ts_format);
	syslog_ts_format = mystrdup(par);
}

void set_resolv_ip_addresses(int linenr, char *cmd, char *par)
{
	resolv_ip_addresses = config_yes_no(par);
}

void set_show_severity_facility(int linenr, char *cmd, char *par)
{
	show_severity_facility = config_yes_no(par);
}

void set_scrollback_fullscreen_default(int linenr, char *cmd, char *par)
{
	scrollback_fullscreen_default = config_yes_no(par);
}

void set_scrollback_no_colors(int linenr, char *cmd, char *par)
{
	scrollback_no_colors = config_yes_no(par);
}

void set_scrollback_search_new_window(int linenr, char *cmd, char *par)
{
	scrollback_search_new_window = config_yes_no(par);
}

void set_gnu_tail(int linenr, char *cmd, char *par)
{
        gnu_tail = config_yes_no(par);
}

config_file_keyword cf_entries[] = {
	{ "abbreviate_filesize", set_abbreviate_filesize },
	{ "abort_key", set_abort_key },
	{ "allow_8bit", set_allow_8bit },
	{ "basename", set_basename },
	{ "beep_method", set_beep_method },
	{ "beep_popup_length", set_beep_popup_length },
	{ "bind", bind_char },
	{ "box_bottom_left_hand_corner", set_box_bottom_left_hand_corner },
	{ "box_bottom_right_hand_corner", set_box_bottom_right_hand_corner },
	{ "box_bottom_side", set_box_bottom_side },
	{ "box_left_side", set_box_left_side },
	{ "box_right_side", set_box_right_side },
	{ "box_top_left_hand_corner", set_box_top_left_hand_corner },
	{ "box_top_right_hand_corner", set_box_top_right_hand_corner },
	{ "box_top_side", set_box_top_side },
	{ "bright", set_bright },
	{ "caret_notation", set_caret_notation },
	{ "changeline_char", set_changeline_char },
	{ "changeline_color", set_changeline_color },
	{ "check_mail", set_check_mail },
	{ "close_closed_windows", set_close_closed_windows },
	{ "cmdfile_history_file", set_cmdfile_history_file },
	{ "cmdfile_history_size", set_cmdfile_history_size },
	{ "cnv_ts_format", set_cnv_ts_format },
	{ "colorscheme", add_colorscheme },
	{ "colorscript", add_colorscript },
	{ "convert", add_convert },
	{ "cs_re", add_cs_re },
	{ "cs_re_s", add_cs_re },
	{ "cs_re_val_bigger", add_cs_re },
	{ "cs_re_val_equal", add_cs_re },
	{ "cs_re_val_less", add_cs_re },
	{ "default_background_color", set_default_background_color },
	{ "default_bufferwhat", set_default_bufferwhat },
	{ "default_bytes", set_default_bytes },
	{ "default_convert", set_default_convert },
	{ "default_linewrap", set_default_linewrap },
	{ "default_mark_change", set_default_mark_change },
	{ "default_nlines", set_default_nlines },
	{ "defaultcscheme", set_defaultcscheme },
	{ "dsblwm", set_dsblwm },
	{ "editrule", add_editrule },
	{ "editscheme", add_editscheme },
	{ "exit_key", set_exit_key },
	{ "filterscheme", add_filterscheme },
	{ "follow_filename", set_follow_filename },
	{ "global_default_nkb", set_global_default_nkb },
	{ "global_default_nlines", set_global_default_nlines },
	{ "global_mark_change", set_global_mark_change },
	{ "gnu_tail", set_gnu_tail },
	{ "idleline_char", set_idleline_char },
	{ "idleline_color", set_idleline_color },
	{ "include", do_load_config },
	{ "inverse", set_inverse_attrs },
	{ "line_ts_format", set_line_ts_format },
	{ "map_delete_as_backspace", set_map_delete_as_backspace },
	{ "markerline_char", set_markerline_char },
	{ "markerline_color", set_markerline_attrs },
	{ "markerline_timestamp", set_markerline_timestamp },
	{ "mcsre", add_cs_re },
	{ "mcsre_s", add_cs_re },
	{ "mcsre_val_bigger", add_cs_re },
	{ "mcsre_val_equal", add_cs_re },
	{ "mcsre_val_less", add_cs_re },
	{ "min_n_bufferlines", set_min_n_bufferlines },
	{ "min_shrink", set_default_min_shrink },
	{ "msgline_char", set_msgline_char },
	{ "msgline_color", set_msgline_color },
	{ "popup_refresh_interval", set_popup_refresh_interval },
	{ "posix_tail", set_posix_tail },
	{ "replace_by_markerline", set_replace_by_markerline },
	{ "resolv_ip_addresses", set_resolv_ip_addresses },
	{ "reuse_searchstring", set_reuse_searchstring },
	{ "rule", add_filterscheme_rule },
	{ "scheme", scheme },
	{ "scrollback_fullscreen_default", set_scrollback_fullscreen_default },
	{ "scrollback_no_colors", set_scrollback_no_colors },
	{ "scrollback_search_new_window", set_scrollback_search_new_window },
	{ "scrollback_show_winnrs", set_scrollback_show_winnrs },
	{ "searches_case_insensitive", set_searches_case_insensitive },
	{ "searchhistory_file", set_searchhistory_file },
	{ "searchhistory_size", set_searchhistory_size },
	{ "shell", set_shell },
	{ "show_severity_facility", set_show_severity_facility },
	{ "show_subwindow_id", set_show_subwindow_id },
	{ "splitline", set_splitline },
	{ "splitline_attrs", set_splitline_attrs },
	{ "statusline_above_data", set_statusline_above_data },
	{ "statusline_attrs", set_statusline_attrs },
	{ "statusline_ts_format", set_statusline_ts_format },
	{ "subwindow_number", set_subwindow_number },
	{ "suppress_empty_lines", set_suppress_empty_lines },
	{ "syslog_ts_format", set_syslog_ts_format },
	{ "tab_stop", set_tab_stop },
	{ "tail", set_tail },
	{ "titlebar", set_titlebar },
	{ "ts_format", set_ts_format },
	{ "umask", set_umask },
	{ "useeditscheme", use_editscheme },
	{ "usefilterscheme", use_filterscheme },
	{ "warn_closed", set_warn_closed },
	{ "window_number", set_window_number },
	{ "wordwrapmaxlength", set_wordwrapmaxlength },
	{ "xclip", set_xclip },
#ifdef __APPLE__
        { "pbcopy", set_pbcopy },
#endif
};

int find_config_entry_in_dispatch_table(char *cmd_name)
{
	int left = 0;
	int right = (sizeof(cf_entries) / sizeof(config_file_keyword)) - 1;

	while(left <= right)
	{
		int mid = (left + right) / 2;
		int compare = strcmp(cmd_name, cf_entries[mid].config_keyword);

		if (compare > 0)
			left = mid + 1;
		else if (compare < 0)
			right = mid - 1;
		else
			return mid;
	}

	return -1;
}

int config_file_entry(int linenr, char *cmd)
{
	int function_nr;
	char *par = NULL;

	/* remove spaces at the beginning of the line */
	while (isspace(*cmd)) cmd++;

	/* skip empty lines and comments */
	if (cmd[0] == 0x00 || cmd[0] == '#' || cmd[0] == ';')
		return 0;

	/* lines are in the format of command:parameter */
	if ((par = find_next_par(cmd)) == NULL)
		config_error_exit(linenr, "Malformed configuration line found: %s (command delimiter (':') missing).\n", cmd);

	function_nr = find_config_entry_in_dispatch_table(cmd);
	if (function_nr == -1)
		return -1;

	cf_entries[function_nr].function(linenr, cmd, par);

	return 0;
}

int sort_colorschemes_compare(const void *a, const void *b)
{
	color_scheme *pa = (color_scheme *)a;
	color_scheme *pb = (color_scheme *)b;

	return strcmp(pa -> name, pb -> name);
}

/* returns the default color scheme or -1 if none */
void do_load_config(int dummynr, char *dummy, char *file)
{
	static char sorted = 0;
	FILE *fh;
	int linenr = 0;

	/* given file */
	fh = fopen(file, "r");
	if (fh == NULL)
	{
		if (errno == ENOENT)	/* file does not exist, not an error */
			return;

		error_exit(TRUE, FALSE, "do_load_config: error loading configfile '%s'\n", file);
	}

	for(;;)
	{
		char read_buffer[CONFIG_READ_BUFFER];
		char *dummy;
		char *cmd = fgets(read_buffer, sizeof(read_buffer) - 1, fh);
		if (!cmd)
			break;

		linenr++;

		/* strip LF */
		dummy = strchr(cmd, '\n');
		if (dummy)
			*dummy = 0x00;
		else
			error_exit(FALSE, FALSE, "line %d of file '%s' is too long!\n", linenr, file);

		/* LOG("%d: %s\n", linenr, cmdin); */
		if (config_file_entry(linenr, cmd) == -1)
			error_exit(FALSE, FALSE, "Configuration parameter '%s' is unknown (file: %s, line: %d).\n", read_buffer, file, linenr);
	}
	fclose(fh);

	if (!sorted)
	{
		sorted = 1;
		qsort(cschemes, n_cschemes, sizeof(color_scheme), sort_colorschemes_compare);
	}
}

void load_configfile_wrapper(char *config_file)
{
	/* load configurationfile (if any) */
	if (load_global_config) {
		DIR *dir = opendir("/etc/multitail/cfg.d");
		int path_max = find_path_max();
		char *path = mymalloc(path_max + 1);

		do_load_config(-1, NULL, CONFIG_FILE);

		for(;dir;) {
			struct dirent *de = readdir(dir);
			if (!de)
				break;

			snprintf(path, path_max, "/etc/multitail/cfg.d/%s", de->d_name);

			do_load_config(-1, NULL, path);
		}

		free(path);

		if (dir)
			closedir(dir);
	}

	if (config_file)
	{
		do_load_config(-1, NULL, config_file);
	}
	else
	{
		int path_max = find_path_max();
		char *path = mymalloc(path_max + 1);
		char *xdg_config = getenv("XDG_CONFIG_HOME");
		char *home = getenv("HOME");
		struct passwd *pp = getuserinfo();

		if (xdg_config)
			snprintf(path, path_max, "%s/multitail/config", xdg_config);
		else if (home)
			snprintf(path, path_max, "%s/.multitailrc", home);
		else if (pp)
			snprintf(path, path_max, "%s/.multitailrc", pp -> pw_dir);

		if (xdg_config || home || pp)
			do_load_config(-1, NULL, path);

		myfree(path);
	}
}

char load_configfile(char *config_file)
{
	static char config_loaded = 0;

	if (config_loaded == 0)
	{
		/* load configurationfile (if any) */
		load_configfile_wrapper(config_file);

		config_loaded = 1;

		return 1;
	}

	return 0;
}
