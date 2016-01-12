%{
 
/*
 * Lexer.l file
 */
 
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
 
NAME      [^|<>\" \r\n\t]+
QNAME     \"[^\"]*\"
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
    char *new_name = (char*) calloc(strlen(yytext) + 1, sizeof(char));
    strcpy(new_name, yytext);
    yylval.name = new_name;
    return TOKEN_NAME; }
 
%%
