#define _LARGEFILE64_SOURCE	/* required for GLIBC to enable stat64 and friends */
#include "doassert.h"
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <regex.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <glob.h>
#ifdef _POSIX_PRIORITY_SCHEDULING
#include <sched.h>
#endif
#include <pwd.h>
#include <sys/wait.h>
#include <fcntl.h>
#if defined(sun) || defined(__sun)
#include <sys/loadavg.h>
#endif
#include <dirent.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "version.h"
#include "error.h"
#include "mem.h"
#include "mt.h"
#include "term.h"
#include "globals.h"
#include "utils.h"


int find_path_max(void)
{
#ifdef PATH_MAX
	int path_max = PATH_MAX;
#else
	int path_max = pathconf("/", _PC_PATH_MAX);
	if (path_max <= 0)
	{
		if (errno) error_exit("pathconf() failed\n");

		path_max = 4096;
	}
	else
		path_max++; /* since its relative to root */
#endif
	if (path_max > 4096)
		path_max = 4096;

	return path_max;
}

int myrand(int max)
{
	return (int)(((double)max * (double)rand()) / (double)RAND_MAX);
}

ssize_t WRITE(int fd, char *whereto, size_t len, char *for_whom)
{
	ssize_t cnt=0;

	while(len>0)
	{
		ssize_t rc;

		rc = write(fd, whereto, len);

		if (rc == -1)
		{
			if (errno != EINTR && errno != EAGAIN)
				error_exit("Problem writing to file descriptor while processing %s.\n", for_whom);
		}
		else if (rc == 0)
		{
			break;
		}
		else
		{
			whereto += rc;
			len -= rc;
			cnt += rc;
		}
	}

	return cnt;
}

void get_load_values(double *v1, double *v2, double *v3)
{
#if !defined(__UCLIBC__) && (defined(__FreeBSD__) || defined(linux) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(__GNU__) || defined(__sun) || defined(sun))
#if defined(__GLIBC__) && ( __GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 2))
	/* Older glibc doesn't have getloadavg() - use sysinfo() */
	/* thanks to Ville Herva for this code! */
	double scale = 1 << SI_LOAD_SHIFT;
	struct sysinfo si;

	if (sysinfo(&si) == -1)
	{
		/* let's exit: if these kind of system-
		 * calls start to fail, something must be
		 * really wrong
		 */
		error_exit("sysinfo() failed\n");
	}

	*v1 = (double)si.loads[0] / scale;
	*v2 = (double)si.loads[1] / scale;
	*v3 = (double)si.loads[2] / scale;
#else
	double loadavg[3];
	if (getloadavg(loadavg, 3) == -1)
	{
		/* see comment on sysinfo() */
		error_exit("getloadavg() failed\n");
	}
	*v1 = loadavg[0];
	*v2 = loadavg[1];
	*v3 = loadavg[2];
#endif
#else
	*v1 = *v2 = *v3 = -1.0;
#endif
}

int get_vmsize(pid_t pid)
{
	int vmsize = -1;
#if defined(linux)
	FILE *fh;
	int path_max = find_path_max();
	char *path = mymalloc(path_max);

	assert(pid > 1);

	snprintf(path, path_max, "/proc/%d/stat", pid);

	fh = fopen(path, "r");
	if (fh)
	{
		char *dummystr = mymalloc(path_max);
		char dummychar;
		int dummy;

		if (fscanf(fh, "%d %s %c %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", &dummy, dummystr, &dummychar, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &vmsize) != 23)
			vmsize = -1;

		fclose(fh);

		myfree(dummystr);
	}

	myfree(path);
#endif

	return vmsize;
}

int mykillpg(pid_t pid, int sigtype)
{
#ifdef AIX
	return kill(pid, sigtype);
#else
	return killpg(pid, sigtype);
#endif
}

/** stop_process
 * - in:      int pid  pid of process
 * - returns: nothing
 * this function sends a TERM-signal to the given process, sleeps for 1009 microseconds
 * and then sends a KILL-signal to the given process if it still exists. the TERM signal
 * is send so the process gets the possibility to gracefully exit. if it doesn't do that
 * in 100 microseconds, it is terminated
 */
