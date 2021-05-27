/*
 * Author: Raj Trivedi
 * Partner Name: James Cooper
 * Date: March 13th, 2021
 *
 * This is the simple header file that collects all the function prototypes and constants necessary for us to implement the Shell
 */

#include "get_path.h"

void process_id();
void setenvvariable(char *varname, char *varvalue);
char *which(char *command, struct pathelement *pathlist);
char **where(char *command, struct pathelement *pathlist);
void list(char *dir);
void printenv(char **envp);

#define PROMPTMAX 64
#define MAXARGS   16
#define MAXLINE   128
#define MAXENVVARIABLES 128
