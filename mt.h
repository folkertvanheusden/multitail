#ifndef __MT_H__
#define __MT_H__

#define _LARGEFILE64_SOURCE	/* required for GLIBC to enable stat64 and friends */
#include <regex.h>
#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define WAIT_FOR_FILE_DELAY	(250)	/* sleep when waiting for a file to become available */
#define MAX_N_RE_MATCHES	(80)	/* max. number of regex matches: used for coloring matches */
#define MIN_N_BUFFERLINES	(100)	/* number of lines to buffer at minimum */
#define MAX_N_SPAWNED_PROCESSES	(16)	/* max. nr. of processes executed by matching regexps */
#define MAX_N_COLUMNS		(15)	/* max number of columns */
#define DEFAULT_TAB_WIDTH	(4)
#define DEFAULT_COLORPAIRS	(8)
#define MAX_BACKTRACE_LENGTH	(256)
#define SCRIPT_IO_BUFFER_SIZE	(4096)
#define CONFIG_READ_BUFFER	(4096)
#define HISTORY_IO_BUFFER	(4096)
#define TIMESTAMP_EXTEND_BUFFER	(1024)

#define SL_REGULAR	(1)
#define SL_NONE		(0)
#define SL_ATTR		(2)

#define LOADAVG_STR_LEN		(20)
#define AMOUNT_STR_LEN		16

#ifndef __GNUC__
        #define __PRETTY_FUNCTION__	"(unknown)"
	#define USE_IF_SET(x, y)		((x)?(x):(y))
#else
	#define USE_IF_SET(x, y)		((x)?:(y))
#endif

typedef enum { MY_FALSE = 0, MY_TRUE = 1 } mybool_t;

typedef enum { VAL_ZERO_POSITIVE = 1, VAL_POSITIVE_NOT_1, VAL_POSITIVE } valcheck_t;

#define M_KB (1024)
#define M_MB (M_KB * 1024)
#define M_GB (M_MB * 1024)

typedef enum { BEEP_FLASH = 1, BEEP_BEEP, BEEP_POPUP, BEEP_NONE } beeb_t;

typedef enum { LINE_LEFT = 1, LINE_RIGHT, LINE_TOP, LINE_BOTTOM } linepos_t;

typedef enum { TERM_IGNORE = 0, TERM_XTERM, TERM_ANSI /* or vt100 */} term_t;

typedef enum { SCHEME_TYPE_EDIT = 0, SCHEME_TYPE_FILTER } filter_edit_scheme_t;

#ifndef _BSD_SOURCE
#define _BSD_SOURCE	/* don't worry: it'll still work if you don't have a BSD system */
#endif
#ifndef __USE_BSD
#define __USE_BSD	/* manpage says _BSD_SOURCE, stdlib.h says __USE_BSD */
#endif

#if defined(UTF8_SUPPORT) && !defined(__APPLE__)
        #if defined(__FreeBSD__) || defined (__linux__)
                #include <panel.h>
                #include <curses.h>
        #else
                #include <ncursesw/panel.h>
                #include <ncursesw/ncurses.h>
        #endif
#else
	#if defined(__APPLE__)
        #include <ncurses.h>
        #include <panel.h>
    #elif defined(sun) || defined(__sun) || defined(scoos) || defined(_HPUX_SOURCE) || defined(AIX) || defined(__CYGWIN__)
		#include <ncurses/panel.h>
		#include <ncurses/ncurses.h>
	#else
		#include <panel.h>
		#include <ncurses.h>
	#endif
#endif

/* it seems the default HP-UX c-compiler doesn't understand atoll and
 * strtoll while it does understand 'long long'
 */
#if defined(_HPUX_SOURCE) || defined(__APPLE__)
	#ifndef atoll
		#define atoll(x)	atol(x)
	#endif
#endif

#ifndef strtoll
	#define strtoll(x, y, z)	strtol(x, y, z)
#endif
#ifndef atoll
      #define atoll(a) strtoll((a), (char **)NULL, 10)
#endif

/* Tru64 workaround */
#if defined(OSF1)
	#undef getmaxyx
	#define getmaxyx(w,y,x) y = w->_maxy;  x = w->_maxx
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(__CYGWIN__) || defined(__minix)
#define off64_t off_t
#define stat64 stat
#define open64 open
#endif

#define MARKER_REGULAR	(NULL)
#define MARKER_CHANGE	((proginfo *)-1)
#define MARKER_IDLE	((proginfo *)-2)
#define MARKER_MSG	((proginfo *)-3)
#define IS_MARKERLINE(x)	((x) == MARKER_REGULAR || (x) == MARKER_CHANGE || (x) == MARKER_IDLE || (x) == MARKER_MSG)

typedef enum { SEL_WIN = 1, SEL_SUBWIN, SEL_FILES, SEL_CSCHEME, SEL_HISTORY } selbox_type_t;