void stop_process(pid_t pid)
{
	assert(pid > 1);

	if (mykillpg(pid, SIGTERM) == -1)
	{
		if (errno != ESRCH)
			error_exit("Problem stopping child process with PID %d (SIGTERM).\n", pid);
	}

	usleep(1000);

	/* process still exists? */
	if (mykillpg(pid, SIGTERM) == 0)
	{
		/* sleep for a millisecond... */
		usleep(1000);

		/* ...and then really terminate the process */
		if (mykillpg(pid, SIGKILL) == -1)
		{
			if (errno != ESRCH)
				error_exit("Problem stopping child process with PID %d (SIGKILL).\n", pid);
		}
	}
	else if (errno != ESRCH)
		error_exit("Problem stopping child process with PID %d (SIGTERM).\n", pid);

	/* wait for the last remainder of the died process to go away,
	 * otherwhise we'll find zombies on our way
	 */
	if (waitpid(pid, NULL, WNOHANG | WUNTRACED) == -1)
	{
		if (errno != ECHILD)
			error_exit("waitpid() failed\n");
	}
}

/** delete_array
 * - in:      char **list array of strings to free
 *            int n       number of strings in this array
 * - returns: nothing
 * this function frees an array of strings: all strings are freed and
 * also the pointer-list itself is freed
 */
void delete_array(char **list, int n)
{
	int loop;

	assert(n >= 0);

	for(loop=n-1; loop>=0; loop--)
		myfree(list[loop]);

	myfree(list);
}

int find_char_offset(char *str, char what)
{
	char *pnt;

	assert(what > 0);

	pnt = strchr(str, what);
	if (!pnt)
		return -1;

	return (int)(pnt - str);
}

int file_info(char *filename, off64_t *file_size, time_field_t tft, time_t *ts, mode_t *mode)
{
	struct stat64 buf;

	if (stat64(filename, &buf) == -1)
	{
		if (errno != ENOENT)
			error_exit("Error while obtaining details of file %s.\n", filename);

		return -1;
	}

	if (file_size) *file_size = buf.st_size;

	if (ts)
	{
		if (tft == TT_ATIME)
			*ts = buf.st_atime;
		else if (tft == TT_MTIME)
			*ts = buf.st_mtime;
		else if (tft == TT_CTIME)
			*ts = buf.st_ctime;
		else
		{
			assert(tft == 0);
		}
	}

	if (mode) *mode = buf.st_mode;

	return 0;
}

int file_exist(char *filename)
{
	struct stat buf;
	int rc = stat(filename, &buf);

	if (rc == -1 && errno != ENOENT)
		error_exit("stat() on file %s failed.\n", filename);

	return rc;
}

char * convert_regexp_error(int error, const regex_t *preg)
{
	/* errors are specified not to be longer then 256 characters */
	char *multitail_string = "MultiTail warning: regular expression failed, reason: ";
	int len = strlen(multitail_string);
	char *error_out = NULL;
	const int max_err_len = 256;

	assert(error != 0);

	if (error != REG_NOMATCH)
	{
		error_out = (char *)mymalloc(max_err_len + len + 1);

		memcpy(error_out, multitail_string, len);

		/* convert regexp error */
		regerror(error, preg, &error_out[len], max_err_len);
	}

	return error_out;
}

char * amount_to_str(long long int amount)
{
	char *out = mymalloc(AMOUNT_STR_LEN);	/* ...XB\0 */

	assert(amount >= 0);

	if (amount >= M_GB)	/* GB */
		snprintf(out, AMOUNT_STR_LEN, "%dGB", (int)((amount + M_GB - 1) / M_GB));
	else if (amount >= M_MB)	/* MB */
		snprintf(out, AMOUNT_STR_LEN, "%dMB", (int)((amount + M_MB - 1) / M_MB));
	else if (amount >= M_KB)	/* KB */
		snprintf(out, AMOUNT_STR_LEN, "%dKB", (int)((amount + M_KB - 1) / M_KB));
	else
		snprintf(out, AMOUNT_STR_LEN, "%d", (int)(amount));

	return out;
}

