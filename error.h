void error_exit_(char *file, const char *function, int line, char *format, ...);

#define error_exit(fmt, ...) error_exit_(__FILE__, __PRETTY_FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
