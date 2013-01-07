#define _LARGEFILE64_SOURCE     /* required for GLIBC to enable stat64 and friends */
#include "doassert.h"
#include <sys/types.h>
#include <regex.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "mt.h"
#include "mem.h"
#include "error.h"
#include "color.h"
#include "term.h"
#include "utils.h"
#include "config.h"
#include "cv.h"
#include "exec.h"
#include "globals.h"


void usage(void)
{
	printf("%s", version_str);
	printf("\n\nmultitail [-cs|-Cs|-c-] [-i] inputfile [-i anotherinputfile] [...]\n");
	printf("-i x	the following parameter is a filename (in case it starts with a dash)\n");
	printf("-I x	like -i only this one merges this logfile into the previous window\n");
	printf("-l x	parameter is a command to be executed\n");
	printf("-L x	see -l but add to the previous window\n");
	printf("-j	read from stdin (can be used (of course) only once)\n");
	printf("-J      like -j but merge into previous window\n");
	printf("--listen [interface]:port behave like a syslog server. port is normally 514\n");
	printf("--Listen [interface]:port like --listen but merge into previous window\n");
	printf("-r interval    restart the command when it died after `interval' seconds\n");
	printf("-R interval    same as -r, only with this one only the difference is displayed\n");
	printf("-Rc/-rc interval  like -r/-R but clean the window before each iteration\n");
	printf("--cont         reconnect lines with a '\' at the end\n");
	printf("--mergeall     merge all of the following files into the same window (in the previous window)\n");
	printf("--mergeall-new merge all of the following files into the same window (in a new window)\n");
	printf("--no-mergeall  stop merging all files into one window\n");
	printf("--no-repeat    suppress repeating lines and replace them with a \"last message repeated x times\"\n");
	printf("--mark-interval x	when nothing comes in, print a '---mark---' line every 'x' seconds\n");
	printf("--mark-change	when multiple files are merged an multitail switches between two windows, print a markerline with the filename\n");
	printf("--no-mark-change	do NOT print the markerline when the file changes (overrides the configfile)\n");
	printf("-n x	initial number of lines to tail\n");
	printf("-m x	set scrollback buffer size (# lines)\n");
	printf("-mb x   set scrollback buffer size (in bytes, use xKB/MB/GB)\n");
	printf("-bw a/f what to buffer: 'a'll or what went through the 'f'ilter\n");
	printf("-a x	like 'tee': write (filtered) input to file 'x'\n");
	printf("-A x	see -a: but write the unfiltered(!) input to file 'x'\n");
	printf("-g x	redirect the input also (filtered) to command/process 'x'\n");
	printf("-G x	redirect the unfiltered input also  to command/process 'x'\n");
	printf("--label x	put in front of each line\n");
	printf("-q i path       check every 'i' seconds for new files in 'path', create a new window for those\n");
	printf("-Q i path       check every 'i' seconds for new files in 'path', put them all in the same window (using subwindows)\n");
	printf("--closeidle x   close windows when more then 'x' seconds no new data was processed\n"); 
	printf("--new-only	(for -q/-Q) only create windows for files created after multitail was started\n");
	printf("-s x	vertical split screen (in 'x' columns)\n");
	printf("-sw x,x,...	at what columns to split the screen, use '0' for automatic size\n");
	printf("-sn x,x,...     number of windows per column\n");
	printf("-wh x	height of window\n");
	printf("-S	prepend merged output with subwindow-number\n");
	printf("-f	follow the following filename, not the descriptor\n");
	printf("--follow-all	for all files after this switch; follow the filename instead of the descriptor\n");
	printf("--retry	keep trying to open the following file if it is inaccessible\n");
	printf("--retry-all like --retry but for all following files\n");
	printf("-fr scheme  use the predefined filter from the configfile\n");
	printf("-e[m]	print only when matching with this regexp\n");
	printf("-ev	print only when NOT matching with this regexp\n");
	printf("-ec	use regular expression but display the matches inverted on following file\n");
	printf("-eC	use regexp, display everything but matches inverted on following file\n");
	printf("-ex	execute command ('-ex regexp command') when matches, matching line is given as commandline parameter\n");
	printf("-eX     like -ex but only give the matching substring as commandline parameter to the command\n");
	printf("-E	use regular expression on following files\n");
	printf("-Ec	use regular expression but display the matches inverted on following files\n");
	printf("-EC	use regexp, display everything but matches inverted on following files\n");
	printf("-ke x   strip parts of the input using regular expression 'x'\n");
	printf("-kr x y strip parts of the input starting at offset x and ending (not including!) offset y\n");
	printf("-kc x y strip parts of the input: strip column 'y' with delimiter 'x'\n");
	printf("-ks x   use edit scheme 'x' (defined in configfile)\n");
	printf("-kS x   only show the substrings matched by the substring-selects (the parts between '(' and ')') in the regular epxression 'x'\n");
	printf("-v      invert next regular expression (do not use with -ev/em)\n");
	printf("-cv x	use conversion scheme 'x' (see multitail.conf)\n");
	printf("-c	colorize current\n");
	printf("-cS scheme	use colorscheme 'scheme' (as defined in multitail.conf)\n");
	printf("-csn    extra switch for the following switches; do not use reverse (inverted) colors\n");
	printf("-Cs	colorize all following files with syslog-scheme\n");
	printf("-C	colorize all following files\n");
	printf("-Cf/-cf field delimiter		colorize next/all file(s) depending on the given field number. fields are delimited with the given field-delimiter\n");
	printf("-ci color  use 'color' (red, green, etc), usefull when merging multiple inputs\n");
	printf("-c-	do NOT colorize the following file\n");
	printf("-C-	do NOT colorize the following files\n");
	printf("-cT term	interpret terminal-codes from file/command (for terminal type 'term')\n");
	printf("-Z color   set color for markerline\n");
	printf("-ts	add a timestamp (format configurable in multitail.conf) before each line\n");
	printf("-T      put a timestamp in markerlines\n");
	printf("-d	do NOT update the status-line\n");
	printf("-D	do not display a status-line at all\n");
	printf("-du     put the statusline above the data window\n");
	printf("-z	do not \"window closed\" windows\n");
	printf("-w	do not use colors\n");
	printf("-u	set update interval (for slow links)\n");
	printf("-p x [y]	set linewrap (l=left/a=all/r=right/s=syslog,S=syslog w/o procname,o=offset -> 'y',w=wordwrap)\n");
	printf("-P      like -p but for all following files\n");
	printf("-b n	set TAB-width\n");
	printf("--basename	only display the filename (and not the path) in the statusline\n");
	printf("-x str	switch on the xtermtitlebar stuff\n");
#ifndef S_SPLINT_S
	printf("-F file   use 'file' as configfile (instead of " CONFIG_FILE ")\n");
	printf("--no-load-global-config do not read " CONFIG_FILE "\n");
#endif
	printf("-o configfileparameter    do a setting which would normally be set in the configfile\n");
	printf("-H x	show heartbeat (to keep your sessions alive)\n");
	printf("-iw file i	check every 'i' seconds if 'file' appeared in the filesystem\n");
	printf("-t x	display 'x' in the window-title (when MultiTail runs in an xterm)\n");
	printf("--beep-interval x	beep every x lines processed\n");
	printf("--bi x	like '--beep-interval' but only for current (sub-)window\n");
	printf("-V	show version and exit\n");
	printf("-h	this help\n");
	printf("\n");
	printf("You can have multiple regular expressions per file/command. Be warned: if\n");
	printf("you define multiple and one of them is specified with '-E' (=for every\n");
	printf("following file), _all_ of the current regular expressions are for all\n");
	printf("following files!\n");
	printf("\n");

	printf("%s\n", F1);
}

