/*
 * Author: Raj Trivedi
 * Partner Name: James Cooper
 * Date: March 13th, 2021
 *
 * This is the simple program that just keeps track of whether the env variable is new or an existing one and modifies "dynamic envvariables"
 *   - If the env variable is new, then it will add it our global variable "dynamic_envvariables" at the end
 *   - However, if it's an existing one, then it will modify the value of existing env variable with the provided new value within "dynamic_envvariables"
 */

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
extern char **dynamic_envvariables;

// This is a helper function for implementing "setenv" command functionality of our Shell
void setenvvariable(char *varname,char *varvalue){

	// Flag to keep track of same environmental name from list
	int has_same_name = 0;

	// Size of arg[1] and arg[2]	
	int arg1len = (int) strlen(varname);
	int arg2len = (int) strlen(varvalue);

	// This is a variable to check if the environment variable name already exists
	char namecheck[arg1len + 1];
	strcpy(namecheck,varname);
	int index = 0;
	while(dynamic_envvariables[index] != NULL){
		// Get the name of current environment variable(just the name and not both)
		int namelen = 0;
		for(int counter = 0; dynamic_envvariables[index][counter] != '='; counter++){
			namelen++;
		}
		char *name = (char *) malloc(sizeof(char) * (namelen + 1));
		strncpy(name,dynamic_envvariables[index],namelen);
		name[namelen] = '\0';

		// Compare both environment variable names to see if they are equal
		// And if they are, then manage pointers for "dynamic_envvariables" and its current pointer value accordingly	 
		// Treat "dynamic_envvariables" like a linked list to expand or shrink the size at runtime
		if(strcmp(name,namecheck) == 0){
			// Remove the current value for the environment variable before assigning a new one
			free(dynamic_envvariables[index]);

			// Allocate new space for the environment variable in heap memory
			// Assign name and value for the environment variable
			dynamic_envvariables[index] = (char *) malloc(sizeof(char) * (arg1len + arg2len + 2));
			strcpy(dynamic_envvariables[index],varname);
			strcat(dynamic_envvariables[index],"=");
			strcat(dynamic_envvariables[index],varvalue);

			has_same_name = 1;
		}
		// Free the local dyncamical allocation
		free(name);
		index++;
	}
	// If the name does not exist in environment variable list, then create a new environment variable at the end
	if(dynamic_envvariables[index] == NULL && !has_same_name){
		// Allocate a space for the new environment variable
		// Assign name and a value for the new environment variable
		dynamic_envvariables[index] = (char *) malloc(sizeof(char) * (arg1len + arg2len + 2));
		strcpy(dynamic_envvariables[index],varname);
		strcat(dynamic_envvariables[index],"=");
		strcat(dynamic_envvariables[index],varvalue);
	}
	// Sets the next element of "dynamic_envvariables" to NULL for marking the end point
	dynamic_envvariables[index + 1] = NULL;

}
