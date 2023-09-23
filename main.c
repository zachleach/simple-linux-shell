/*
        Written by Zach Leach for CS 3377.004 - Project 1
        NetID: zcl190002
*/      

#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#define _GNU_SOURCE

/**
 * returns a dynammically allocated array of strings and its length.
 */
void parseargs(char *line, const char *delim, int *argc, char ***args);


/**
 * reads a line of input into dst.
 * uses malloc and realloc internally (dst should be freed after use).
 * 
 * and maybe the string dst is pointing to should be freed too, idk.
 */
void getline2(char **dst);


/**
 * returns the number of args delimited substrings within a string.
 */
int countargs(char *line, const char *delim);


/**
 * converts a line of input to an array of null terminated 'args' which can be passed directly into execvp()
 * 
 * line                 string of input representing a command
 * 
 * &commandline[][]     array of null terminated args which can be passed directly into execvp(). 
 *                      each element represents a command in a pipeline.
 *                      command is stripped of spaces.
 * 
 * &cmdc                set to the number of arrays in commandline. similar to 'argc'.
 */
void parse_commandline(char *line, int *cmdc, char ****commandline);


/**
 * executes a non built-in command.
 */
int exec_commandline(int argc, char ***commandline);


/**
 * executes a commandline with only one command.
 * executes built-in commands.
 */
void exec_command(char *line, char **cmd_history, int *cmd_count);


/**
 * handles builtin
 */
int builtin_history(char **args, char **history, int *cmd_count);




/*
        read line of input
        pass line of input to exec_command()
        add command to history
        increment command count
        
        repeat       
*/
int main() {
        int cmd_count = 0;
        char **history = calloc(100, sizeof(char*));
        
        while (1) {
                printf("sish> ");
                
                // parse command 
                char *line;
                getline2(&line);
                
                // execute command
                exec_command(line, history, &cmd_count);
                
                // add to history
                history[cmd_count % 100] = line;
                cmd_count++;
        }
        
}


/*
        parse commandline from line
        
        if command is non-piped:
                if command is built-in:
                        execute built-in command
                
                if command not built-in:
                        pass commandline to exec_commandline()
        
        if error flag returned from above function calls:
                print error message
        
        free commandline
*/
void exec_command(char *line, char **cmd_history, int *cmd_count) {
        int argc;
        char ***commandline, **args;
        int exit_status = EXIT_SUCCESS;
        
        // "..." --> { { "cmd", "arg1", "arg2", ..., NULL }, {"cmd", ..., NULL }, ... }  
        parse_commandline(line, &argc, &commandline);
        
        // built-in
        args = commandline[0];
        if (strcmp(args[0], "exit") == 0) {
                exit(EXIT_SUCCESS);
        }
        else if (strcmp(args[0], "history") == 0) {
                if (argc == 1) {
                        exit_status = builtin_history(args, cmd_history, cmd_count);
                }
        }
        else if (strcmp(args[0], "cd") == 0) {
                if (argc == 1) {
                        int ret_val = chdir(args[1]);
                        if (ret_val == -1) {
                                exit_status = EXIT_FAILURE;
                        }
                }
        }
        
        // non-builtin / pipelined commands
        else {
                exit_status = exec_commandline(argc, commandline);
        }
        
        // error handling
        if (exit_status == EXIT_FAILURE) {
                printf("error on: %s\n", line);
        }
        
        // cleanup
        for (int i = 0; i < argc; i++) {
                free(commandline[i]);
        }
}


/*
        for command in commandline:
                make pipe
                fork()
                
                if child:
                        redirect stdin to previous pipe-read
                        if not last command in commandline:
                                redirect stdout to pipe-write
                        
                        execute command
                        exit(failure)
                
                if parent:
                        get child's return status
                        
                        close write file file descriptor to prevent hang
                        hold pipe-read for child of next iteration
                        
                        if child return status is failure:
                                close pipe-read
                                break out of loop
*/
int exec_commandline(int argc, char ***commandline) {
        
        int fd[2], pid;
        int prev_read = 0;
        int exit_failure = 0;
        
        // for command in commandline
        for (int i = 0; i < argc; i++) {
                char **cmd = commandline[i];
                
                // make pipe and fork
                pipe(fd);
                pid = fork();
                
                // child
                if (pid == 0) {
                        dup2(prev_read, STDIN_FILENO);
                        if (i < argc - 1) {
                                dup2(fd[1], STDOUT_FILENO);
                        }
                        execvp(cmd[0], cmd);
                        exit(EXIT_FAILURE);
                }
                
                // parent
                else {
                        // get child termination status
                        int child_status;
                        wait(&child_status);
                        
                        // close write end of pipe for parent, hold pipe-read for next child
                        close(fd[1]);
                        prev_read = fd[0];
                        
                        // if child termination status is failure:
                        if (WIFEXITED(child_status)) {
                                int exit_status = WEXITSTATUS(child_status);
                                if (exit_status == EXIT_FAILURE) {
                                        close(fd[0]);
                                        exit_failure = 1;
                                        break;
                                }
                        }
                        
                }
        }
        
        
        return exit_failure;
}



