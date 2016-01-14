%{
 
/*
 * Parser.y file
 */
 
#include "Command.h"
#include <stdio.h>
#include <string.h>

typedef void* yyscan_t;

void yyerror(Command** command, yyscan_t scanner, const char *str)
{
}
 
int yywrap()
{
        return 1;
} 
 
%}

%output  "Parser.c"
%defines "Parser.h"
 
%lex-param   { yyscan_t scanner }
%parse-param { Command **command }
%parse-param { yyscan_t scanner }

%union {
    char *name;
    Command *com;
    argList *args;
}

%left "|" TOKEN_PIPE

%token TOKEN_PIPE
%token TOKEN_IN
%token TOKEN_OUT
%token <name> TOKEN_NAME

%type <com> command
%type <com> commands
%type <args> names
 
%%

input
    : commands { *command = $1; }
    ;

names
    : TOKEN_NAME {
        $$ = createArgList($1, NULL);
    }
    | TOKEN_NAME names {
        $$ = createArgList($1, $2);
    }
    ;

command
    : names { $$ = createCommand($1->arg, $1, NULL, NULL, NULL); }
    | names TOKEN_IN TOKEN_NAME { $$ = createCommand($1->arg, $1, $3, NULL, NULL); }
    | names TOKEN_OUT TOKEN_NAME { $$ = createCommand($1->arg, $1, NULL, $3, NULL); }
    | names TOKEN_IN TOKEN_NAME TOKEN_OUT TOKEN_NAME { $$ = createCommand($1->arg, $1, $3, $5, NULL); }
    | names TOKEN_OUT TOKEN_NAME TOKEN_IN TOKEN_NAME { $$ = createCommand($1->arg, $1, $5, $3, NULL); }
    ;

commands
    : command { $$ = $1; }
    | command TOKEN_PIPE commands { $$ = pipeCommand($1, $3); }
    ;

%%
