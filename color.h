void add_color_scheme(int_array_t *schemes, int cur_scheme);
myattr_t choose_color(char *string, proginfo *cur, color_offset_in_line **cmatches, int *n_cmatches, mybool_t *has_merge_color, char **new_string);
int find_colorscheme(const char *name);
void init_colors(void);
color_offset_in_line *realloc_color_offset_in_line(color_offset_in_line *oldp, int n_entries);
