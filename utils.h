#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <stdio.h>

void printMessage(const char *tag, const char *msg)
{
	printf("\x1B[38;2;200;64;200m" "[%s]" "\x1B[0m " "%s\n", tag, msg);
}

void printError(const char *name, const char *msg)
{
	perror(name);
	printf("%s\n", msg);
}

#endif
