void * mymalloc(int size, char *file, const char *function, int line);
void * myrealloc(void *oldp, int newsize, char *file, const char *function, int line);
void myfree(void *p);
char * mystrdup(char *in, char *file, const char *function, int line);
#ifdef _DEBUG
void clean_memory(void);
#endif
