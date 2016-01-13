
FILES = mysh.c Command.c Lexer.c Parser.c
CC = gcc
CFLAGS = -Wall -g
READLINE = -lreadline

all: mysh

mysh: $(FILES)
	$(CC) $(CFLAGS) $^ -o mysh $(READLINE)

Lexer.c:	Lexer.l 
		lex Lexer.l

Parser.c:	Parser.y Lexer.c
		yacc -d Parser.y

clean:
		rm -f *.o *~ Lexer.c Lexer.h Parser.c Parser.h mysh

.PHONY: all clean


