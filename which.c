/*
 * Author: Raj Trivedi
 * Partner Name: James Cooper
 * Date: March 13th, 2021
 *
 * This is the simple program that implements the "which" command functionality of our Shell
 */

#include "get_path.h"

// This is the helper function for implementing "which" command
char *which(char *command, struct pathelement *p)
{
  char cmd[64], *ch;
  int  found;

  found = 0;
  while (p) {       
    sprintf(cmd, "%s/%s", p->element, command);
    if (access(cmd, X_OK) == 0) {
      found = 1;
      break;
    }
    p = p->next;
  }
  if (found) {
    ch = malloc(strlen(cmd)+1);
    strcpy(ch, cmd);
    return ch;
  }
  else
    return (char *) NULL;
}
