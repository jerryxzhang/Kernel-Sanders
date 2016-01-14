typedef void* yyscan_t;

#include <unistd.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <linux/limits.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include "Command.h"
#include "Parser.h"
#include "Lexer.h"

#include <readline/readline.h>
#include <readline/history.h>

#define MAX_COMMAND_SIZE 1000
#define USERNAME_MAX 12
#define PROMPT_MAX (USERNAME_MAX + PATH_MAX + 2) // +2 for ":" and ">"


int yyparse(Command **command, yyscan_t scanner);
void runCommand(char *cmd_buffer);

char **translateArgs(argList* args) {
    int len = 0;
    argList* argscopy = args;
    while (argscopy != NULL) {
        argscopy = argscopy->nextArg;
        len++;
    }
    char **ret = (char**) calloc(len+1, sizeof(char**));
    len = 0;
    while (args != NULL) {
        ret[len] = args->arg;
        args = args->nextArg;
        len++;
    }
    return ret;
}


/**
 * @function    print_sh_history
 * 
 * @abstract    Handles history command.
 * @discussion  If no arguments given, entire history is printed.  If an
 *              argument is given and it is >=0, then the last [argument] 
 *              commands are printed.  Any other argument will result in an
 *              error message.
 * 
 * @param       command     The command being parsed.
 * @result      Nothing is returned, but the history/error message is printed
 *              to the terminal.
 */
void print_sh_history(Command* command) {
    // Get a NULL terminated array of commands where index 0 is the first
    // chronological entry.
    HIST_ENTRY **cmd_history = history_list();
    
    // Start printing history from this one-indexed place.
    int history_start = 1;
    
    // First, check if there are any arguments.  Only positive numbers are
    // acceptable.
    if (command->args->nextArg != NULL) {
        // There is an argument.  Check if we can convert it to an integer.
        // atoi returning zero implies either failure or "0" argument.
        
        // Convert the argument to an integer (or get an error code).
        int nextArgVal = atoi(command->args->nextArg->arg);
        if ((nextArgVal == 0) && strcmp(command->args->nextArg->arg, "0")) {
            // Failure value and atoi was not given "0"
            printf("history: %s: Requires numeric argument.\n", \
                    command->args->nextArg->arg);
                    
            // Not printing any history.
            return;
        }
        else if (nextArgVal < 0) {
            // We need a positive value so we know how many lines to print.
            printf("history: %s: invalid option\n", \
                    command->args->nextArg->arg);
            printf("history: usage: history [n]\n");
            
            // Not printing any history.
            return;
        }
        else {
            // atoi was successful, even if it returned 0 in which case the 
            // argument was the '0' character.  Print the last n commands by
            // finding a pointer to the end of the list, decrementing it n
            // times, then printing until NULL
            
            // Find offset to end of list.
            HIST_ENTRY **last_cmd = cmd_history;
            while (*last_cmd != NULL) {
                history_start++;
                last_cmd++;
            }
            
            // Decrement n times.  If there are fewer than n entries, start at
            // the first entry and show entire history.  nextArgVal is n.
            // Otherwise, history_start should be 1 for the start.
            if (nextArgVal < history_start)
                history_start = history_start - nextArgVal;
            else
                history_start = 1;
        }
    }
    
    // Iterate over all of the history starting at the value we decided to
    // start at, either by default or from an argument.  The start value is
    // one-indexed, so we must decrement it before altering the pointer start.
    cmd_history += history_start - 1;
    while (*cmd_history != NULL) {
        printf("%d\t%s\n", history_start, (*cmd_history)->line);
        cmd_history++;
        history_start++;
    }
    
    return;
}