struct passwd *getuserinfo(void)
{
	struct passwd *pp = getpwuid(geteuid());
	if (!pp)
		error_exit("Failed to get passwd-structure for effective user id %d.\n", geteuid());

	return pp;
}

char * getusername(void)
{
	static char username[128] = { 0 };

	if (!username[0])
	{
		strncpy(username, getuserinfo() -> pw_name, sizeof(username));
		username[sizeof(username) - 1] = 0x00;
	}

	return username;
}

/* these are there because AIX/IRIX can return EINTR for those */
int myopen(char *path, int mode)
{
	int fd;

	for(;;)
	{
		fd = open64(path, mode);
		if (fd == -1)
		{
			if (errno == EINTR || errno == EAGAIN) /* for AIX */
				continue;

			return fd;
		}

		break;
	}

	return fd;
}

int myclose(int fd)
{
	for(;;)
	{
		if (close(fd) == -1)
		{
			if (errno == EINTR || errno == EAGAIN) /* for AIX */
				continue;

			return -1;
		}

		return 0;
	}
}

char * shorten_filename(char *in, int max_len)
{
	static char buffer[4096];
	int len = strlen(in);
	int cutlen, dummy;

	assert(max_len >= 0);

	if (len <= max_len)
		return in;

	cutlen = (max_len - 3) / 2;

	memcpy(buffer, in, cutlen);
	memcpy(&buffer[cutlen], "...", 3);

	dummy = max_len - (cutlen + 3);
	memcpy(&buffer[cutlen + 3], &in[len - dummy], dummy + 1);

	return buffer;
}

double get_ts(void)
{
	struct timeval ts;
	struct timezone tz;

	if (gettimeofday(&ts, &tz) == -1)
		error_exit("gettimeofday() failed");

	return (((double)ts.tv_sec) + ((double)ts.tv_usec)/1000000.0);
}

int match_files(char *search_for, char **path, char ***found, char **isdir)
{
	DIR *dir;
	struct dirent *entry;
	char *cur_dir = mymalloc(find_path_max() + 1);
	char *fname;
	char **list = NULL;
	int nfound = 0;
	size_t fname_size;
	int path_len;
	int s1, s2;
	char *slash = strrchr(search_for, '/');
	if (slash)
	{
		fname = mystrdup(slash + 1);
		*(slash + 1) = 0x00;
		*path = mystrdup(search_for);
	}
	else
	{
		*path = mystrdup("./");
		fname = mystrdup(search_for);
	}
	fname_size = strlen(fname);
	path_len = strlen(*path);

	dir = opendir(*path);
	if (!dir)
	{
		free(cur_dir);
		return 0;
	}

	memcpy(cur_dir, *path, path_len + 1);

	while((entry = readdir(dir)) != NULL)
	{
		if ((fname_size == 0 || strncmp(entry -> d_name, fname, fname_size) == 0) && strcmp(entry -> d_name, ".") != 0 &&
				strcmp(entry -> d_name, "..") != 0)
		{
			struct stat finfo;

			/* get filename */
			list = (char **)myrealloc(list, (nfound + 1) * sizeof(char *));
			list[nfound] = mystrdup(entry -> d_name);

			/* check if the file is a directory */
			*isdir = (char *)myrealloc(*isdir, (nfound + 1) * sizeof(char));
			strncpy(&cur_dir[path_len], entry -> d_name, max(0,find_path_max() - path_len));
			if (stat(cur_dir, &finfo) == -1)
			{
				if (errno != ENOENT)	/* file did not disappear? then something is very wrong */
					error_exit("Error while invoking stat() %s.\n", cur_dir);
			}
			(*isdir)[nfound] = S_ISDIR(finfo.st_mode)?1:0;

			nfound++;
		}
	}

	if (closedir(dir) == -1)
		error_exit("closedir() failed\n");

	/*	qsort( (void *)list, (size_t)nfound, sizeof(char *), compare_filenames); */
	for(s1=0; s1<(nfound - 1); s1++)
	{
		for(s2=s1+1; s2<nfound; s2++)
		{
			if (strcasecmp(list[s2], list[s1]) < 0)
			{
				char *fdummy = list[s1], ddummy = (*isdir)[s1];

				list[s1] = list[s2];
				(*isdir)[s1] = (*isdir)[s2];
				list[s2] = fdummy;
				(*isdir)[s2] = ddummy;
			}
		}
	}

	*found = list;

	myfree(fname);
	myfree(cur_dir);

	return nfound;
}