void add_redir_to_file(char mode, char *file, redirect_t **predir, int *n_redirect)
{
	int cur_index = (*n_redirect)++;
	*predir = (redirect_t *)myrealloc(*predir, (*n_redirect) * sizeof(redirect_t));

	assert(mode == 'A' || mode == 'a');

	if (mode == 'a')
		(*predir)[cur_index].type = REDIRECTTO_FILE_FILTERED;
	else
		(*predir)[cur_index].type = REDIRECTTO_FILE;

	(*predir)[cur_index].redirect = mystrdup(file);

	(*predir)[cur_index].fd = open((*predir)[cur_index].redirect, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
	if ((*predir)[cur_index].fd == -1)
		error_exit("%s: cannot open file %s for write access.\n", file, (*predir)[cur_index].redirect);
}

void add_redir_to_proc(char mode, char *proc, redirect_t **predir, int *n_redirect)
{
	int fds_to_proc[2], fds_from_proc[2];

	assert(mode == 'G' || mode == 'g');

	*predir = (redirect_t *)myrealloc(*predir, (*n_redirect + 1) * sizeof(redirect_t));

	if ((*predir)[*n_redirect].type != REDIRECTTO_NONE) error_exit("One can only set one redirection-type per (sub-)window.\n");

	if (mode == 'g')
		(*predir)[*n_redirect].type = REDIRECTTO_PIPE_FILTERED;
	else
		(*predir)[*n_redirect].type = REDIRECTTO_PIPE;

	(*predir)[*n_redirect].redirect = mystrdup(proc);

	(*predir)[*n_redirect].pid = exec_with_pipe((*predir)[*n_redirect].redirect, fds_to_proc, fds_from_proc);
	myclose(fds_to_proc[0]);
	myclose(fds_from_proc[0]);
	myclose(fds_from_proc[1]);

	(*predir)[*n_redirect].fd = fds_to_proc[1];

	(*n_redirect)++;
}

void add_redir_to_socket(char filtered, char *prio, char *fac, char *address, redirect_t **predir, int *n_redirect)
{
	char *local_address = mystrdup(address);
	char *colon = strchr(local_address, ':');
	int prio_nr = -1, fac_nr = -1;
	int loop;
    char* node;
    char* service;
    struct addrinfo hints;
    struct addrinfo* result;
    struct addrinfo* rp;
    int s, sfd = -1;

	*predir = (redirect_t *)myrealloc(*predir, (*n_redirect) * sizeof(redirect_t));

	assert(filtered == 1 || filtered == 0);

	if (filtered)
		(*predir)[*n_redirect].type = REDIRECTTO_SOCKET_FILTERED;
	else
		(*predir)[*n_redirect].type = REDIRECTTO_SOCKET;

	(*predir)[*n_redirect].redirect = mystrdup(address);

	if (colon)
	{
		*colon = 0x00;
		node = local_address;
		service = colon + 1;
	}
	else
	{
		node = local_address;
		service = "syslog";
	}

	memset(&hints, 0x00, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	s = getaddrinfo(node, service, &hints, &result);
	if (s != 0)
		error_exit("Cannot create socket for redirecting via syslog protocol: %s.\n", gai_strerror(s));

	for (rp = result; rp != NULL; rp = rp -> ai_next)
	{
		sfd = socket(rp -> ai_family, rp -> ai_socktype, rp -> ai_protocol);
		if (sfd == -1)
		    continue;
		if (connect(sfd, rp -> ai_addr, rp -> ai_addrlen) != -1)
		    break;
		close(sfd);
	}

	freeaddrinfo(result);

	if (rp == NULL)
		error_exit("Cannot create socket for redirecting via syslog protocol.\n");

	(*predir)[*n_redirect].fd = sfd;
	
	for(loop=0; loop<8; loop++)
	{
		if (strcasecmp(severities[loop], prio) == 0)
		{
			prio_nr = loop;
			break;
		}
	}
	if (prio_nr == -1)
		error_exit("Priority '%s' is not recognized.\n", prio);
	
	for(loop=0; loop<24; loop++)
	{
		if (strcasecmp(facilities[loop], fac) == 0)
		{
			fac_nr = loop;
			break;
		}
	}
	if (fac_nr == -1)
		error_exit("Facility '%s' is not known.\n", fac);

	(*predir)[*n_redirect].prio_fac = (fac_nr * 8) + prio_nr;

	myfree(local_address);

	(*n_redirect)++;
}

void argv_set_window_widths(char *widths)
{
	if (split != 0)
		error_exit("-s and -sw are mutual exclusive.\n");

	split = 0;
	for(;;)
	{
		int cur_width;
		char *pnt;

		pnt = strtok(widths, ",");
		if (!pnt) break;

		split++;
		vertical_split = (int *)myrealloc(vertical_split, split * sizeof(int));

		cur_width = get_value_arg("-sw", pnt, VAL_POSITIVE);
		widths = NULL;

		if (cur_width < 4)
		{
			if (cur_width == 0)
				cur_width = -1;
			else
				error_exit("The width of a column must be 4 or greater (or '0' for automatic size).\n");
		}

		vertical_split[split - 1] = cur_width;
	}

	if (split == 1)
		error_exit("You have to give the width for each window or set it to 0 (=auto width).\n");
}

void argv_set_n_windows_per_column(char *pars)
{
	int index = 0;

	if (split == 0)
		error_exit("First use -s or -sw to define the number of columns.\n");

	for(;;)
	{
		int cur_n;
		char *pnt;

		pnt = strtok(pars, ",");
		if (!pnt) break;

		index++;
		n_win_per_col = (int *)myrealloc(n_win_per_col, index * sizeof(int));

		cur_n = get_value_arg("-sn", pnt, VAL_POSITIVE);
		pars = NULL;

		if (cur_n < 0)
			error_exit("The number of windows must be either 0 (=auto) or >= 1.\n");

		n_win_per_col[index - 1] = cur_n;
	}
}

int argv_add_re(char *mode, char *pars[], char invert_regex, re **pre_cur, int *n_re_cur, re **pre_all, int *n_re_all)
{
	char *cmd = NULL, *expr = NULL;
	char regex_mode = 0;
	int n_pars_used = 0;
	re **pre = pre_cur;
	int *n_re = n_re_cur;

	/* -e => only for this file, -E => for all following files */
	if (mode[1] == 'E')
	{
		pre = pre_all;
		n_re = n_re_all;
	}

	/* c/C/m define colors, x says: execute */
	if (toupper(mode[2]) == 'C')
		regex_mode = mode[2];
	else if (toupper(mode[2]) == 'B')
		regex_mode = mode[2];
	else if (mode[2] == 'm' || mode[2] == 0x00)
		regex_mode = 'm';	/* m = match, only print when matches */
	else if (mode[2] == 'v')
		regex_mode = 'v';	/* v = !match, only print when not matching */
	else if (toupper(mode[2]) == 'X')
		regex_mode = mode[2];	/* x = execute */
	else
		error_exit("%s is an unknown switch.\n", mode);

	/* get expression */
	expr = pars[n_pars_used++];

	/* and if there's anything to execute, get commandline */
	if (toupper(regex_mode) == 'X')
	{
		cmd = pars[n_pars_used++];

		if (regex_mode == 'X' && (strchr(expr, '(') == NULL || strchr(expr, ')') == NULL))
			error_exit("Filterscheme rule: -eX requires a regular expression which selects a substring using '(' and ')'.\n");
	}

	/* compile & set expression */
	/* allocate new structure */
	*pre = (re *)myrealloc(*pre, sizeof(re) * (*n_re + 1));

	/* initialize structure */
	memset(&(*pre)[*n_re], 0x00, sizeof(re));

	/* compile */
	compile_re(&(*pre)[*n_re].regex, expr);

	/* remember string for later edit */
	(*pre)[*n_re].regex_str = mystrdup(expr);

	/* set flag on current file */
	(*pre)[*n_re].use_regex = regex_mode;
	if (mode[1] == 'E')
		regex_mode = 0;

	/* wether to invert the reg exp or not */
	if ((regex_mode == 'v' || regex_mode == 'm') && invert_regex)
	{
		error_exit("-e[m] / -ev cannot be used together with -v\n");
	}
	(*pre)[*n_re].invert_regex = invert_regex;

	/* what to execute (if...) */
	if (cmd)
		(*pre)[*n_re].cmd = mystrdup(cmd);
	else
		(*pre)[*n_re].cmd = NULL;

	(*n_re)++;

	return n_pars_used;
}

int argv_color_settings(char *mode, char *pars[], char *allcolor, char *curcolor, int *field_index, char **field_delimiter, myattr_t *cdef, term_t *cur_term_emul, int_array_t *cur_color_schemes, myattr_t *alt_col_cdev1, myattr_t *alt_col_cdev2)
{
	int n_pars_used = 0;
	char cur_mode = mode[2], doall = 0;

	if (mode[1] == 'C')
		doall = 1;

	if (cur_mode == 's')	/* syslog-file coloring? */
	{
	}
	else if (cur_mode == 'a')	/* alternating colors */
	{
		*alt_col_cdev1 = parse_attributes(pars[n_pars_used++]);
		*alt_col_cdev2 = parse_attributes(pars[n_pars_used++]);
	}
	else if (cur_mode == 'i')	/* use one specific color */
	{
		*cdef = parse_attributes(pars[n_pars_used++]);
	}
	else if (cur_mode == 'T')	/* terminal mode */
	{
		if (pars[n_pars_used] != NULL && (strcasecmp(pars[n_pars_used], "ANSI") == 0 || strcasecmp(pars[n_pars_used], "vt100") == 0))
			*cur_term_emul = TERM_ANSI;
		else
			error_exit("Terminal emulation '%s' is not known.\n", pars[n_pars_used]);

		n_pars_used++;
	}
	else if (cur_mode == 'S')	/* use colorscheme */
	{
		int cur_scheme_index;
		char *cur_cscheme = pars[n_pars_used++];
		if (!cur_cscheme)
			error_exit("%s requires a color scheme name.\n", mode);

		if ((cur_scheme_index = find_colorscheme(cur_cscheme)) == -1)
		{
			if (use_colors)
				error_exit("Color scheme %s not found! Please check your configuration file.\n", cur_cscheme);
			else
				error_exit("Color schemes are not supported on monochrome terminals.\n");
		}

		add_color_scheme(cur_color_schemes, cur_scheme_index);
	}
	else if (cur_mode == '-')	/* do not color current */
	{
		cur_mode = 'n';
	}
	else if (cur_mode == 'f')	/* select field for coloring */
	{
		*field_index = get_value_arg(mode, pars[n_pars_used++], VAL_ZERO_POSITIVE);
		*field_delimiter = pars[n_pars_used++];
	}
	else if (cur_mode == 0x00)	/* use complete line for coloring */
	{
		cur_mode = 'm';
	}
	else
	{
		error_exit("Invalid -c mode: '%c'.\n", cur_mode);
	}

	if (doall)
	{
		*allcolor = cur_mode;
		*curcolor = 'n';
	}
	else
	{
		*curcolor = cur_mode;
		*allcolor = 'n';
	}

	return n_pars_used;
}

int argv_add_stripper(char *mode, char *pars[], strip_t **pstrip, int *n_strip)
{
	int n_pars_used = 0;

	if (mode[2] == 'e' || mode[2] == 'S')
	{
		char *re = pars[n_pars_used++];

		*pstrip = myrealloc(*pstrip, (*n_strip + 1) * sizeof(strip_t));
		if (mode[2] == 'e')
			(*pstrip)[*n_strip].type = STRIP_TYPE_REGEXP;
		else if (mode[2] == 'S')
			(*pstrip)[*n_strip].type = STRIP_KEEP_SUBSTR;
		(*pstrip)[*n_strip].regex_str = mystrdup(re);
		(*pstrip)[*n_strip].del = NULL;
		compile_re(&(*pstrip)[*n_strip].regex, re);
		(*n_strip)++;
	}
	else if (mode[2] == 'r')
	{
		*pstrip = myrealloc(*pstrip, (*n_strip + 1) * sizeof(strip_t));
		(*pstrip)[*n_strip].type = STRIP_TYPE_RANGE;
		(*pstrip)[*n_strip].start = get_value_arg(mode, pars[n_pars_used++], VAL_ZERO_POSITIVE);
		(*pstrip)[*n_strip].end = get_value_arg(mode, pars[n_pars_used++], VAL_ZERO_POSITIVE);
		(*pstrip)[*n_strip].del = NULL;
		if ((*pstrip)[*n_strip].end <= (*pstrip)[*n_strip].start)
			error_exit("'-kr start end': end must be higher then start.\n");
		(*n_strip)++;
	}
	else if (mode[2] == 'c')
	{
		*pstrip = myrealloc(*pstrip, (*n_strip + 1) * sizeof(strip_t));
		(*pstrip)[*n_strip].type = STRIP_TYPE_COLUMN;
		(*pstrip)[*n_strip].del = mystrdup(pars[n_pars_used++]);
		(*pstrip)[*n_strip].col_nr = get_value_arg(mode, pars[n_pars_used++], VAL_ZERO_POSITIVE);
		(*n_strip)++;
	}
	else if (mode[2] == 's')
	{
		char *scheme = pars[n_pars_used++];
		int editscheme_index = find_editscheme(scheme);
		if (editscheme_index == -1)
			error_exit("-ks %s: scheme not found.\n", scheme);
		duplicate_es_array(pes[editscheme_index].strips, pes[editscheme_index].n_strips, pstrip, n_strip);
	}
	else
		error_exit("'%s' is not recognized.\n", mode);

	return n_pars_used;
}

void add_glob_check(char *check_glob, int check_interval, char merge, char new_only)
{
	cdg = (check_dir_glob *)myrealloc(cdg, sizeof(check_dir_glob) * (n_cdg + 1));

	cdg[n_cdg].glob_str       = mystrdup(check_glob);
	cdg[n_cdg].check_interval = check_interval;
	cdg[n_cdg].in_one_window  = merge;
	cdg[n_cdg].new_only       = new_only;
	cdg[n_cdg].window_nr      = -1;
	cdg[n_cdg].last_check     = (dtime_t)0.0;
	n_cdg++;
}

void do_commandline(int argc, char *argv[])
{
	int loop;
	char curcolor = 'n', allcolor = 'n';
	int field_index = 0;
	char *field_delimiter = NULL;
	char follow_filename = -1, retry = 0, invert_regex = 0;
	char retry_all = 0;
	int maxlines = -1;
	char setallmaxlines = 0;
	int restart = -1;
	char restart_clear = 0;
	char do_diff = 0;
	char cur_line_wrap = -1;
	int cur_line_wrap_offset = -1;
	char all_line_wrap = 0;
	char *label = NULL;
	re *pre = NULL;
	int n_re = 0;
	re *pre_all = NULL;
	int n_re_all = 0;
	int_array_t cur_color_schemes = { NULL, 0, 0 };
	myattr_t cdef = { -1, -1 };
	int window_height = -1;
	int initial_n_lines_tail = min_n_bufferlines;
	char *win_title = NULL;
	term_t cur_term_emul = TERM_IGNORE;
	strip_t *pstrip = NULL;
	int n_strip = 0;
	int n_redirect = 0;
	redirect_t *predir = NULL;
	mybool_t used_stdin = MY_FALSE;
	char merge_all = 0;
	char merge_in_new_first = 0;
	int close_idle = 0;
	char setallmaxbytes = 0;
	int maxbytes = -1;
	myattr_t alt_col_cdev1 = { -1, -1 }, alt_col_cdev2 = { -1, -1 };
	time_field_t new_only = 0;
	char no_repeat = 0;
	int mark_interval = 0;
	char syslog_noreverse = 0;
	char cont = 0;
	char marker_of_other_window = 0;
	char no_marker_of_other_window = 0;
	char bufferwhat = -1;
	int cur_beep_interval = -1;

	char do_add_timestamp = 0;
	int_array_t conversions = { NULL, 0, 0 };

	/* first, before we load the main configfile, see if we should load the global
	 * file or not
	 */
	for(loop=1; loop<argc; loop++)
	{
		if (strcmp(argv[loop], "--no-load-global-config") == 0)
		{
			load_global_config = 0;
			break;
		}
	}
	if (load_global_config)
		(void)load_configfile(NULL);
	/* set defaults from configfile */
	bufferwhat = default_bufferwhat;
	cur_line_wrap = default_linewrap;
	cur_line_wrap_offset = default_line_wrap_offset;
	follow_filename = default_follow_filename;

	/* parse commandline */
	for(loop=1; loop<argc; loop++)
	{
		if (strcmp(argv[loop], "-V") == 0)
		{
			version();

			exit(0);
		}
		else if (strcmp(argv[loop], "--no-load-global-config") == 0)
		{
			/* ignore */
		}
		else if (strcmp(argv[loop], "-bw") == 0)
		{
			char *what = argv[++loop];

			if (what)
			{
				if (what[0] == 'a' || what[0] == 'f')
					bufferwhat = what[0];
				else
					error_exit("-bw expects either 'a' or 'f' as parameter (got: '%c').\n", what[0]);
			}
			else
				error_exit("-bw expects a parameter ('a' or 'f').\n");
		}
		else if (strcmp(argv[loop], "--no-repeat") == 0)
		{
			no_repeat = 1;
		}
		else if (strcmp(argv[loop], "--cont") == 0)
		{
			cont = 1;
		}
		else if (strcmp(argv[loop], "--mark-interval") ==0)
		{
			mark_interval = get_value_arg("--mark-interval", argv[++loop], VAL_ZERO_POSITIVE);
		}
		else if (strcmp(argv[loop], "-ts") == 0)
		{
			do_add_timestamp = 1;
		}
		else if (strcmp(argv[loop], "--basename") == 0)
		{
			filename_only = 1;
		}
		else if (strcmp(argv[loop], "--mergeall") == 0)
		{
			merge_all = 1;
			merge_in_new_first = 0;
		}
		else if (strcmp(argv[loop], "--mergeall-new") == 0)
		{
			merge_all = 1;
			merge_in_new_first = 1;
		}
		else if (strcmp(argv[loop], "--no-mergeall") == 0)
		{
			merge_all = 0;
		}
		else if (strcmp(argv[loop], "--closeidle") == 0)
		{
			close_idle = get_value_arg("--closeidle", argv[++loop], VAL_ZERO_POSITIVE);
		}
		else if (strcmp(argv[loop], "-H") == 0)
		{
			++loop;
			if (argv[loop])
			{
				heartbeat_interval = atof(argv[loop]);
				if (heartbeat_interval < 0.0) error_exit("The value for -H must be >= 0.\n");
			}
			else
				error_exit("-H requires a parameter.\n");
		}
		else if (strcasecmp(argv[loop], "-a") == 0)
		{
			char mode = argv[loop][1];

			add_redir_to_file(mode, argv[++loop], &predir, &n_redirect);
		}
		else if (strcasecmp(argv[loop], "-g") == 0)
		{
			char mode = argv[loop][1];

			add_redir_to_proc(mode, argv[++loop], &predir, &n_redirect);
		}
		else if (argv[loop][0] == '-' && argv[loop][1] == 'U')
		{
			char *prio, *fac, *addr;
			char filtered;

			// -U[af][as] <facil> <prio> host[:port]
			if (argv[loop][2] != 'a' && argv[loop][2] != 'f')
				error_exit("-Ux where x needs to be either 'a' or 'f'");

			filtered = argv[loop][2] == 'f';
			prio = argv[++loop];
			fac  = argv[++loop];
			addr = argv[++loop];

			add_redir_to_socket(filtered, prio, fac, addr, &predir, &n_redirect);
		}
		else if (strcmp(argv[loop], "-F") == 0 || strcmp(argv[loop], "--config") == 0)
		{
			config_file = argv[++loop];
			if (file_exist(config_file) == -1)
				error_exit("Configuration file %s does not exist.\n", config_file);

			(void)do_load_config(-1, NULL, config_file);
		}
		else if (strcmp(argv[loop], "-Z") == 0)
		{
			markerline_attrs = parse_attributes(argv[++loop]);
		}
		else if (strcmp(argv[loop], "-T") == 0)
		{
			timestamp_in_markerline = 1;
		}
		else if (strcmp(argv[loop], "-S") == 0)
		{
			show_subwindow_id = 1;
		}
		else if (strcmp(argv[loop], "-t") == 0)
		{
			++loop;
			if (!argv[loop])
				error_exit("-t requires a parameter.\n");
			win_title = mystrdup(argv[loop]);
		}
		else if (strcmp(argv[loop], "-x") == 0)
		{
			++loop;
			if (!argv[loop])
				error_exit("-x requires a parameter.\n");
			set_title = mystrdup(argv[loop]);
		}
		else if (argv[loop][0] == '-' && toupper(argv[loop][1]) == 'P')
		{
			char *lw = argv[++loop];
			if (lw)
			{
				if (argv[loop - 1][1] == 'P')
					all_line_wrap = 1;
				else
					all_line_wrap = 0;
				cur_line_wrap = lw[0];
				if (cur_line_wrap == 'o')
					cur_line_wrap_offset = get_value_arg("-p", argv[++loop], VAL_ZERO_POSITIVE);
				else if (cur_line_wrap != 'a' && cur_line_wrap != 'l' && cur_line_wrap != 'r' && toupper(cur_line_wrap) != 'S' && cur_line_wrap != 'w')
					error_exit("Invalid mode for -p\n");
			}
			else
				error_exit("-p requires a parameter.\n");
		}
		else if (strcmp(argv[loop], "--retry") == 0)
		{
			retry = 1;
		}
		else if (strcmp(argv[loop], "--retry-all") == 0)
		{
			retry_all = 1;
			retry = 1;
		}
		else if (strcmp(argv[loop], "-n") == 0)
		{
			initial_n_lines_tail = get_value_arg("-n", argv[++loop], VAL_ZERO_POSITIVE);
		}
		else if (strcmp(argv[loop], "-N") == 0)
		{
			initial_n_lines_tail = min_n_bufferlines = get_value_arg("-n", argv[++loop], VAL_ZERO_POSITIVE);
		}
		else if (strcmp(argv[loop], "-b") == 0)
		{
			tab_width = get_value_arg("-b", argv[++loop], VAL_ZERO_POSITIVE);
		}
		else if (strcmp(argv[loop], "-u") == 0)
		{
			update_interval = get_value_arg("-u", argv[++loop], VAL_ZERO_POSITIVE);
		}
		else if (argv[loop][0] == '-' && toupper(argv[loop][1]) == 'R')
		{
			if (argv[loop][1] == 'R')
				do_diff = 1;

			if (argv[loop][2] == 'c')
				restart_clear = 1;

			restart = get_value_arg("-r/R", argv[++loop], VAL_ZERO_POSITIVE);
		}
		else if (strcmp(argv[loop], "-s") == 0)
		{
			split = get_value_arg("-s", argv[++loop], VAL_POSITIVE_NOT_1);
		}
		else if (strcmp(argv[loop], "-sw") == 0)
		{
			argv_set_window_widths(argv[++loop]);
		}
		else if (strcmp(argv[loop], "-sn") == 0)
		{
			argv_set_n_windows_per_column(argv[++loop]);
		}
		else if (strcmp(argv[loop], "-wh") == 0)
		{
			window_height = get_value_arg("-wh", argv[++loop], VAL_POSITIVE);
		}
		else if (strcmp(argv[loop], "-fr") == 0)
		{
			char *par = argv[++loop];
			for(;;)
			{
				int filter;
				char *komma = strchr(par, ',');
				if (komma)
					*komma = 0x00;

				filter = find_filterscheme(par);
				if (filter == -1)
					error_exit("'%s' is not a known filter scheme.\n", par);

				duplicate_re_array(pfs[filter].pre, pfs[filter].n_re, &pre, &n_re);

				if (!komma)
					break;

				par = komma + 1;
			}
		}
		else if (strcmp(argv[loop], "-cv") == 0)
		{
			char *par = argv[++loop];
			for(;;)
			{
				char *komma = strchr(par, ',');
				if (komma)
					*komma = 0x00;

				add_conversion_scheme(&conversions, par);

				if (!komma)
					break;

				par = komma + 1;
			}
		}
		else if (argv[loop][0] == '-' && toupper(argv[loop][1]) == 'E')
		{
			loop += argv_add_re(argv[loop], &argv[loop + 1], invert_regex, &pre, &n_re, &pre_all, &n_re_all);
		}
		else if (strcmp(argv[loop], "-v") == 0)
		{
			invert_regex = 1;
		}
		else if (strcmp(argv[loop], "-csn") == 0)
		{
			syslog_noreverse = 1;
		}
		else if (argv[loop][0] == '-' && toupper(argv[loop][1]) == 'C')
		{
			loop += argv_color_settings(argv[loop], &argv[loop + 1], &allcolor, &curcolor, &field_index, &field_delimiter, &cdef, &cur_term_emul, &cur_color_schemes, &alt_col_cdev1, &alt_col_cdev2);
		}
		else if (strcmp(argv[loop], "-f") == 0)
		{
			follow_filename = 1;
		}
		else if (strcmp(argv[loop], "--follow-all") == 0)
		{
			default_follow_filename = 1;
			follow_filename = 1;
		}
		else if (strcmp(argv[loop], "-w") == 0)
		{
			use_colors = 0;
		}
		else if (argv[loop][0] == '-' && argv[loop][1] == 'k')
		{
			loop += argv_add_stripper(argv[loop], &argv[loop + 1], &pstrip, &n_strip);
		}
		else if (strcasecmp(argv[loop], "-m") == 0)
		{
			if (argv[loop][1] == 'M')
				setallmaxlines = 1;
			maxlines = get_value_arg("-m/M", argv[++loop], VAL_ZERO_POSITIVE);
		}
		else if (strcasecmp(argv[loop], "-mb") == 0)
		{
			if (argv[loop][1] == 'M')
				setallmaxbytes = 1;
			maxbytes = get_value_arg("-m/M", argv[++loop], VAL_ZERO_POSITIVE);
		}
		else if (strcasecmp(argv[loop], "--label") == 0)
		{
			label = mystrdup(argv[++loop]);
		}
		else if (strcasecmp(argv[loop], "-i") == 0 || argv[loop][0] != '-' ||
				strcasecmp(argv[loop], "-l") == 0 ||
				strcasecmp(argv[loop], "-j") == 0 ||
				strcasecmp(argv[loop], "-iw") == 0 ||
				strcasecmp(argv[loop], "--listen") == 0)
		{
			struct stat64 buf;
			char *dummy;
			char is_cmd = 0;
			char is_sub = 0;
			char is_giw = 0;
			char is_sock = 0;
			char is_stdin = 0;
			int check_interval = 0;
			proginfo *cur;

			if (strcasecmp(argv[loop], "-l") == 0)
			{
				is_cmd = 1;
			}
			else if (strcasecmp(argv[loop], "-j") == 0)
			{
				if (used_stdin == MY_TRUE) error_exit("One can use %s only once.\n", argv[loop]);
				is_stdin = used_stdin = 1;
			}
			else if (strcasecmp(argv[loop], "-iw") == 0)
			{
				if (argv[loop + 2])
				{
					if ((argc - loop) > 2 && isdigit(argv[loop+2][0]))
					{
						is_giw = 1;
						check_interval = atoi(argv[loop + 2]);
					}
					else
					{
						check_interval = 5;
					}
				}
				else
					error_exit("-iw requires 2 parameters.\n");
			}
			else if (strcasecmp(argv[loop], "--listen") == 0)
			{
				is_sock = 1;
			}

			if (strcmp(argv[loop], "-L") == 0  ||
			    strcmp(argv[loop], "-I") == 0  ||
			    strcmp(argv[loop], "-Iw") == 0 ||
			    strcmp(argv[loop], "-J") == 0  ||
			    strcmp(argv[loop], "--Listen") == 0 ||
			    merge_all)
			{
				is_sub = 1;

				if (merge_all & (merge_in_new_first == 1))
				{
					is_sub = 0;
					merge_in_new_first = 0;
				}
			}

			if (argv[loop][0] == '-' && toupper(argv[loop][1]) != 'J')
			{
				loop++;
			}

			dummy = argv[loop];

			if (is_sub == 1 && nfd > 0)
			{
				cur = &pi[nfd - 1];

				while(cur -> next)
				{
					cur = cur -> next;
				}

				cur -> next = (proginfo *)mymalloc(sizeof(proginfo));

				cur = cur -> next;

				nsubwindows[nfd-1]++;
			}
			else
			{
				pi = (proginfo *)myrealloc(pi, (nfd + 1) * sizeof(proginfo));

				lb = (buffer *)myrealloc(lb, (nfd + 1) * sizeof(buffer));

				nsubwindows = (char *)myrealloc(nsubwindows, (nfd + 1) * sizeof(char));
				nsubwindows[nfd] = 1;
				memset(&lb[nfd], 0x00, sizeof(buffer));

				lb[nfd].maxnlines = maxlines;
				if (!setallmaxlines)
					maxlines = -1;
				lb[nfd].bufferwhat = bufferwhat;
				bufferwhat = default_bufferwhat;

				lb[nfd].maxbytes = maxbytes;
				if (!setallmaxbytes)
					maxbytes = -1;

				if (marker_of_other_window || lb[nfd].marker_of_other_window)
					lb[nfd].marker_of_other_window = 1;
				else if (no_marker_of_other_window == 1)
					lb[nfd].marker_of_other_window = -1; /* override global configfile setting */

				no_marker_of_other_window = marker_of_other_window = 0;

				cur = &pi[nfd];
				nfd++;
			}
			memset(cur, 0x00, sizeof(proginfo));

			/* see if file exists */
			if (check_interval == 0 && is_cmd == 0 && is_stdin == 0 && is_sock == 0 && retry == 0 && stat64(dummy, &buf) == -1)
			{
				fprintf(stderr, "Error opening file %s (%s)\n", dummy, strerror(errno));
				exit(EXIT_FAILURE);
			}

			/* init. struct. for this file */
			if (is_stdin == 0)
			{
				if (!dummy) error_exit("No filename given.\n");
				cur -> filename = mystrdup(dummy);
			}
			else
			{
				cur -> filename = mystrdup("STDIN");
			}
			if (is_cmd)
				cur -> wt = WT_COMMAND;
			else if (is_stdin)
				cur -> wt = WT_STDIN;
			else if (is_sock)
				cur -> wt = WT_SOCKET;
			else
				cur -> wt = WT_FILE;
			cur -> check_interval = check_interval;

			cur -> win_title = win_title;
			win_title = NULL;

			cur -> close_idle = close_idle;
			close_idle = 0;

			/* initial number of lines to tail */
			cur -> initial_n_lines_tail = initial_n_lines_tail;
			initial_n_lines_tail = min_n_bufferlines;

			/* default window height */
			cur -> win_height = window_height;
			window_height = -1;

			/* store regular expression(s) */
			cur -> pre = pre;
			cur -> n_re = n_re;
			pre = NULL;
			n_re = 0;
			if (n_re_all > 0)
				duplicate_re_array(pre_all, n_re_all, &cur -> pre, &cur -> n_re);

			/* hide this window? */
			cur -> hidden = 0;

			/* add timestamp in front of each line? */
			cur -> add_timestamp = do_add_timestamp;
			do_add_timestamp = 0;

			/* strip */
			cur -> pstrip = pstrip;
			cur -> n_strip = n_strip;
			pstrip = NULL;
			n_strip = 0;

			cur -> conversions = conversions;
			init_iat(&conversions);

			/* line wrap */
			cur -> line_wrap = cur_line_wrap;
			cur -> line_wrap_offset = cur_line_wrap_offset;
			if (!all_line_wrap)
			{
				cur_line_wrap = default_linewrap;
				cur_line_wrap_offset = default_line_wrap_offset;
			}

			cur -> retry_open = retry;
			cur -> follow_filename = follow_filename;
			follow_filename = default_follow_filename;

			cur -> cont = cont;
			cont = 0;

			/* 'watch' functionality configuration (more or less) */
			cur -> restart.restart = restart;
			cur -> restart.restart_clear = restart_clear;
			restart = -1; /* invalidate for next parameter */
			restart_clear = 0;
			cur -> restart.first = 1;
			cur -> restart.do_diff = do_diff;
			do_diff = 0;

			cur -> repeat.suppress_repeating_lines = no_repeat;

			cur -> mark_interval = mark_interval;

			/*  colors */
			cur -> cdef.attributes = cdef;
			cur -> cdef.alt_col_cdev1 = alt_col_cdev1;
			cur -> cdef.alt_col_cdev2 = alt_col_cdev2;
			cur -> cdef.syslog_noreverse = syslog_noreverse;
			if (curcolor != 'n' && curcolor != 0)
			{
				cur -> cdef.colorize = curcolor;
				cur -> cdef.color_schemes = cur_color_schemes;
				init_iat(&cur_color_schemes);
			}
			else
			{
				if (allcolor == 'n' && default_color_scheme != -1)
				{
					cur -> cdef.colorize = 'S';
					add_color_scheme(&cur -> cdef.color_schemes, default_color_scheme);
				}
				else if (allcolor == 'n')
				{
					cur -> cdef.colorize = 0;
				}
				else if (allcolor == 'S')
				{
					cur -> cdef.colorize = 'S';
					cur -> cdef.color_schemes = cur_color_schemes;
				}
				else
				{
					cur -> cdef.colorize = allcolor;
				}
			}
			cur -> cdef.field_nr = field_index;
			cur -> cdef.field_del = field_delimiter ? mystrdup(field_delimiter) : NULL;

			cur -> statistics.sccfirst = 1;

			curcolor = 'n';
			if (!retry_all)
				retry = 0;

			if (is_giw)	/* skip over the checkinterval */
				loop++;

			cur -> cdef.term_emul = cur_term_emul;
			cur_term_emul = TERM_IGNORE;

			/* redirect input also to a file or pipe */
			cur -> n_redirect = n_redirect;
			n_redirect = 0;
			cur -> predir = predir;
			predir = NULL;

			cur -> beep.beep_interval = cur_beep_interval;
			cur_beep_interval = -1;

			cur -> label = label;
			label = NULL;
		}
		else if (strcmp(argv[loop], "-du") == 0)
		{
			statusline_above_data = 1;
		}
		else if (strcmp(argv[loop], "-d") == 0)
		{
			mode_statusline = 0;
		}
		else if (strcmp(argv[loop], "-D") == 0)
		{
			mode_statusline = -1;
		}
		else if (strcmp(argv[loop], "-z") == 0)
		{
			warn_closed = 0;
		}
		else if (strcmp(argv[loop], "--new-only") == 0)
		{
			char *type = argv[++loop];

			if (type)
			{
				if (strcasecmp(type, "atime") == 0)
					new_only = TT_ATIME;
				else if (strcasecmp(type, "mtime") == 0)
					new_only = TT_MTIME;
				else if (strcasecmp(type, "ctime") == 0)
					new_only = TT_CTIME;
				else
					error_exit("--new-only requires either atime, mtime or ctime as parameter, not '%s'.\n", type);
			}
			else
				error_exit("--new-only requires a parameter.\n");
		}
		else if (strcasecmp(argv[loop], "-q") == 0)
		{
			int merge = argv[loop][1] == 'Q';
			int check_interval = get_value_arg("-q/-Q", argv[++loop], VAL_ZERO_POSITIVE);
			char *check_glob = argv[++loop];

			add_glob_check(check_glob, check_interval, merge, new_only);
		}
		else if (strcmp(argv[loop], "--mark-change") == 0)
		{
			marker_of_other_window = 1;
		}
		else if (strcmp(argv[loop], "--no-mark-change") == 0)
		{
			no_marker_of_other_window = 1;
		}
		else if (strcmp(argv[loop], "-o") == 0)
		{
			loop++;

			if (!argv[loop])
				error_exit("-o requires a parameter.\n");
			else
				config_file_entry(-1, argv[loop]);
		}
		else if (strcmp(argv[loop], "--beep-interval") == 0)
		{
			beep_interval = get_value_arg("--beep-interval", argv[++loop], VAL_POSITIVE);
		}
		else if (strcmp(argv[loop], "--bi") == 0)
		{
			cur_beep_interval = get_value_arg("--bi", argv[++loop], VAL_POSITIVE);
		}
		else
		{
			if (strcmp(argv[loop], "-h") != 0)
				fprintf(stderr, "unknown parameter '%s'\n", argv[loop]);

			usage();

			exit(1);
		}
	}

	for(loop=0; loop<n_re_all; loop++)
	{
		free_re(&pre_all[loop]);
	}
	myfree(pre_all);
}
