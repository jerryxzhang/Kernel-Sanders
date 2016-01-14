%{
 
/*
 * Lexer.l file
 */

typedef void* yyscan_t;

#include "Command.h"
#include "Parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
 
%}

%option outfile="Lexer.c" header-file="Lexer.h"

PIPE      "|"
IN        "<"
OUT       ">"

/* One or more of any character not reserved or whitespace. */
NAME      [^|<>\" \r\n\t]+

/* Any number of non quote characters, between two quotes. */
QNAME     \"[^\"]*\"

/* Any whitespace. */
WS        [ \r\n\t]*
 
%%
 
{WS}            { /* Skip blanks. */ }
 
{PIPE}      { return TOKEN_PIPE; }
{IN}          { return TOKEN_IN; }
{OUT}        { return TOKEN_OUT; }

{QNAME}        {
    // Strip the quotes on the string before returning
    char *new_name = (char*) calloc(strlen(yytext) - 1, sizeof(char));
    strncpy(new_name, yytext + 1, strlen(yytext) - 2);
    yylval.name = new_name;
    return TOKEN_NAME; }
{NAME}        {
    // yytext can change, so its contents must be copied
    char *new_name = (char*) calloc(strlen(yytext) + 1, sizeof(char));
    strcpy(new_name, yytext);
    yylval.name = new_name;
    return TOKEN_NAME; }
 
%%
