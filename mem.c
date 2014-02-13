#define _LARGEFILE64_SOURCE	/* required for GLIBC to enable stat64 and friends */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <regex.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "error.h"
#include "globals.h"
#include "term.h"
#include "utils.h"
#include "mem.h"

void myfree(void *p)
{
	free(p);
}

void * myrealloc(void *oldp, int new_size)
{
	/* ----------------------------------------------------
	 * add code for repeatingly retry? -> then configurable
	 * via configurationfile with number of retries and/or
	 * sleep
	 * ----------------------------------------------------
	 */
	void *newp = realloc(oldp, new_size);
	if (!newp)
		error_exit(TRUE, TRUE, "Failed to reallocate a memory block to %d bytes.\n", new_size);

	return newp;
}

void * mymalloc(int size)
{
	return myrealloc(NULL, size);
}

char * mystrdup(const char *in)
{
	char *newp = strdup(in);
	if (!newp)
		error_exit(TRUE, TRUE, "Failed to duplicate a string: out of memory?\n");

	return newp;
}

void clean_memory(void)
{
	int loop;

	/* color schemes */
	for(loop=0; loop<n_cschemes; loop++)
	{
		int loop2;

		myfree(cschemes[loop].name);
		myfree(cschemes[loop].descr);

		for(loop2=0; loop2<cschemes[loop].n; loop2++)
		{
			regfree(&cschemes[loop].pentries[loop2].regex);
		}

		myfree(cschemes[loop].color_script.script);
		myfree(cschemes[loop].pentries);
	}
	myfree(cschemes);

	/* colors */
	myfree(cp.fg_color);
	myfree(cp.bg_color);

	/* text buffers */
	for(loop=0; loop<nfd; loop++)
		delete_be_in_buffer(&lb[loop]);
	myfree(lb);

	/* programs array */
	for(loop=0; loop<nfd; loop++)
	{
		proginfo *cur = &pi[loop];

		while(cur)
		{
			proginfo *pcur = cur;
			int loop2;

			for(loop2=0; loop2<cur -> n_strip; loop2++)
			{
				if ((cur -> pstrip)[loop2].type == STRIP_TYPE_REGEXP ||
				    (cur -> pstrip)[loop2].type == STRIP_KEEP_SUBSTR)
				{
					regfree(&(cur -> pstrip)[loop2].regex);
					myfree ((cur -> pstrip)[loop2].regex_str);
				}
				myfree ((cur -> pstrip)[loop2].del);
			}
			myfree(cur -> pstrip);

			myfree(cur -> filename);

			for(loop2=0; loop2<cur -> n_redirect; loop2++)
				myfree((cur -> predir)[loop2].redirect);
			myfree(cur -> predir);

			myfree(cur -> label);

			free_iat(&cur -> conversions);

			myfree(cur -> incomplete_line);
			myfree(cur -> win_title);

			delete_array(cur -> restart.diff.bcur, cur -> restart.diff.ncur);
			delete_array(cur -> restart.diff.bprev, cur -> restart.diff.nprev);

			myfree(cur -> repeat.last_line);

			free_iat(&cur -> cdef.color_schemes);

			delete_popup(cur -> status);
			delete_popup(cur -> data);

			for(loop2=0; loop2<cur -> n_re; loop2++)
				free_re(&(cur -> pre)[loop2]);
			myfree(cur -> pre);

			cur = cur -> next;

			if (pcur != &pi[loop])
				myfree(pcur);
		}
	}
	myfree(pi);

	/* parameters per file */
	for(loop=0; loop<n_pars_per_file; loop++)
	{
		int loop2;

		for(loop2=0; loop2<ppf[loop].n_colorschemes; loop2++)
			myfree(ppf[loop].colorschemes[loop2]);
		myfree(ppf[loop].colorschemes);

		if (ppf[loop].re_str)
		{
			regfree(&ppf[loop].regex);
			myfree(ppf[loop].re_str);
		}

		free_iat(&ppf[loop].filterschemes);
		free_iat(&ppf[loop].editschemes);

		for(loop2=0; loop2<ppf[loop].n_conversion_schemes; loop2++)
			myfree(ppf[loop].conversion_schemes[loop2]);
		myfree(ppf[loop].conversion_schemes);
	}
	myfree(ppf);

	/* conversions */
	for(loop=0; loop<n_conversions; loop++)
	{
		int loop2;

		myfree(conversions[loop].name);

		for(loop2=0; loop2<conversions[loop].n; loop2++)
			regfree(&conversions[loop].pcb[loop2].regex);
		myfree(conversions[loop].pcb);

		myfree(conversions[loop].pcs -> script);
		myfree(conversions[loop].pcs);
	}
	myfree(conversions);

	/* filterschemes */
	for(loop=0; loop<n_fs; loop++)
	{
		int loop2;

		myfree(pfs[loop].fs_name);
		myfree(pfs[loop].fs_desc);

		for(loop2=0; loop2<pfs[loop].n_re; loop2++)
			free_re(&pfs[loop].pre[loop2]);
		myfree(pfs[loop].pre);
	}
	myfree(pfs);

	/* edit schemes */
	for(loop=0; loop<n_es; loop++)
	{
		int loop2;

		myfree(pes[loop].es_name);
		myfree(pes[loop].es_desc);

		for(loop2=0; loop2<pes[loop].n_strips; loop2++)
		{
			regfree(&pes[loop].strips[loop2].regex);
			myfree(pes[loop].strips[loop2].regex_str);
			myfree(pes[loop].strips[loop2].del);
		}
		myfree(pes[loop].strips);
	}
	myfree(pes);

	/* keybindings */
	for(loop=0; loop<n_keybindings; loop++)
		myfree(keybindings[loop].command);
	myfree(keybindings);

	/* splitlines */
	for(loop=0; loop<n_splitlines; loop++)
		delete_popup(splitlines[loop]);
	myfree(splitlines);

	/* history */
	myfree(search_h.history_file);
	if (search_h.history)
	{
		for(loop=0; loop<search_h.history_size; loop++)
			myfree(search_h.history[loop]);
	}
	myfree(search_h.history);
	myfree(cmdfile_h.history_file);
	if (cmdfile_h.history)
	{
		for(loop=0; loop<cmdfile_h.history_size; loop++)
			myfree(cmdfile_h.history[loop]);
	}
	myfree(cmdfile_h.history);

	myfree(color_names);
	myfree(window_number);
	myfree(subwindow_number);
	myfree(defaultcscheme);
	myfree(shell);
	myfree(ts_format);
	myfree(cnv_ts_format);
	myfree(statusline_ts_format);
	myfree(tail);
	myfree(set_title);
	myfree(replace_by_markerline);
	myfree(line_ts_format);
	myfree(n_win_per_col);
	myfree(syslog_ts_format);

	myfree(global_find);
}
