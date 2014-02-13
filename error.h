#define BOOL char
#define TRUE 1
#define FALSE 0

void error_exit_(BOOL show_errno, BOOL show_st, char *file, const char *function, int line, char *format, ...);

#define error_exit(x, y, fmt, ...) error_exit_(x, y, __FILE__, __PRETTY_FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
