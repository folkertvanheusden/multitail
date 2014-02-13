#define _LARGEFILE64_SOURCE     /* required for GLIBC to enable stat64 and friends */
#include <sys/types.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "mem.h"
#include "error.h"
#include "utils.h"
#include "exec.h"
#include "globals.h"


int cv_offsets_compare(const void *a, const void *b)
{
	cv_off *pa = (cv_off *)a, *pb = (cv_off *)b;

	if (pa -> start > pb -> start)
		return -1;
	else if (pa -> start == pb -> start)
	{
		if (pa -> end > pb -> end)
			return -1;
	}

	return 0;
}

char * epoch_to_str(time_t epoch)
{
	char *new_string;
	struct tm *ptm = localtime(&epoch);
	if (!ptm)
		return NULL;

	new_string = mymalloc(4096);
	if (!strftime(new_string, 4096, cnv_ts_format, ptm))
		error_exit(FALSE, FALSE, "An error occured whilte converting timestamp format '%s'.\n", cnv_ts_format);

	return new_string;
}

char *do_convert(char *what, int what_len, int type, script *pscript)
{
	switch(type)
	{
		case CONVTYPE_SIGNRTOSTRING:
			{
				int signr = atoi(what);

				if (signr > n_known_sigs || signr < 1)
					return mystrdup("unknown signal");

				return mystrdup(sigs[signr]);
			}

		case CONVTYPE_TAI64NTODATE:
			{
				long long int v2_62 = (long long int)1 << (long long int)62;
				long long int val = 0;
				int loop;

				if (what[0] == '@')
					what++;

				/* http://cr.yp.to/libtai/tai64.html#tai64n */
				/* convert to 64 bit integer */
				for(loop=0; loop<(8 * 2); loop++)
				{
					int c = tolower(what[loop]);

					val <<= (long long int)4;
					if (c >= 'a')
						val += 10 + c - 'a';
					else
						val += c - '0';
				}

				if (val >= v2_62) /* FIXME: 2^63 are reserved, not checking for that, sorry */
				{
					char *new_str = epoch_to_str((time_t)(val - v2_62));

					if (new_str)
						return new_str;
					else
						return mystrdup("cannot convert current 'TAI64N'-date to string");
				}
				else
				{
					/* before 1970/1/1 now what should I do with that? */

					return mystrdup("cannot convert 'TAI64N'-dates before the epoch");
				}
			}

		case CONVTYPE_IP4TOHOST:
			{
				if (resolv_ip_addresses)
				{
					struct hostent *ht;
					in_addr_t addr = inet_addr(what);
					if ((int)addr == -1)
						return mystrdup(what);

					if ((ht = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET)) == NULL)
						return mystrdup(what);

					return mystrdup(ht -> h_name);
				}

				return mystrdup(what);
			}
			break;

		case CONVTYPE_EPOCHTODATE:
			{
				char *new_str = epoch_to_str((time_t)atoll(what));

				if (new_str)
					return new_str;
				else
					return mystrdup("cannot convert current epoch value");
			}
			break;

		case CONVTYPE_ERRNO:
			{
				return mystrdup(strerror(atoi(what)));
			}

		case CONVTYPE_HEXTODEC:
			{
				long long int result = strtoll(what, NULL, 16);
				char result_str[128];

				snprintf(result_str, sizeof(result_str), "%lld", result);

				return mystrdup(result_str);
			}

		case CONVTYPE_DECTOHEX:
			{
				long long int result = atoll(what);
				char result_str[128];

				snprintf(result_str, sizeof(result_str), "%llx", result);

				return mystrdup(result_str);
			}

		case CONVTYPE_SCRIPT:
			{
				int rc;
				char *send_buffer = mymalloc(what_len + 1 + 1);
				char *result_str = mymalloc(SCRIPT_IO_BUFFER_SIZE);

				exec_script(pscript);

				memcpy(send_buffer, what, what_len);
				send_buffer[what_len] = '\n';
				send_buffer[what_len + 1] = 0x00;

				WRITE(pscript -> fd_w, send_buffer, what_len + 1, "conversion script (is it still running?)");
				myfree(send_buffer);

				rc = READ(pscript -> fd_r, result_str, SCRIPT_IO_BUFFER_SIZE - 1, pscript -> script);
				result_str[rc > 0?rc - 1:rc] = 0x00;

				return result_str;
			}

		case CONVTYPE_ABBRTOK:
			return amount_to_str(atoll(what));

		default:
			error_exit(FALSE, FALSE, "Internal error: unknown conversion type %d.\n", type);
	}

	return "do_convert: INTERNAL ERROR";
}

