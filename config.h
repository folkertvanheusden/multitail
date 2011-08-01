typedef struct
{
	char *config_keyword;

	void (*function)(int, char *, char *);
} config_file_keyword;

int config_file_entry(int dummy, char *line);
void do_load_config(int dummynr, char *dummy, char *file);
char load_configfile(char *config_file);