void setup_for_childproc(int fd, char close_fd_0, char *term)
{
	int term_len = strlen(term) + 5;
	char *dummy = (char *)mymalloc(term_len + 1);

	if (close_fd_0) if (-1 == myclose(0)) error_exit("close() failed\n");
	if (-1 == myclose(1)) error_exit("close() failed\n");
	if (-1 == myclose(2)) error_exit("close() failed\n");
	if (close_fd_0) if (-1 == mydup(fd)) error_exit("dup() failed\n");
	if (-1 == mydup(fd)) error_exit("dup() failed\n");
	if (-1 == mydup(fd)) error_exit("dup() failed\n");

	/*
	 * not doing this: it also clears the 'PATH'
	 * I could make first do a getenv for PATH and then put it
	 * back after the clearenv, but what would be the point of
	 * doing the clearenv in the first place?
	 *
	 *	if (clearenv() == -1)
	 *	{
	 *		fprintf(stderr, "WARNING: could not clear environment variables: %s (%d)\n", strerror(errno), errno);
	 *		exit(1);
	 *	}
	 */

	/* set terminal */
	assert(term != NULL);
	snprintf(dummy, term_len, "TERM=%s", term);
	if (putenv(dummy) == -1)
		fprintf(stderr, "setup_for_childproc: Could not set TERM environment-variable (%s): %s (%d)\n", dummy, strerror(errno), errno);

	(void)umask(007);
}

int open_null(void)
{
	int fd = myopen("/dev/null", O_RDWR);
	if (fd == -1)
		error_exit("Failed to open file /dev/null.\n");

	return fd;
}

void free_re(re *cur_re)
{
	if (cur_re)
	{	
		myfree(cur_re -> regex_str);
		if (cur_re -> use_regex)
			regfree(&cur_re -> regex);

		myfree(cur_re -> cmd);
	}
}

char * find_most_recent_file(char *filespec, char *cur_file)
{
	glob_t files;
	char *selected_file = NULL;
	unsigned int loop;
	time_t prev_ts = (time_t)0;

	/* get timestamp of previous file */
	if (cur_file)
	{
		if (file_info(cur_file, NULL, TT_MTIME, &prev_ts, NULL) == -1)
		{
			prev_ts = (time_t)0;
		}
	}

	/* get list of files that match */
#if defined(__APPLE__) || defined(__CYGWIN__)
	if (glob(filespec, GLOB_ERR | GLOB_NOSORT, NULL, &files) != 0)
#else
	if (glob(filespec, GLOB_ERR | GLOB_NOSORT | GLOB_NOESCAPE, NULL, &files) != 0)
#endif
	{
		return NULL;
	}

	/* see if any of them is more recent than the current one */
	for(loop=0; loop<files.gl_pathc; loop++)
	{
		time_t new_ts;

		/* current file? then skip it */
		if (cur_file != NULL && strcmp(cur_file, files.gl_pathv[loop]) == 0)
			continue;

		/* get the modificationtime of this found file */
		if (file_info(files.gl_pathv[loop], NULL, TT_MTIME, &new_ts, NULL) == -1)
		{
			new_ts = (time_t)0;
		}

		/* more recent? */
		if (new_ts > prev_ts)
		{
			selected_file = files.gl_pathv[loop];
			prev_ts = new_ts;
		}
	}

	/* found a file? then remember the filename */
	if (selected_file != NULL)
	{
		selected_file = mystrdup(selected_file);
	}

	globfree(&files);

	return selected_file;
}

char zerotomin(char c)
{
	if (c == 0)
		return 'n';
	if (c == 1)
		return 'y';

	return c;
}

char *find_next_par(char *start)
{
	char *dummy = strchr(start, ':');
	if (dummy)
	{
		*dummy = 0x00;
		dummy++;
	}

	return dummy;
}