/**
 * @function    rerun_cmd_n
 * 
 * @abstract    Runs the n-th command in history.
 * @discussion  Given a command of the form !n [args], it runs the n-th
 *              command in history.  If n is positive, it runs as normal.
 *              If it is negative, it counts its offset from the newest
 *              command in history.  If the absolute value of n is 0 or greater
 *              than the total number of commands, nothing happens.  If n is
 *              non-integer, nothing happens.  If nothing happens, a message is
 *              printed to explain why.  Any arguments included are appended to
 *              the n-th command.
 * 
 * @param       command     The command that starts with a '!' character and
 *              contains which line to rerun.
 * @result      Runs the n-th command.
 */
void rerun_cmd_n(Command* command) {
    // Get the history so we can rerun a line.
    HIST_ENTRY **cmd_history = history_list();
    
    // Find the number of commands.
    int num_commands = 0;
    HIST_ENTRY **temp = cmd_history;
    while (*temp != NULL) {
        num_commands++;
        temp++;
    }
    
    // Figure out which line we are supposed to run from.  This is the arg
    // after the '!', or the argument starting at the first index.
    char* argument = command->args->arg + 1;
    int iarg = atoi(argument);
    
    // Check that the argument is in range.
    if ((iarg == 0) || (iarg > num_commands) || (-1 * iarg > num_commands)) {
        printf("%s: event not found.\n", command->args->arg);
        return;
    }
    
    // If a negative number given, we rerun counting from the end.  If positive
    // we rerun counting from the beginning.  In any case, if the absolute
    // value is more than the number of commands we have or is 0, nothing can
    // be done.
    int run_offset = iarg - 1;
    if (iarg < 0) {
        // The offset should be subtracted from the end (remember iarg < 0).
        run_offset = num_commands + iarg - 1;
    }
    
    // Create a command string from this and any arguments given to !n.
    char next_cmd[MAX_COMMAND_SIZE];
    strcpy(next_cmd, (*(cmd_history + run_offset))->line);
    
    // Put the rest of the arguments from !n onto the command string.
    argList *iter = command->args->nextArg;
    while (iter != NULL) {
        strcat(next_cmd, " ");
        strcat(next_cmd, iter->arg);
        iter = iter->nextArg;
    }
    
    // Run this new command.
    runCommand(next_cmd);
    
    return;
}

/**
 *
 */
void executeCommand(Command* command, int inPipe) {
    if (strcmp(command->args->arg, "exit") == 0) {
        printf("Exiting\n");
        exit(0);
    }
    else if (strcmp(command->args->arg, "cd") == 0 || strcmp(command->args->arg, "chdir") == 0) {
        // Change directories
        char* path;
        if (command->args->nextArg == NULL) {
            char buff[PATH_MAX];
            char* username = getpwuid(getuid())->pw_name;
            sprintf(buff, "/home/%s", username);
            path = buff;
        }
        else {
            path = command->args->nextArg->arg;
        }
        if (chdir(path) == -1) {
            printf("%s\n", strerror(errno));
        }
        return;
    }
    else if (strcmp(command->args->arg, "history") == 0) {
        // Print history or error as to why that cannot be done.
        print_sh_history(command);
        return;
    }
    else if (command->args->arg[0] == '!') {
        // If !n is argued, rerun the n-th line in history.
        rerun_cmd_n(command);
        return;
    }

    // Create a pipe if there is a next command
    int pipefd[2] = {-1, -1};
    if (command->pipe_destination != NULL) {
        if (pipe(pipefd) == -1){
            printf("%s\n", strerror(errno));
        }
    }
    
    pid_t pid = fork();
    if (pid < 0) {
       printf("Fork failed!\n");
       exit(1);
    }
    else if (pid == 0) {
        char** args = translateArgs(command->args);
        
        // Load the input and output
        // Use the pipe if it exists 
        // (output and input take precedence over pipe)

        int in_fd, out_fd;

        if (inPipe > -1) {
            dup2(inPipe, STDIN_FILENO);
            close(inPipe);
        }
        else if (command->input != NULL) {
            in_fd = open(command->input, O_RDONLY);
            if (in_fd == -1) {
                printf("Failed to open input file \n");
                return;
            }
            dup2(in_fd, STDIN_FILENO); /* Replace stdin */
            close(in_fd);  
        }
        if (pipefd[1] > -1) {
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
        }
        else if (command->output != NULL) {
            out_fd = open(command->output, O_CREAT | O_TRUNC | O_WRONLY,
                S_IRUSR | S_IRGRP | S_IROTH);
            if (out_fd == -1) {
                printf("Failed to open output file \n");
                return;
            }
            dup2(out_fd, STDOUT_FILENO); /* Replace stdout */
            close(out_fd);
        }

        if (execvp(args[0], args) == -1) {
            printf("Could not execute %s!\n", args[0]);
            exit(1);
        }
    }
    else {
        // Recurse on the next command if there is one, pass along the pipe
        if (inPipe > -1) {
            close(inPipe);
        }
        if (pipefd[1] > -1) {
            close(pipefd[1]);
        }
        if (command->pipe_destination != NULL) {
            executeCommand(command->pipe_destination, pipefd[0]);
        }

    } 
}


