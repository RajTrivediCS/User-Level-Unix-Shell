/*
 * Author: Raj Trivedi
 * Partner Name: James Cooper
 * Date: March 13th, 2021
 *
 * This is the simple program that implements "where" command functionality of our Shell
 */

#include "get_path.h"
#define BUFFERSIZE 128

// This is the helper function for implementing "where" command
char **where(char *command, struct pathelement *p)
{
  char cmd[64], **ch = (char **) malloc(sizeof(char *) * BUFFERSIZE);
  int index;
  int  found;

  index = 0;
  found = 0;
  while (p) {
    sprintf(cmd, "%s/%s", p->element, command);
    if (access(cmd, X_OK) == 0) {
      found = 1;
      ch[index] = (char *) malloc(sizeof(char) * (strlen(cmd) + 1));
      strcpy(ch[index],cmd);
      index++;
    }
    p = p->next;
  }
  if(index){
	  ch[index] = NULL;
  }

  if(!found){
	  free(ch);
	  return (char **) NULL;
  }
  else
	  return ch;
}

