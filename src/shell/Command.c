/*
 * Command.c
 * Implementation of functions used to build the syntax tree.
 */

#include "Command.h"

#include <stdio.h>
#include <stdlib.h>

Command *createCommand(char *name, argList *args, char *input, char *output, Command *pipe)
{
    Command *b = (Command *)calloc(1, sizeof(Command));

    if (b == NULL)
        return NULL;

    b->program_name = name;
    b->args = args;
    b->input = input;
    b->output = output;
    b->pipe_destination = pipe;

    return b;
}

void deleteCommand(Command *b)
{
    if (b == NULL)
        return;

    deleteCommand(b->pipe_destination);
    free(b->input);
    free(b->output);

    while (b->args != NULL) {
        argList* next = b->args->nextArg;
        free(b->args->arg);
        free(b->args);
        
        b->args = next;
    }

    free(b);
}

Command *pipeCommand(Command *c1, Command *c2) {
    c1->pipe_destination = c2;
    return c1;
}

argList *createArgList(char *arg, argList *nextArgs) {
    argList * args = (argList *) calloc(1, sizeof(argList));
    args->arg = arg;
    args->nextArg = nextArgs;
    return args;
}

void printCommandHelper(Command *command, int indent) {
    int i;
    if (command == NULL) return;

    char *name = (command->program_name == NULL) ? "NULL" : command->program_name;
    char *input = (command->input == NULL) ? "NULL" : command->input;
    char *output = (command->output == NULL) ? "NULL" : command->output;
    for (i = 0; i < indent; i++) {
        printf("    ");
    }
    printf("NAME:%s\n", name);
    for (i = 0; i < indent; i++) {
        printf("    ");
    }
    printf("ARGS:");
    argList *args = command->args;
    while (args != NULL) {
        printf(" %s", args->arg);
        args = args->nextArg;
    }
    printf("\n");

    for (i = 0; i < indent; i++) {
        printf("    ");
    }
    printf("INPUT:%s\n", input);
    for (i = 0; i < indent; i++) {
        printf("    ");
    }
    printf("OUTPUT:%s\n", output);
    printCommandHelper(command->pipe_destination, indent + 1);
}

void printCommand(Command *command) {
    printCommandHelper(command, 0);
}

