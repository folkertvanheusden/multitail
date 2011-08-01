#define _LARGEFILE64_SOURCE
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <regex.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "mt.h"
#include "error.h"
#include "utils.h"
#include "mem.h"
#include "selbox.h"
#include "ui.h"
#include "help.h"
#include "globals.h"

void init_history(history_t *ph)
{
}

void load_history(history_t *ph)
{
	if (ph -> history == NULL)
	{
		int array_in_bytes = sizeof(char *) * ph -> history_size;

		ph -> history = (char **)mymalloc(array_in_bytes, __FILE__, __PRETTY_FUNCTION__, __LINE__);
		memset(ph -> history, 0x00, array_in_bytes);

		if (file_exist(ph -> history_file) == 0)
		{
			int loop;
			FILE *fh = fopen(ph -> history_file, "r");
			if (!fh)
				error_popup("Load history", -1, "Failed to open history file %s.\n", ph -> history_file);

			for(loop=0; loop<ph -> history_size; loop++)
			{
				char *lf;
				char buffer[HISTORY_IO_BUFFER];

				if (!fgets(buffer, sizeof(buffer), fh))
					break;

				lf = strchr(buffer, '\n');
				if (lf)
					*lf = 0x00;

				(ph -> history)[loop] = mystrdup(buffer, __FILE__, __PRETTY_FUNCTION__, __LINE__);
			}

			fclose(fh);
		}
	}
}

int history_find_null_entry(history_t *ph)
{
	int loop;

	for(loop=0; loop<ph -> history_size; loop++)
	{
		if (!(ph -> history)[loop])
		{
			return loop;
		}
	}

	return -1;
}

void save_history(history_t *ph)
{
	int loop;
	int last_entry = history_find_null_entry(ph);
	int n_entries = last_entry == -1 ? ph -> history_size : last_entry;
	FILE *fh = fopen(ph -> history_file, "w+");
	if (!fh)
	{
		error_popup("Error saving history", -1, "Error creating/opening file %s: %s", ph -> history_file, strerror(errno));
		return;
	}

	for(loop=0; loop<n_entries; loop++)
	{
		fprintf(fh, "%s\n", (ph -> history)[loop]);
	}

	fclose(fh);
}

void history_add(history_t *ph, char *string)
{
	if (ph -> history_size > 0)
	{
		int loop;
		int found = -1;

		load_history(ph);

		/* bail out if this string is already in the history */
		for(loop=0; loop<ph -> history_size; loop++)
		{
			if ((ph -> history)[loop] != NULL && strcmp((ph -> history)[loop], string) == 0)
				return ;
		}

		/* find free spot */
		found = history_find_null_entry(ph);

		/* when no free spot was found, free-up an entry */
		if (found == -1)
		{
			myfree((ph -> history)[ph -> history_size - 1]);

			memmove(&(ph -> history)[1], &(ph -> history)[0], sizeof(char *) * (ph -> history_size - 1));

			found = 0;
		}

		(ph -> history)[found] = mystrdup(string, __FILE__, __PRETTY_FUNCTION__, __LINE__);

		save_history(ph);
	}
}

char * search_history(history_t *ph, char *search_string)
{
	if (ph -> history_size > 0)
	{
		int sel_index, n_entries, free_entry;

		load_history(ph);

		free_entry = history_find_null_entry(ph);
		n_entries = free_entry == -1 ? ph -> history_size : free_entry;

		if (n_entries > 0)
		{
			sel_index = selection_box((void **)ph -> history, NULL, n_entries, SEL_HISTORY, HELP_HISTORY, NULL);

			if (sel_index >= 0)
			{
				return mystrdup((ph -> history)[sel_index], __FILE__, __PRETTY_FUNCTION__, __LINE__);
			}
		}
		else
			error_popup("Search history", -1, "The history list is empty.");
	}

	return NULL;
}