/**
 * @function    runCommand
 * 
 * @abstract    Parses and executes a command given a NULL terminated string.
 * @discussion  Parses the command and adds it to the history if it is
 *              nontrivial.
 * 
 * @param       cmd_buffer      Pointer to NULL terminated string containing
 *                              the user input.
 * @result      Returns nothing, but runs a command, most likely printing to
 *              the terminal.
 */
void runCommand(char* cmd_buffer) {
    Command *command;
    yyscan_t scanner;
    YY_BUFFER_STATE state;
        
    // Check if there is anything in the line.  It is useless to store
    // a history of empty lines.  If it is not empty, then add it to our
    // history.  Also don't want to record line rerun calls.
    if ((cmd_buffer[0] != 0) && (cmd_buffer[0] != '!')) {
        add_history(cmd_buffer);
    }

    state = yy_scan_string(cmd_buffer);

    if (yyparse(&command, scanner)) {
        printf("PARSING ERROR!\n");
        deleteCommand(command);
        return;
    }

    printCommand(command);
    executeCommand(command, -1);
    deleteCommand(command);

    // Wait on all child processes
    while (wait(NULL) > 0) {}

    yy_delete_buffer(state);

    yylex_destroy();
    
    return;
}


int main(int argc, char** argv) {
    char *command_buffer;
    char path_buffer[PATH_MAX];
    char prompt_buffer[PROMPT_MAX];

    while (1) {
        char* username = getpwuid(getuid())->pw_name;
        getcwd(path_buffer, PATH_MAX);
        
        // Compile the prompt that the user sees for entering commands.  This
        // will look like [username]:[current_path]>
        strcpy(prompt_buffer, username);
        strcat(prompt_buffer, ":");
        strcat(prompt_buffer, path_buffer);
        strcat(prompt_buffer, "> ");
        
        // Read from the command line and store it in command_buffer.  This
        // calls calloc on the return value, so we must free command_buffer
        // later.
        command_buffer = readline(prompt_buffer);
        
        // To allow multiline commands, keep reading commands until we get a
        // nonempty command whose last character is not a backslash.
        int end_cmd = 0;
        while (!end_cmd) {
            // Figure out the last character
            char *curr_char = command_buffer;
            char last = *command_buffer;
            while (*curr_char != '\0') {
                last = *curr_char;
                curr_char++;
            }
            
            // If the last character is a backslash, continue listening for
            // command.  Otherwise, break so we can parse.
            if (last == '\\') {
                *(curr_char - 1) = '\0';
                char *next = readline("> ");
                strcat(command_buffer, next);
                free(next);
            }
            else {
                end_cmd = 1;
            }
        }
            
        // Run the parsing and execution of this command string.
        runCommand(command_buffer);
        
        // Must delete the command buffer that readline allocated memory for.
        free(command_buffer);
    }

    return 0;

}
