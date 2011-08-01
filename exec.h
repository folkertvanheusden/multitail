int start_tail(char *filename, char retry_open, char follow_filename, int initial_tail, int *pipefd);
int start_proc(proginfo *cur, int initial_tail);
int execute_program(char *execute, char bg);
void init_children_reaper(void);
pid_t exec_with_pipe(char *command, int pipe_to_proc[], int pipe_from_proc[]);
void exec_script(script *pscript);