int builtin_history(char **args, char **history, int *cmd_count) {
        
        // history [-c] [offset]
        if (args[1] != NULL && args[2] == NULL) {
                int offset = atoi(args[1]);
                
                // if args[1] is an int
                if (strcmp(args[1], "0") == 0 || offset != 0) {
                        // offset out of bounds
                        if (offset < 0 || offset > 99) {
                                return EXIT_FAILURE;
                        }
                        
                        // command is null
                        if (history[offset] == NULL) {
                                return EXIT_FAILURE;
                        }
                        
                        // adjust offset for commands over 99
                        if (*cmd_count > 99) {
                                offset = (*cmd_count + offset) % 100;
                        }
                        exec_command(history[offset], history, cmd_count);
                }
                
                // if args[1] is "-c"
                else if (strcmp(args[1], "-c") == 0) {
                        for (int i = 0; i < 100; i++) {
                                history[i] = NULL;
                        }
                        
                        // assume main increments cmd_count from -1 to 0
                        *cmd_count = -1;
                }
                
                // invalid args
                else {
                        return EXIT_FAILURE;
                }
        }
        
        // history
        else {
                int offset = 0;
                if (*cmd_count > 99) {
                        offset = (*cmd_count + 1) % 100;
                }
                
                int i;
                for (i = 0; i < 100; i++) {
                        int j = (i + offset) % 100;
                        if (history[j] == NULL) {
                                break;
                        }
                        
                        printf("%d %s\n", i, history[j]);
                }
                printf("%d history\n", i);
        }
        
        
        return EXIT_SUCCESS;
        
}



/**
 * parses the arg count and args[] from a line of input.
 * args[] is dynamically allocated and needs to be freed.
 * 
 */
 
void parseargs(char *line, const char *delim, int *argc, char ***args) {
        *argc = countargs(line, delim);
        *args = malloc((*argc) * sizeof(char*));
        
        int i;
        char *token, *saveptr;
        char *str = malloc(strlen(line) * sizeof(char));
        for(i = 0, strcpy(str, line); i < *argc; i++, str = NULL) {
                token = strtok_r(str, delim, &saveptr);
                (*args)[i] = token;
        }
        
        free(str);
}



/**
 * reads a line of input into dst.
 * does not include the trailing '\n' character like getline().
 * 
 * uses malloc and realloc internally (dst should be freed after use).
 */
void getline2(char **dst) {
        size_t size = 0;
        ssize_t n = getline(dst, &size, stdin);
        if ((*dst)[n - 1] == '\n') {
                (*dst)[n - 1] = '\0';
        }
}



int countargs(char *line, const char *delim) {
        int i;
        char *token, *saveptr;
        char *str = malloc(strlen(line) * sizeof(char));
        for(i = 0, strcpy(str, line); ; i++, str = NULL) {
                token = strtok_r(str, delim, &saveptr);
                if (token == NULL) {
                        break;
                }
        }
        
        free(str);
        return i;
}



void parse_commandline(char *line, int *cmdc, char ****commandline) {
        
        // split line by pipe
        // { "cmd1 arg1 arg2 ... ", "cmd2 arg1 arg2 ... ", "cmd3 arg1 arg2 ... " }
        char **command_string;
        parseargs(line, "|", cmdc, &command_string);
        
        // initialize commandline[][];
        *commandline = malloc((*cmdc) * sizeof(char*));
        
        
        // for command in command_string
        for (int i = 0; i < (*cmdc); i++) {
                // "cmd arg1 arg2 ... " --> { cmd, arg1, arg2, ... }
                int argc;
                char **args;
                parseargs(command_string[i], " ", &argc, &args);
                
                // { cmd, arg1, arg2, ... } --> { cmd, arg1, arg2, ..., NULL }
                char **cmd = malloc((argc + 1) * sizeof(char*));
                int j;
                for (j = 0; j < argc; j++) {
                        cmd[j] = args[j];
                }
                cmd[j] = NULL;
                
                // commandline[i] = { cmd, arg1, arg2, ..., NULL }
                (*commandline)[i] = cmd;
                free(args);
        }
        
        
        free(command_string);
}


