typedef enum { TT_ATIME = 1, TT_MTIME, TT_CTIME } time_field_t;

typedef double dtime_t;

typedef struct
{
	char *history_file;
	int history_size;

	char **history;
} history_t;

typedef struct
{
	int *elements;
	int n;
	int size;
} int_array_t;

typedef struct
{
	char        *glob_str;
	int          check_interval;
	dtime_t      last_check;
	time_field_t new_only;
	const char  *color_scheme;

	const char  *label;

	char in_one_window;
	int window_nr;	/* if 'in_one_window' is set, merge into the window 'window_nr' */
} check_dir_glob;

typedef struct
{
	WINDOW *win;
	PANEL *pwin;

	int x_off, y_off;
	int width, height;

} NEWWIN;

typedef struct
{
        char *regex_str;
        regex_t regex;
        char invert_regex;
	char use_regex;

	int match_count;

	/* command to run if matches */
	char *cmd;
} re;

typedef struct
{
	char *fs_name;
	char *fs_desc;
	int n_re;
	re *pre;
} filterscheme;

typedef enum { STRIP_TYPE_REGEXP = 1, STRIP_TYPE_RANGE, STRIP_TYPE_COLUMN, STRIP_KEEP_SUBSTR } striptype_t;

typedef struct
{
	striptype_t type;
	
	regex_t  regex;
	char *regex_str;

	int start, end;

	int col_nr;
	char *del;
	
	int match_count;
} strip_t;

typedef struct
{
	char *es_name;
	char *es_desc;

	int n_strips;
	strip_t *strips;
} editscheme;

#define MAX_COLORS_PER_LINE (80)

typedef struct
{
	int *fg_color;
	int *bg_color;
	int size;		/* COLOR_PAIRS */
	int n_def;		/* n defined, at least 1 as color_pair(0) is the default
				 * terminal colors (white on black mostly) which also
				 * cannot be changed
				 */
} colorpairs;

typedef struct
{
	int colorpair_index;
	int attrs;
} myattr_t;

typedef enum { REDIRECTTO_NONE = 0, REDIRECTTO_PIPE_FILTERED = 1, REDIRECTTO_PIPE, REDIRECTTO_FILE_FILTERED, REDIRECTTO_FILE, REDIRECTTO_SOCKET_FILTERED, REDIRECTTO_SOCKET } redirecttype_t;

typedef struct {
	char *redirect;
	redirecttype_t type;
	int fd;
	pid_t pid;
	struct sockaddr_in sai;
	int prio_fac;
} redirect_t;

typedef struct
{	
	dtime_t lastevent;
	double prev_deltat, total_deltat;
	double med;
	double dev;
	char sccfirst;
	double scc, sccu0, scclast, scct1, scct2, scct3;
	int n_events;

	dtime_t start_ts;
	long long int bytes_processed;
} statistics_t;

typedef struct
{
	char colorize;
	char field_nr;
	char *field_del;
	int_array_t color_schemes;
	myattr_t attributes;
	char alt_col;
	myattr_t alt_col_cdev1, alt_col_cdev2;
	char syslog_noreverse;

	term_t term_emul;
} cdef_t;

typedef struct
{
	char **bcur, **bprev;
	int ncur, nprev;
} diff_t;

typedef struct
{
	int restart;
	char restart_clear; /* clear after each iteration? */
	char is_restarted;
	char first;
	char do_diff;
	diff_t diff;
} restart_t;

typedef struct
{
	char suppress_repeating_lines;
	char *last_line;
	int n_times_repeated;
} repeatinglines_t;

typedef enum { WT_FILE=1, WT_COMMAND, WT_STDIN, WT_SOCKET } windowtype_t;

typedef struct _subwindow_
{
	char *filename;
	windowtype_t wt;
	int last_exit_rc;
	int n_runs;
	int check_interval;
	int fd;		/* read */
	int wfd;	/* write */
	pid_t pid;

	int n_redirect;
	redirect_t *predir;

	off64_t last_size;

	char cont;		/* "re-connect" lines with \ at the end */

	char add_timestamp;

	char *label;		/* put in front of each line */

	int_array_t conversions;

	char paused;
	char closed;

	char *incomplete_line;

	char *win_title;

	char line_wrap;
	int line_wrap_offset;

	int win_height;

	/* repeatingly start a program */
	restart_t restart;

	int initial_n_lines_tail;

	int mark_interval;

	repeatinglines_t repeat;

	cdef_t cdef;

	char hidden;
	char follow_filename;
	char retry_open;

	int close_idle;

	statistics_t statistics;

	struct
	{
		int beep_interval;
		int linecounter_for_beep;
		int did_n_beeps;
	} beep;

	NEWWIN *status;
	NEWWIN *data;

	int n_re;
	re *pre;

	int n_strip;
	strip_t *pstrip;

	struct _subwindow_ *next;
} proginfo;

