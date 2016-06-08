extern char *clipboard;

#ifdef __APPLE__
#define CLIPBOARD_NAME "pbcopy"
#else
#define CLIPBOARD_NAME "xclip"
#endif

void send_to_clipboard_binary(char *what);
void send_to_clipboard(buffer *pb);
