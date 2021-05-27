/*
 * Author: Raj Trivedi
 * Partner Name: James Cooper
 * Date: March 13th, 2021
 *
 * This is the simple program that prints name and value of ALL environment variables given in pointer to char pointers array "envp"
 */

#include<stdio.h>

// This is a helper function for implementing both "printenv" and "setenv" commands functionality of our Shell
void printenv(char **envp){
        // Print ALL environment variable names with its associated values
	for(int index = 0; envp[index] != NULL; index++)
		printf("%s\n",envp[index]);
}