int mydup(int old_fd)
{
	int new_fd = -1;

	for(;;)
	{
		new_fd = dup(old_fd);

		if (new_fd == -1)
		{
			if (errno == EINTR)
				continue;

			error_exit("dup() failed\n");
		}

		break;
	}

	return new_fd;
}

char * term_t_to_string(term_t term)
{
	switch(term)
	{
		case TERM_ANSI:
			return "ansi";

		case TERM_XTERM:
			return "xterm";

		case TERM_IGNORE:
			return "dumb";
	}

	return "dumb";
}

void double_ts_to_str(dtime_t ts, char *format_str, char *dest, int dest_size)
{
	time_t now = (time_t)ts;
	struct tm *ptm = localtime(&now);
	if (!ptm) error_exit("localtime() failed\n");

	assert(ts > 0);
	assert(dest_size > 0);

	(void)strftime(dest, dest_size, format_str, ptm);
}

void get_now_ts(char *format_str, char *dest, int dest_size)
{
	double_ts_to_str(get_ts(), format_str, dest, dest_size);
}

void duplicate_re_array(re *pre_in, int n_rein, re **pre_out, int *n_reout)
{
	int loop, old_n = *n_reout;

	assert(n_rein >= 0);

	*n_reout += n_rein;
	if (*n_reout)
	{
		*pre_out = (re *)myrealloc(*pre_out, (*n_reout) * sizeof(re));

		for(loop=0; loop<n_rein; loop++)
		{
			memcpy(&(*pre_out)[loop + old_n], &pre_in[loop], sizeof(re));

			memset(&(*pre_out)[loop + old_n].regex, 0x00, sizeof(regex_t));
			(*pre_out)[loop + old_n].regex_str = mystrdup(pre_in[loop].regex_str);
			compile_re(&(*pre_out)[loop + old_n].regex, (*pre_out)[loop + old_n].regex_str);

			if (pre_in[loop].cmd)
				(*pre_out)[loop + old_n].cmd = mystrdup(pre_in[loop].cmd);
		}
	}
	else
	{
		*pre_out = NULL;
	}
}

void duplicate_es_array(strip_t *pes_in, int n_esin, strip_t **pes_out, int *n_esout)
{
	int loop, old_n = *n_esout;

	assert(n_esin >= 0);

	*n_esout += n_esin;
	*pes_out = (strip_t *)myrealloc(*pes_out, (*n_esout) * sizeof(strip_t));

	for(loop=0; loop<n_esin; loop++)
	{
		memcpy(&(*pes_out)[loop + old_n], &pes_in[loop], sizeof(strip_t));

		memset(&(*pes_out)[loop + old_n].regex, 0x00, sizeof(regex_t));
		if (pes_in[loop].type != STRIP_TYPE_COLUMN && pes_in[loop].type != STRIP_TYPE_RANGE)
		{
			(*pes_out)[loop + old_n].regex_str = mystrdup(pes_in[loop].regex_str);

			compile_re(&(*pes_out)[loop + old_n].regex, (*pes_out)[loop + old_n].regex_str);
		}

		if (pes_in[loop].del)
			(*pes_out)[loop + old_n].del = mystrdup(pes_in[loop].del);
	}
}

int find_filterscheme(char *name)
{
	int loop;

	for(loop=0; loop<n_fs; loop++)
	{
		if (strcmp(pfs[loop].fs_name, name) == 0)
			return loop;
	}

	return -1;
}

int find_editscheme(char *name)
{
	int loop;

	for(loop=0; loop<n_es; loop++)
	{
		if (strcmp(pes[loop].es_name, name) == 0)
			return loop;
	}

	return -1;
}

void compile_re(regex_t *whereto, char *what)
{
	/* compile & store regular expression */
	int rc = regcomp(whereto, what, REG_EXTENDED);
	if (rc != 0)
		error_exit("Failed to compile regular expression '%s'.\nReason: %s\n", what, convert_regexp_error(rc, whereto));
}

void grow_mem_if_needed(char **what, int *cur_len, int requested_len)
{
	char changed = 0;

	assert(requested_len > 0);

	while(*cur_len < requested_len)
	{
		changed = 1;

		if (*cur_len)
			(*cur_len) *= 2;
		else
			*cur_len = 128;
	}

	if (changed)
		*what = myrealloc(*what, *cur_len);
}

