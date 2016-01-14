/*
 * Command.h
 */

/**
 * @brief The command struct.
 */
typedef struct tagCommand
{
    char *program_name;
    struct argListStruct *args;

    char *input;
    char *output;

    struct tagCommand* pipe_destination; 
} Command;

/**
 * A linked list of arguments
 */
typedef struct argListStruct
{
    char *arg;
    struct argListStruct *nextArg;
} argList;

/**
 * @brief It creates a command
 * @param name - name of program to run
 * @param args - any arguments, NULL terminated
 * @param input - a file to redirect into stdin, or NULL if none
 * @param output - a file to redirect stdout to, or NULL if none
 * @param pipe - the command that the output of this command is piped to 
 * @return The expression or NULL in case of no memory
*/
Command *createCommand(char *name, argList *args, char *input, char *output, Command *pipe);

/**
 * Sets the pipe destination of c1 to c2.
 */
Command *pipeCommand(Command *c1, Command *c2);

/**
 * Frees all memory associated with the given command and all commands it links to.
 */
void deleteCommand(Command *b);


/**
 * Adds a new element to the beginning of an argList.
 */
argList *createArgList(char* arg, argList* nextArgs);

/**
 * Prints the contents of the Command struct in a pretty indented way.
 */
void printCommand(Command *command);
