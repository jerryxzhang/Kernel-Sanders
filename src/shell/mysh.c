typedef void* yyscan_t;

#include <unistd.h>
#include <pwd.h>
#include <string.h>
#include <stdio.h>
#include <linux/limits.h>
#include <ctype.h>
#include <sys/wait.h>
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
 *
 */
void executeCommand(Command* command, FILE* pipe) {
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

    // Create a pipe if there is a next command
    
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

        if (execvp(args[0], args) == -1) {
            printf("Could not execute %s!\n", args[0]);
            exit(1);
        }
    }
    else {
        // Recurse on the next command if there is one, pass along the pipe
    } 
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
        
        // Check if there is anything in the line.  It is useless to store
        // a history of empty lines.  If it is not empty, then add it to our
        // history.
        if (command_buffer[0] != 0)
            add_history(command_buffer);
        
        Command *command;
        yyscan_t scanner;
        YY_BUFFER_STATE state;
 
        state = yy_scan_string(command_buffer);
 
        if (yyparse(&command, scanner)) {
            printf("PARSING ERROR!\n");
            deleteCommand(command);
            continue;
        }

        printCommand(command);
        executeCommand(command, NULL);
        deleteCommand(command);
        
        // Must delete the command buffer that readline allocated memory for.
        free(command_buffer);

        // Wait on all child processes
        while (wait(NULL) > 0) {}

        yy_delete_buffer(state);
 
        yylex_destroy();
    }

    return 0;

}