typedef struct
{
	char *Bline;
	proginfo *pi;
	double ts;
} buffered_entry;

typedef struct
{
	buffered_entry *be;
	int curpos;
	char bufferwhat;
	int maxnlines;
	int maxbytes, curbytes;

	proginfo *last_win;
	char marker_of_other_window;
} buffer;

typedef enum {
	CSREFLAG_SUB = 1,		/* substring matching */
	CSREFLAG_CMP_VAL_LESS,		/* compare with value: value less then what is configured? */
	CSREFLAG_CMP_VAL_BIGGER,	/* compare with value: value higher then what is configured? */
	CSREFLAG_CMP_VAL_EQUAL		/* compare with value: value equal to what is configured? */
} csreflag_t;

typedef struct
{
	char *script;
	pid_t pid;
	int fd_r, fd_w;
} script;

typedef struct
{
	myattr_t attrs1;
	myattr_t attrs2;
	mybool_t use_alternating_colors;
	int ac_index;
} cse_main;

typedef struct
{
	cse_main cdef;

	regex_t regex;
	csreflag_t flags;
	double cmp_value;

	mybool_t merge_color;

} color_scheme_entry;

typedef struct
{
	char *name;
	char *descr;

	script color_script;

	int n;
	color_scheme_entry *pentries;
} color_scheme;

typedef enum { CONVTYPE_IP4TOHOST = 1, CONVTYPE_EPOCHTODATE, CONVTYPE_ERRNO, CONVTYPE_HEXTODEC, CONVTYPE_DECTOHEX, CONVTYPE_TAI64NTODATE, CONVTYPE_SCRIPT, CONVTYPE_ABBRTOK, CONVTYPE_SIGNRTOSTRING } conversion_t;
typedef struct
{
	conversion_t type;
	regex_t regex;
	int match_count;
} conversion_bundle_t;
typedef struct
{
	char *name;

	int n;
	conversion_bundle_t *pcb;

	/* conversion script */
	script *pcs;
} conversion;

typedef enum { SCHEME_CONVERSION = 1, SCHEME_COLOR } scheme_t;

typedef struct cv_off
{
	int start, end;
	char *newstr;
} cv_off;

typedef struct
{
	int n_colorschemes;
	char **colorschemes;

	int buffer_maxnlines;
	int buffer_bytes;

	char change_win_marker;

	regex_t regex;
	char *re_str;

	int_array_t filterschemes;

	int_array_t editschemes;

	int n_conversion_schemes;
	char **conversion_schemes;
} pars_per_file;

typedef struct
{
	regoff_t start;
	regoff_t end;

	myattr_t attrs;
	mybool_t merge_color;
} color_offset_in_line;

typedef struct
{
	char key;
	char *command;
} keybinding;

void do_exit(void);
char * select_file(char *input, int what_help);
char check_no_suppress_lines_filter(proginfo *cur);
void color_print(int f_index, NEWWIN *win, proginfo *cur, char *string, regmatch_t *matches, int matching_regex, mybool_t force_to_winwidth, int start_at_offset, int end_at_offset, double ts, char show_window_nr);
int select_window(int what_help, char *heading);
char check_filter(proginfo *cur, char *string, regmatch_t **pmatch, char **error, int *matching_regex, char do_re, char *display);
int wait_for_keypress(int what_help, double max_wait, NEWWIN *popup, char shift_cursor);
int toggle_colors(void);
void regexp_error_popup(int rc, regex_t *pre);
void redraw_statuslines(void);
void buffer_replace_pi_pointers(int f_index, proginfo *org, proginfo *new);
char delete_entry(int f_index, proginfo *sub);
char * key_to_keybinding(char what);
void do_buffer(int f_index, proginfo *cur, char *string, char filter_match, double now);
void do_print(int f_index, proginfo *cur, char *string, regmatch_t *matches, int matching_regex, double ts);
void do_set_bufferstart(int f_index, char store_what_lines, int maxnlines);
void emit_myattr_t(FILE *fh, myattr_t what);
void update_statusline(NEWWIN *status, int win_nr, proginfo *cur);
void write_escape_str(FILE *fh, char *string);
void check_proc_sigh(int sig);
void info(void);
void statistics(void);
int emit_to_buffer_and_term(int f_index, proginfo *cur, char *line);
void add_pars_per_file(char *re, char *colorscheme, int n_buffer_lines, int buffer_bytes, char change_win_marker, int fs, int es, char *conversion_scheme);
void version(void);
void usage(void);
void create_new_win(proginfo **cur, int *nr);
void delete_be_in_buffer(buffer *pb);
void do_restart_window(proginfo *cur);

void LOG(char *str, ...);

#endif
