#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#define ERR -1

#define RESET "\x1b[0m"

#define BLACK   "\033[30m"  
#define WHITE   "\033[37m"
#define BLUE    "\x1b[34m"
#define YELLOW  "\x1b[33m"
#define GREEN   "\033[0;32m"
#define RED     "\033[31m" 
#define MAGENTA "\033[35m" 
#define CYAN    "\033[36m" 

#define BOLDBLACK   "\033[1m\033[30m"
#define BOLDRED     "\033[1m\033[31m"
#define BOLDGREEN   "\033[1m\033[32m"
#define BOLDYELLOW  "\033[1m\033[33m"
#define BOLDBLUE    "\033[1m\033[34m"
#define BOLDMAGENTA "\033[1m\033[35m"
#define BOLDCYAN    "\033[1m\033[36m" 
#define BOLDWHITE   "\033[1m\033[37m"

int getNbDigit(int nb);
char* repeatStrtok(int nb, char* separators);
void repeatPrintf(int nb, char* string);
bool charIn(char,char*);
