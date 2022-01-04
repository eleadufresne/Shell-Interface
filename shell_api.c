// This is a C program that implements a shell interface that accepts user commands and
// executes each command in a separate process.
// Éléa Dufresne
// 2021-10-09

/* includes */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "shell_api.h"

/* fields */
pid_t process; // current foreground process
pid_t* processes; // list of background processes
char **names; // their name
int num_of_processes; // number of processes

/* main function */
int main(void){
    char *args[20]; // user input command
    int background = 0, command_length; // does this process run in the background, input command length
    // initialize empty arrays of processes
    names=(char **)malloc(sizeof(char **));
    processes=(pid_t *)malloc(sizeof(pid_t));
    num_of_processes=0;

    // infinite loop
    while(1) {
        // deal with CTRL+C, CTRL+Z and associated errors
        if(signal(SIGINT, sigintHandler)==SIG_ERR || signal(SIGTSTP, sigtstpHandler)==SIG_ERR){
            fprintf(stderr,"Error, could not bind the signal handler\n");
            my_exit();
        }
        signal(SIGINT, sigintHandler); // kill foreground process is CTRL+C is pressed
        signal(SIGTSTP, sigtstpHandler); // do nothing if CTRL+Z is pressed

        // get the user input and its length
        command_length = getcmd("\n>> ", args, &background);
        // fork the child process
        process = fork();

        // if an error as occurred
        if( process < 0 ){
            fprintf(stderr, "Forked failed.");
            my_exit();
        // if this is the child process
        }else if( !process ){
            executeCommand(args, command_length); //execute the command
        // if this is the parent process
        }else{
            // if this process has to run in the background
            if( background ) {
                // add it to the arrays of background processes
                names[num_of_processes] = *args;
                processes[num_of_processes++] = process;
            // otherwise, wait for the child to exit
            } else waitpid(process, NULL, 0);
        }
        // empty the args array and reset background to zero
        memset(args, 0, 20);
        background = 0;
    }
    return 0;
}

/* read the user’s next command and parse it into separate tokens */
int getcmd(char *prompt, char *args[], int *background){
    int length, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;
    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);
    if (length <= 0) {
        my_exit();
    }
    // Check if background is specified..
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } else
        *background = 0;
    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (int j = 0; j < strlen(token); j++)
            if (token[j] <= 32)
                token[j] = '\0';
        if (strlen(token) > 0)
            args[i++] = token;
    }
    args[i] = NULL;
    return i;
}

/* execute the current command */
void executeCommand(char *cmd[], int count){
    int index = 0;
    int index1 = isTherePipe(cmd, count); // index of the first "|" or 0 if none
    int index2 = isThereRedirection(cmd, count); // index of the first ">" or 0 if none

    // if there is both piping and redirection
    if(index1 && index2) index = (index1 < index2) ? index1 : index2; // set index to the smallest of both
    // if there is only piping or redirection
    else if (index1 || index2) index = index1 ? index1 : index2; // set index to whichever is true

    // if there is either piping or redirection
    if( index ){
        char *cmd1[index+1]; // first command
        char *cmd2[count - index]; // second command
        for (int i = 0; i < index; i++) cmd1[i] = cmd[i]; // don't include '|' or '>'
        for (int j = 0, k = index + 1; k < count; j++, k++) cmd2[j] = cmd[k];

        // null terminate the commands
        cmd1[index] = NULL;
        cmd2[count - index - 1] = NULL;

        // if it's a pipe
        if(index == index1 ) pipeCommand(cmd1, cmd2);
        // if it's an output redirection
        else redirectOutput(cmd1, cmd2);
    // if the command is built-in
    } else if (isBuiltInCommand(cmd)) executeBuiltInCommand(cmd);
    // otherwise, invoke execvp() and print if an error occurs
    else { if (execvp(cmd[0], cmd) == -1) fprintf(stderr,"Error, could not execute command \"%s\".\n", cmd[0]); }
}

/* handle CTRL+C (SIGINT) */
static void sigintHandler(int signal){
    // kill the process that is running within the shell when Ctrl+C is pressed
    kill(process, SIGKILL);
	printf("\n");
}

/* handle CTRL+Z (SIGTSTP) */
static void sigtstpHandler(int signal){
    // do nothing
}

/* returns the first index at which there is "|", otherwise returns 0 */
int isTherePipe(char *cmd[], int count) {
    // if the syntax is incorrect
    if (!strcmp(cmd[0], "|") || (count == 2 && !strcmp(cmd[1], "|"))) {
        fprintf(stderr, "Error, expected : command_1 | command_2 | .... | command_N \n");
    // if there are enough arguments for a pipe and the syntax is correct
    } else if ( count > 2 ) {
        // parse the command string
        for(int i=1; i<count; i++){
            // if there is a "|"
            if( !strcmp(cmd[i],"|") ) {
                // if the syntax is incorrect
                if( i==count-1 ) fprintf(stderr, "Error, expected : command_1 | command_2 | .... | command_N \n");
                // return the index of the first pipe
                else return i;
            }
        }
    }
    // if there is no valid piping
    return 0;
}

/* returns the first index at which there is ">", otherwise returns 0 */
int isThereRedirection(char *cmd[], int count) {
    // if the syntax is incorrect
    if (!strcmp(cmd[0], ">") || (count == 2 && !strcmp(cmd[1], ">"))) {
        fprintf(stderr, "Error, expected : command > [file or directory] \n");
    // if there are enough arguments for a redirection and the syntax is correct
    } else if ( count > 2 ) {
        // parse the command string
        for(int i=1; i<count; i++){
            // if there is a ">"
            if(!strcmp(cmd[i],">")) {
                // if the syntax is incorrect
                if( i==count-1 ) fprintf(stderr, "Error, expected : command > [file or directory] \n");
                // return the index of the first redirection
                else return i;
            }
        }
    }
    // if there is no valid redirection
    return 0;
}

