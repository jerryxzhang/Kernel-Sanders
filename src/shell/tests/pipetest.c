#include <stdio.h>

int main(int argc, char **argv) {
	char buff[100];
	fgets(buff, 100, stdin);
	printf("RECEIVED IN PIPE: %s", buff);
	return 0;

}