int READ(int fd, char *where_to, int max_len, char *for_whom)
{
	assert(max_len > 0);

	for(;;)
	{
		int rc = read(fd, where_to, max_len);
		if (rc >= 0)
			return rc;

		if (errno != EINTR && errno != EAGAIN)
			error_exit("Error writing to fd for %s.\n", for_whom);
	}
}

int get_value_arg(char *par, char *string, valcheck_t check)
{
	long int result;
	int len, loop;
	int multiplier = 1;

	if (!string)
		error_exit("%s needs a parameter.\n", par);

	len = strlen(string);

	for(loop=0; loop<len; loop++)
	{
		if (!isdigit(string[loop]))
		{
			char dummy = toupper(string[loop]);

			if (toupper(string[loop + 1]) == 'B' && (dummy == 'K' || dummy == 'M' || dummy == 'G'))
			{
				if (dummy == 'K')
					multiplier = 1024;
				else if (dummy == 'M')
					multiplier = 1024 * 1024;
				else if (dummy == 'G')
					multiplier = 1024 * 1024 * 1024;

				break;
			}
			else
				error_exit("%s needs a value as parameter.\n", par);
		}
	}

	result = strtol(string, NULL, 10);

	if (result > INT_MAX || result == LONG_MIN || result == LONG_MAX)
		error_exit("Value %s for parameter %s is too large.\n", string, par);

	result *= multiplier;

	if (check == VAL_ZERO_POSITIVE)
	{
		if (result < 0)
			error_exit("Value for %s must be >= 0.\n", par);
	}
	else if (check == VAL_POSITIVE_NOT_1)
	{
		if (result < 0 || result == 1)
			error_exit("Value for %s must be >= 0 and must not be 1.\n", par);
	}
	else if (check == VAL_POSITIVE)
	{
		if (result <= 0)
			error_exit("Value for %s must be > 0.\n", par);
	}
	else
	{
		assert(0);
	}

	return (int)result;
}

void add_to_iat(int_array_t *piat, int element)
{
	if (piat -> n == piat -> size)
	{
		piat -> size = USE_IF_SET(piat -> size, 8) * 2;
		piat -> elements = (int *)myrealloc(piat -> elements, piat -> size * sizeof(int));
	}

	(piat -> elements)[piat -> n] = element;
	(piat -> n)++;
}

int get_iat_size(int_array_t *piat)
{
	return piat -> n;
}

void init_iat(int_array_t *piat)
{
	piat -> elements = NULL;
	piat -> size = piat -> n = 0;
}

void free_iat(int_array_t *piat)
{
	myfree(piat -> elements);

	init_iat(piat);
}

int get_iat_element(int_array_t *piat, int index)
{
	assert(index < piat -> n);

	return (piat -> elements)[index];
}

char *gethome(char *user)
{
	struct passwd *pw;
	if (user)
		pw = getpwnam(user);
	else
		pw = getpwuid(getuid());
	return mystrdup(pw->pw_dir);
}

char *myrealpath(char *in)
{
	char *home, *pin;
	int home_len;
	char *pout;

	if (in[0] != '~')
		return mystrdup(in);

	if (in[1] != '/')
	{
		char *user;
		char *slash = strchr(in, '/');
		int len;

		if (slash)
		{
			len = (int)((slash - in) - 1);
			pin = slash + 1;
		}
		else
		{
			len = strlen(in) - 1;
			pin = in + len + 1;
		}

		user = (char *)mymalloc(len + 1);
		memcpy(user, &in[1], len);
		user[len] = 0x00;

		home = gethome(user);

		myfree(user);
	}
	else
	{
		home = gethome(NULL);
		pin = in + 1;
	}

	if (!home)
		error_exit("Cannot expand path %s:\nhome directory not found.\n", in);

	home_len = strlen(home);
	pout = (char *)mymalloc(home_len + strlen(in) + 1);

	sprintf(pout, "%s/%s", home, pin);

	myfree(home);

	return pout;
}
