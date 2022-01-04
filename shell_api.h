#ifndef SHELL_API_H
#define SHELL_API_H

/* Function prototypes */
// main
int main(void);
// commands
int getcmd(char *prompt, char *args[], int *background);
void executeCommand(char *cmd[], int count);
// signal handlers
static void sigintHandler(int signal);
static void sigtstpHandler(int signal);
// check for piping, redirection or built-ins
int isTherePipe(char *cmd[], int count);
int isThereRedirection(char *cmd[], int count);
int isBuiltInCommand(char *cmd[]);
// execution of built-ins
void executeBuiltInCommand(char *cmd[]);
void my_cd(char *directory[]);
void my_pwd();
void my_exit();
void my_jobs();
void my_fg(int32_t job);
// redirection and piping
void redirectOutput(char *cmd[], char *file[]);
void pipeCommand(char *cmd1[], char *cmd2[]);

#endif
