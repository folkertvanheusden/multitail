#define _LARGEFILE64_SOURCE     /* required for GLIBC to enable stat64 and friends */
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "mem.h"
#include "utils.h"
#include "globals.h"


void store_for_diff(diff_t *diff, char *string)
{
	diff -> bcur = (char **)myrealloc(diff -> bcur, sizeof(char *) * (diff -> ncur + 1));

	(diff -> bcur)[diff -> ncur] = mystrdup(string);

	diff -> ncur++;
}

void generate_diff(int winnr, proginfo *cur)
{
	int loop_cur;
	int search_offset = 0;

	/* calculate & print difference */
	for(loop_cur=0; loop_cur < cur -> restart.diff.ncur; loop_cur++)
	{
		int loop_prev;
		char found = 0;

		for(loop_prev=0; loop_prev < cur -> restart.diff.nprev; loop_prev++)
		{
			if (strcmp((cur -> restart.diff.bprev)[(search_offset + loop_prev) % cur -> restart.diff.nprev], (cur -> restart.diff.bcur)[loop_cur]) == 0)
			{
				found = 1;
				break;
			}
		}

		if (!found)
		{
			/* output cur[loop] */
			emit_to_buffer_and_term(winnr, cur, cur -> restart.diff.bcur[loop_cur]);
			search_offset = 0;
		}
		else
		{
			search_offset = loop_prev + 1;
		}
	}
	update_panels();

	/* free up previous */
	delete_array(cur -> restart.diff.bprev, cur -> restart.diff.nprev);

	/* remember current */
	cur -> restart.diff.bprev = cur -> restart.diff.bcur;
	cur -> restart.diff.nprev = cur -> restart.diff.ncur;
	cur -> restart.diff.bcur = NULL;
	cur -> restart.diff.ncur = 0;
}
