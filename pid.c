/*
 * Author: Raj Trivedi
 * Partner Name: James Cooper
 * Date: March 13th, 2021
 *
 * This program prints out the Process ID(PID) of our running shell
 */

#include<stdio.h>
#include<sys/types.h>
#include<unistd.h>

void process_id(){
	printf("PID of the shell: %d\n",getpid());
}