/* returns 1 if cmd is a built-in command, otherwise returns 0 */
int isBuiltInCommand(char *cmd[]){
    if(!strcmp(cmd[0],"cd") || !strcmp(cmd[0],"pwd") || !strcmp(cmd[0],"exit") || !strcmp(cmd[0],"fg") || !strcmp(cmd[0],"jobs"))
        return 1;
    else
        return 0;
}

/* execute built-in commands */
void executeBuiltInCommand(char *cmd[]){
    // call the appropriate function to execute the command
    if(!strcmp(cmd[0],"cd")) my_cd(cmd);
    else if(!strcmp(cmd[0],"pwd")) my_pwd();
    else if(!strcmp(cmd[0],"exit")) my_exit();
    else if(!strcmp(cmd[0],"fg")) my_fg((int)*cmd[1]);
    else if(!strcmp(cmd[0],"jobs")) my_jobs();
    // if cmd is not a built-in command
    else fprintf(stderr, "Error, expected built-in command.\n");
}

/* change into specified directory */
void my_cd(char *directory[]){
    // if no directory was provided
    if( directory[1]==NULL )
        fprintf(stderr, "Expected : cd [OPTIONS] directory\n");
    // otherwise, invoke chdir() and print if there is an error
    else if( chdir(directory[1]) )
        fprintf(stderr,"Error, could not execute command cd.\n");
}

/* print the current working directory */
void my_pwd(){
    long length = pathconf(".", _PC_PATH_MAX); // max length of a file path
    char *temp, *directory; // buffers to store the present directory
    temp = (char *)malloc(length); // empty array
    directory = getcwd(temp, length); // current directory
    // if it failed
    if( directory==NULL )
        fprintf(stderr,"Error, could not execute command pwd.\n");
    // otherwise, print the present working directory
    else
        printf("%s\n",directory);
    // free the temporary buffer
    free(temp);
}

/* terminate the mini shell */
void my_exit(){
    // kill ever background processes
    while( num_of_processes ) {
        kill(processes[--num_of_processes],SIGTERM);
        free(processes);
        free(names);
    }
    // kill the foreground process
    kill(process,SIGKILL);
    // exit the mini shell
    exit(0);
}

/* print all the background jobs */
void my_jobs() {
    // if no job is currently running
    if ( !num_of_processes )
        printf("No job is running in the background at the moment.\n");
    // otherwise, list all the jobs that are running in the background
    else
        for (int i = 1; i <= num_of_processes; i++) printf("[%d] %d %s \n", i, processes[i - 1], names[i - 1]);
}

/* bring the specified background job to the foreground */
void my_fg(int32_t job){
    // if no job is provided
    if( job == '\0' )
        fprintf(stderr, "Expected : fg job\n");
    // if the job number is valid
    else if( job <= num_of_processes ){
        // bring the job to the foreground
    	tcsetpgrp(0, processes[job-1]);
        // remove it from the background processes, and shift the following jobs so the array remains sequential
    	for(int i = job-1; i < num_of_processes-1; i++){
        	processes[i]=processes[i+1];
		    names[i]= names[i+1];
	    }
	    num_of_processes--;
    } else {
    	fprintf(stderr,"Error, job could not be found.\n");
    }
}

/* output redirection (NOT APPLIED TO BUILT-INS) */
void redirectOutput(char *cmd[], char *file[]){
    // make sure the command is not built in
    if( isBuiltInCommand(cmd) ) fprintf(stderr,"Error, cannot redirect output of built-in commands.\n");
    else {
        int fd = open(*file, O_CREAT | O_RDWR | O_TRUNC, 0644); // file descriptor
        close(1); // FDT[1] becomes available for use
        dup2(fd, 1); // redirect output to file
        close(fd); // close the file descriptor
        // execute the command and print if an error occurs
        if (execvp(cmd[0], cmd) == -1) fprintf(stderr, "Error, could not execute command \"%s\".\n", cmd[0]);
    }
    exit(0);
}

/* command piping (NOT APPLIED TO BUILT-INS) */
void pipeCommand(char *cmd1[], char *cmd2[]) {
    // make sure the commands are not built in
    if( isBuiltInCommand(cmd1) || isBuiltInCommand(cmd2) ) fprintf(stderr,"Error, cannot pipe built-in commands.\n");
    else {
        int fd[2]; // file descriptors
        if (pipe(fd)) fprintf(stderr, "Error, could not create pipe.\n"); // create pipe
        pid_t pid = fork(); // create a fork so the processes don't execute at the same time

        // if an error as occurred
        if ( pid < 0 ) {
            fprintf(stderr, "Forked failed.");
            my_exit();
        // if this is the child process, execute the command after the "|"
        } else if ( !pid ) {
            close(0); // FDT[0] becomes available for use
            dup2(fd[0], 0); // redirect input to read-end of pipe
            close(fd[1]); // close the write-end of the pipe
            if (execvp(cmd2[0], cmd2) == -1) fprintf(stderr, "Error, could not execute command \"%s\".\n", cmd1[0]);
        // if this is the parent process, execute the command before the "|"
        } else {
            close(1); // FDT[1] becomes available for use
            dup2(fd[1], 1); // redirect output to write-end of pipe
            close(fd[0]); // close the read-end of the pipe
            if (execvp(cmd1[0], cmd1) == -1) fprintf(stderr, "Error, could not execute command \"%s\".\n", cmd2[0]);
        }
    }
}