char *convert(int_array_t *pconversions, char *line)
{
	conversion *cur_conv = NULL;
	cv_off *cv_offsets = NULL;
	int conv_index;
	int conv_req;
	int new_len = 0;
	int cur_n_cv_matches = 0;
	char *new_string = NULL;
	int offset_old = 0, offset_new = 0;
	int old_len = strlen(line);
	int n_conversions = get_iat_size(pconversions);

	if (n_conversions == 0)
		return line;

	for(conv_req=0; conv_req < n_conversions; conv_req++)
	{
		cur_conv = &conversions[get_iat_element(pconversions, conv_req)];

		/* find where they match */
		for(conv_index=0; conv_index<cur_conv -> n; conv_index++)
		{
			int offset = 0;
			do
			{
				int cur_match_index;
				int cur_offset = offset;
				regmatch_t matches[MAX_N_RE_MATCHES];

				/* FIXME: what to do with regexp errors? */
				if (regexec(&(cur_conv -> pcb)[conv_index].regex, &line[offset], MAX_N_RE_MATCHES, matches, offset?REG_NOTBOL:0) != 0)
					break;

				for(cur_match_index=1; cur_match_index<MAX_N_RE_MATCHES; cur_match_index++)
				{
					char *dummy;
					int dummylen;
					int match_start, match_end;

					match_start = matches[cur_match_index].rm_so + cur_offset;
					match_end   = matches[cur_match_index].rm_eo + cur_offset;

					offset = max(offset + 1, match_end);

					if (matches[cur_match_index].rm_so == -1)
						break;

					(cur_conv -> pcb)[conv_index].match_count++;

					cv_offsets = (cv_off *)myrealloc(cv_offsets, sizeof(cv_off) * (cur_n_cv_matches + 1));
					cv_offsets[cur_n_cv_matches].start = match_start;
					cv_offsets[cur_n_cv_matches].end   = match_end;

					dummylen = match_end - match_start;

					dummy = mymalloc(dummylen + 1);
					memcpy(dummy, &line[match_start], dummylen);
					dummy[dummylen] = 0x00;

					cv_offsets[cur_n_cv_matches].newstr = do_convert(dummy, dummylen, (cur_conv -> pcb)[conv_index].type, &(cur_conv -> pcs)[conv_index]);

					myfree(dummy);

					cur_n_cv_matches++;
				}
			} while (offset < old_len);
		}
	}

	if (cur_n_cv_matches)
	{
		int n_copy;

		/* sort */
		if (cur_n_cv_matches > 1)
			qsort(cv_offsets, cur_n_cv_matches, sizeof(cv_off), cv_offsets_compare);

		/* create new string */
		for(conv_index=0; conv_index < cur_n_cv_matches; conv_index++)
		{
			n_copy = cv_offsets[conv_index].start - offset_old;
			if (n_copy > 0)
			{
				new_string = myrealloc(new_string, new_len + n_copy + 1);
				memcpy(&new_string[offset_new], &line[offset_old], n_copy);
				new_string[offset_new + n_copy] = 0x00;
				new_len += n_copy;
				offset_new += n_copy;
			}
			offset_old = cv_offsets[conv_index].end;

			n_copy = strlen(cv_offsets[conv_index].newstr);
			new_string = myrealloc(new_string, new_len + n_copy + 1);
			memcpy(&new_string[offset_new], cv_offsets[conv_index].newstr, n_copy);
			new_string[offset_new + n_copy] = 0x00;
			myfree(cv_offsets[conv_index].newstr);
			new_len += n_copy;
			offset_new += n_copy;
		}

		n_copy = old_len - offset_old;
		if (n_copy)
		{
			new_string = myrealloc(new_string, new_len + n_copy + 1);
			memcpy(&new_string[offset_new], &line[offset_old], n_copy);
			new_string[offset_new + n_copy] = 0x00;
		}
	}
	else
	{
		new_string = line;
	}

	myfree(cv_offsets);

	return new_string;
}

int find_conversion_scheme(char *name)
{
	int loop;

	for(loop=0; loop<n_conversions; loop++)
	{
		if (strcmp(conversions[loop].name, name) == 0)
			return loop;
	}

	return -1;
}

void add_conversion_scheme(int_array_t *conversions, char *conversion_name)
{
	int conversion_nr = find_conversion_scheme(conversion_name);
	if (conversion_nr == -1)
		error_exit(FALSE, FALSE, "'%s' is not a known conversion scheme.\n", conversion_name);

	add_to_iat(conversions, conversion_nr);
}
