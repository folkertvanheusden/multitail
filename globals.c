#define _LARGEFILE64_SOURCE     /* required for GLIBC to enable stat64 and friends */
#include <regex.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mt.h"
#include "mem.h"

NEWWIN **splitlines = NULL;
NEWWIN *menu_win = NULL;
buffer *lb = NULL;
char *config_file = NULL;
char *mail_spool_file = NULL;
char *nsubwindows = NULL;
char *set_title = NULL;
char *shell = NULL;
char *tail = NULL;
char *ts_format = NULL;
char *statusline_ts_format = NULL;
char *syslog_ts_format = NULL;
char *cnv_ts_format = NULL;
char *line_ts_format = NULL;
char *window_number = NULL;
char *subwindow_number = NULL;
char **color_names = NULL;
char *replace_by_markerline = NULL;
const char *F1 = "For help at any time press F1.";
int *n_win_per_col = NULL;
int *vertical_split = NULL;
color_scheme *cschemes = NULL;
const char *version_str = " --*- multitail " VERSION " (C) 2003-2014 by folkert@vanheusden.com -*--";
conversion *conversions = NULL;
keybinding *keybindings = NULL;
pars_per_file *ppf = NULL;
proginfo *pi = NULL;
check_dir_glob *cdg = NULL;
int n_cdg = 0;
editscheme *pes = NULL;
int n_es = 0;
char *defaultcscheme = NULL;
proginfo *terminal_index = NULL;
char *global_find = NULL;

int terminal_main_index = -1;
int default_color_scheme = -1;
int max_y, max_x;
int min_n_bufferlines = -1;
int mode_statusline = 1;
int n_children = 0;
int n_conversions = 0;
int n_cschemes = 0;
int n_keybindings = 0;
int n_pars_per_file = 0;
int n_splitlines = 0;
int nfd = 0;
int update_interval = 0;
int n_colors_defined = 0;
int check_for_mail = 5;	/* check for mail every 5 seconds */
int default_maxnlines = 0;
int default_maxbytes = 0;
int popup_refresh_interval = 5; /* every 5 seconds */
int default_line_wrap_offset = 0;
int inverse_attrs = A_REVERSE;
int beep_interval = -1;
int linecounter_for_beep = 0;
int did_n_beeps = 0;
int default_min_shrink = 10;
int wordwrapmaxlength = 10;
int default_bg_color = -1;
int total_wakeups = 0;
int split = 0;
int syslog_port = 514;

double heartbeat_interval = 0.0;
double heartbeat_t = 0.0;
off64_t msf_prev_size = 0;

dtime_t msf_last_check = 0;
dtime_t mt_started;

pid_t children_list[MAX_N_SPAWNED_PROCESSES];
pid_t tail_proc = 0;	/* process used by checker-proc */
struct stat64 msf_info;
term_t term_type = TERM_IGNORE;
myattr_t markerline_attrs = { -1, -1 };
myattr_t changeline_attrs = { -1, -1 };
myattr_t idleline_attrs   = { -1, -1 };
myattr_t msgline_attrs    = { -1, -1 };
myattr_t statusline_attrs = { -1, -1 };
myattr_t splitline_attrs  = { -1, -1 };
colorpairs cp;
mode_t def_umask = 0022;
history_t search_h = { NULL, 0, NULL };
history_t cmdfile_h = { NULL, 0, NULL };
mybool_t re_case_insensitive = MY_FALSE;
chtype box_bottom_left_hand_corner=0;
chtype box_bottom_right_hand_corner=0;
chtype box_bottom_side=0;
chtype box_left_side=0;
chtype box_right_side=0;
chtype box_top_left_hand_corner=0;
chtype box_top_right_hand_corner=0;
chtype box_top_side=0;

beeb_t beep_method = BEEP_BEEP;
double beep_popup_length = 0.0;

char afs = 0;	/* abbreviate filesize */
char allow_8bit = 0; /* allow 8 bits ascii*/
char banner = 1;
char bright_colors = 0;
char do_refresh = 0;
char filename_only = 0;
char got_sigusr1 = 0;
char mail = 0;
char no_linewrap = 0;
char prev_term_char = -1;
char show_subwindow_id = 0;
char tab_width = 4;		/* some people use 8 */
char terminal_changed = 0;
char timestamp_in_markerline = 0;
char use_colors = 0;
char warn_closed = 1;
char caret_notation = 0;
char statusline_above_data = 0;
char global_marker_of_other_window = 0;
char markerline_char = '-';
char changeline_char = '-';
char idleline_char   = '-';
char msgline_char   = '-';
char default_bufferwhat = 'f';
char load_global_config = 1;
char default_linewrap = 'a';
char default_follow_filename = 0;
char do_not_close_closed_windows = 0;
char suppress_empty_lines = 1;
char splitline_mode = SL_REGULAR;
char abort_key = 7;
char exit_key = 3;
char default_sb_showwinnr = 0;
char reuse_searchstring = 1;
char need_died_procs_check = 0;
char scrollback_no_colors = 0;
char scrollback_search_new_window = 1;
mybool_t posix_tail = 0;
mybool_t resolv_ip_addresses = 1;
mybool_t show_severity_facility = 1;
mybool_t gnu_tail = 0;

regex_t global_highlight_re;
char *global_highlight_str = NULL;

filterscheme *pfs = NULL;
int n_fs = 0;

char *sigs[] = { NULL, "SIGHUP", "SIGINT", "SIGQUIT", "SIGILL", "SIGTRAP", "SIGABRT/SIGIOT", "SIGIOT", "SIGBUS", "SIGFPE", "SIGKILL", "SIGUSR1", "SIGSEGV", "SIGUSR2", "SIGPIPE", "SIGALRM", "SIGTERM", "SIGSTKFLT", "SIGCHLD", "SIGCONT", "SIGSTOP", "SIGTSTP", "SIGTTIN", "SIGTTOU", "SIGURG", "SIGXCPU", "SIGXFSZ", "SIGVTALRM", "SIGPROF", "SIGWINCH", "SIGIO/SIGPOLL", "SIGPWR", "SIGSYS/SIGUNUSED" };
int n_known_sigs = 32;
char *severities[8] = { "emerg", "alert", "crit", "err", "warning", "notice", "info", "debug" };
char *facilities[24] = { "kern", "user", "mail", "daemon", "auth", "syslog", "lpr", "news", "uucp", "cron", "authpriv", "ftp", "?", "?", "?", "?", "local0", "local1", "local2", "local3", "local4", "local5", "local6", "local7" };

void set_do_refresh(char val)
{
	do_refresh = val;
}

char get_do_refresh()
{
	return do_refresh;
}
