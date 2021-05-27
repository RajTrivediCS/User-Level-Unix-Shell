/*
 * Author: Raj Trivedi
 * Partner Name: James Cooper
 * Date: March 13th, 2021
 *
 * This is the simple program that prints all the files in the given directory
 */
#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<dirent.h>

/*
 * This function will list all the files in a given directory
 * This is a helper function for implementing "list" command functionality of our Shell
 */
void list(char *dir){
	DIR *dirstream = opendir(dir);
	struct dirent *dp;

	// Unable to open directory stream
	if(!dirstream)
		return;
	while((dp = readdir(dirstream)) != NULL){
		// d_name is one of the fields of structure "dirent" which represents the name of file 
		printf("%s\n", dp->d_name);
	}

	// Close the directory stream
	closedir(dirstream);
}
