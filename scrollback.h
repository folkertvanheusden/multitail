int find_string(int window, char *find, int offset);
void scrollback_help(void);
void scrollback_savefile(int window);
int get_lines_needed(char *string, int terminal_width);
void scrollback_displayline(NEWWIN *win, int window, int offset, int terminal_offset);
void scrollback(void);
void delete_mark(void);
void merged_scrollback_with_search(char *search_for, mybool_t case_insensitive);
