void error_exit_(BOOL show_errno, BOOL show_st, char *file, const char *function, int line, char *format, ...);

#define error_exit(x, y, fmt, ...) error_exit_(__FILE__, __PRETTY_FUNCTION__, __LINE__, x, y, fmt, ##__VA_ARGS__)
