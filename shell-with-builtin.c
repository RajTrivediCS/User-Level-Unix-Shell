/*
 * Author: Raj Trivedi
 * Assignment Name: Adding More Features to the Shell
 * Partner Name: James Cooper
 * Date: March 29th, 2021
 *
 * This is the main program that will gather all the codes from other C files and implements a basic Shell
 */

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <glob.h>
#include <errno.h>
#include <sys/wait.h>
#include <pthread.h>
#include <utmpx.h>
#include "sh.h"

#define SLEEP_TIME 20
#define READ_END 0
#define WRITE_END 1
#define NAMESIZE  32

// Definition for a node of linked list "watchuser_list"
struct user_node {
	char user[NAMESIZE];
	struct user_node *next;
};

/* MUTEX object */
pthread_mutex_t m; 

// This is the dynamically allocated 2D array like variable that stores all the environment variables of the system
// This global variable will also keep track of which new env variables are added or existing env variables are modified
char **dynamic_envvariables;

// "watchuser_list" is the dynamically allocated linked list that stores the users that needs to be watched on
struct user_node *watchuser_list, *tail;

/* Helper function for watchuser command 
 * This function adds the given username to the global linked list
 */
void addUser(char *username){

	// Check if the linked list is empty or not
	// If it is, then this will be the first user added in the linked list
	if(watchuser_list == NULL){
		watchuser_list = (struct user_node *) malloc(sizeof(struct user_node));
		strcpy(watchuser_list -> user, username);
		watchuser_list -> next = NULL;
		tail = watchuser_list;
	}

	// This assumes that ATLEAST ONE user is present in the linked list
	// If that's the case, then just append the given user into the linked list
	else{
		struct user_node *tmp = (struct user_node *) malloc(sizeof(struct user_node));
		strcpy(tmp -> user, username);
		tmp -> next = NULL;
		tail -> next = tmp;
		tail = tmp;
	}
}

/* Helper function for watchuser command
 * This function removes the given username from the global linked list given that the list is not empty
 */
void removeUser(char *username){
	
	if(watchuser_list != NULL) {
		// Pointer to "user_node" pointers
		// In other words, "indirect" stores the address of each "user_node" pointer
		// Thus, (*indirect) = NODE OF THE LINKED LIST

		struct user_node **indirect = &watchuser_list;
		struct user_node *tmp;
		while((*indirect) != NULL) {

			// If an entry is found, then do following steps:
			//      1. Access the current "user_node" using "(*indirect)" notation and replace the current "user_node" with its "next node"                
			//      2. Before replacing, store the current "user_node" in a buffer to free up its heap space later
			if(strcmp((*indirect) -> user, username) == 0){
				tmp = *indirect;
				*indirect = (*indirect) -> next;
				free(tmp);
			}
	
			// Else move on with "next node"
			else{
				// Make the "indirect" point to "next node"
				indirect = &((*indirect) -> next);
			}
		}
	}

	else
		printf("Watchuser List is empty...\n");
}

/* Helper function for watchuser command
 * This function checks if a given user is present in the global linked list
 * Returns 1 on success and 0 if not present
 */
int searchUser(char *username){
	struct user_node *tmp = watchuser_list;
	while(tmp){
		if(strcmp(tmp->user, username) == 0)
			return 1;
		tmp = tmp -> next;
	}
	return 0;
}

void sig_handler(int sig)
{
	printf("\n");
	char *cwd_prompt_prefix;
        cwd_prompt_prefix = getcwd(NULL,0);
        fprintf(stdout, " [%s]> ",cwd_prompt_prefix); /* print prompt */
        free(cwd_prompt_prefix);
        fflush(stdout);

}

/* This function is a SIGCHLD handler function that reaps out MULTIPLE zombie processes that came from "bg" command */
void sigchld_handler(int sig){
	while (waitpid((pid_t)(-1), 0, WNOHANG) > 0);
}

/*
 * This function frees all the dynamically allocated environment variables(env) from heap memory
 */
void free_dynamic_envvariables(){
	int index = 0;
	while(dynamic_envvariables[index] != NULL){
		free(dynamic_envvariables[index]);
		index++;
	}
	free(dynamic_envvariables);
}

/*
 * This function frees all the dynamically allocated space for PATH environment variable from the heap memory
 * Remember, PATH is another linked list like data structure that stores paths of directories
 */
void free_path(struct pathelement *pathlist){
        struct pathelement *tmp = pathlist;
        while(tmp != NULL){
                pathlist = pathlist->next;
                free(tmp->element);
                free(tmp);
                tmp = pathlist;
        }
}


/* This is the thread function that watchuser thread executes upon calling "watchuser" command 
 * This thread function gets the list of users from a global linked list "watchuser_list"
 */
void *thread_function(){
	struct utmpx *up;
	struct user_node *tmp = watchuser_list;
	while(1){		
		/* Get the list of users from watchlist to watch on */	
	      	setutxent();			/* start at beginning */
	       	while (up = getutxent()) {	/* get an entry */ 
		    	if ( up->ut_type == USER_PROCESS ) {	/* only care about users */
				
				// Locks the critical section to avoid race condition
				pthread_mutex_lock(&m);
				while(tmp){
					if(strcmp(tmp->user, up->ut_user) == 0) {
						printf("%s has logged on %s from %s\n", up->ut_user, up->ut_line, up ->ut_host);
					}

					tmp = tmp->next;
				}
				pthread_mutex_unlock(&m);
		    	}
			tmp = watchuser_list;
	      	}

		/* Sleeps for SLEEP_TIME(20 seconds) */
		sleep(SLEEP_TIME);
	}
}


int
main(int argc, char **argv, char **envp)
{
	char   *bg_arg[MAXARGS]; /* arguments for bg command */
	int     bg_number;       /* Background process number */
	char	buf[MAXLINE];
	int     buflen;
	char    *arg[MAXARGS];  // an array of tokens
	char    *ptr;
        char    *pch;
	pid_t	pid;
	int	status, i, arg_no, background, noclobber;
	int 	redirection, append, piping, rstdin, rstdout, rstderr;
	char    *cwd_prompt_prefix; // stores current working directory in a pointer to print it out as a prefix of the prompt of shell
	char    prompt_command_prefix[MAXLINE];
	int     prompt_command_flag = 0;
	        dynamic_envvariables = (char **) malloc(sizeof(char *) * MAXENVVARIABLES); // this is the max size for storing env variables
	int	index = 0;
	struct  pathelement *pathlist;
	int     oldpwd_flag = 1;       // keeps track of when to change from OLDPWD env value to PWD env value or vice versa
			               // this flag will ONLY be used if "cd -" command is used
	
	int     count_watchuser_runs; /* Count of watchuser command runs */

	pthread_t *thread_handles; /* Buffer for watchuser thread */ 

	noclobber = 0;             // initially default to 0
	watchuser_list = NULL;     /* Initially default linked list to NULL */
	bg_number = 0;             /* initially default to 0 */
	count_watchuser_runs = 0;

	// Dynamically allocates space in heap memory for our global variable "dynamic_envvariables"
	// It also stores contents from pointer to char pointers array "envp" into our global variable "dynamic_envvariables"
	for( ; envp[index] != NULL; index++){
		dynamic_envvariables[index] = (char *) malloc(sizeof(char) * (strlen(envp[index]) + 1));
		strcpy(dynamic_envvariables[index],envp[index]);
	}

	// NULL value is assigned to mark the end of number of environment variables in our global variable "dynamic_envvariables"
	dynamic_envvariables[index] = NULL;


        signal(SIGINT,  sig_handler); /* INTERRUPT SIGNAL  ; happens when user presses CTRL-C; catches the signal from CTRL-C and continues from next prompt */
	signal(SIGTSTP, sig_handler); /* STOP SIGNAL       ; happens when user presses CTRL-Z; catches the signal from CTRL-Z and continues from next prompt */
	signal(SIGTERM, sig_handler); /* TERMINATE SIGNAL  ; happens when "kill PID" command is given; Shell itself ignores this signal but could
					                                                               pass SIGTERM signal to another process with specified PID */

	cwd_prompt_prefix = getcwd(NULL,0);
	fprintf(stdout, " [%s]> ",cwd_prompt_prefix); /* print prompt */
	free(cwd_prompt_prefix);
	fflush(stdout);

	/* Initializes the MUTEX object */
	if(pthread_mutex_init(&m, NULL) != 0){
		printf("Error Initializing the MUTEX..\n");
	}
	

	// Checks for END-OF-FILE CHARACTER(CTRL-D)
	// If END-OF-FILE CHARACTER provided, then shell would repeatedly remind user to use "exit" to leave
	while(fgets(buf, MAXLINE, stdin) == NULL){

		printf("\n");
		printf("Use \"exit\" to leave shell.\n");
		cwd_prompt_prefix = getcwd(NULL,0);
		fprintf(stdout, " [%s]> ",cwd_prompt_prefix); /* print prompt */
		free(cwd_prompt_prefix);
		fflush(stdout);		
	}
	
	buflen = (int) strlen(buf);
	buf[buflen - 1] = '\0';

	while (strcmp(buf, "exit") != 0) {
		// This is where user types nothing and then presses "Enter" key upon prompt
		// Shell will ignore this command and move on from next line
		if (strlen(buf) == 1 && buf[strlen(buf) - 1] == '\n')
		  goto nextprompt;  // "empty" command line

	
		if (buf[strlen(buf) - 1] == '\n')
			buf[strlen(buf) - 1] = 0; /* replace newline with null */

		// no redirection or pipe
                redirection = append = piping = rstdin = rstdout = rstderr = 0;

                // check for >, >&, >>, >>&, <, |, and |&
                if (strstr(buf, ">>&"))
                  redirection = append = rstdout = rstderr = 1;
                else
                if (strstr(buf, ">>"))
                  redirection = append = rstdout = 1;
                else
                if (strstr(buf, ">&"))
                  redirection = rstdout = rstderr = 1;
                else
                if (strstr(buf, ">"))
                  redirection = rstdout = 1;
                else
                if (strstr(buf, "<"))
                  redirection = rstdin = 1;
                else
                if (strstr(buf, "|&"))
                  piping = rstdout = rstderr = 1;
                else
                if (strstr(buf, "|"))
                  piping = rstdout = 1;

		// parse command line into tokens (stored in buf)
		arg_no = 0;
                pch = strtok(buf, " ");

		// The following loop will successfully store all the tokens from "pch" into an array of strings called "arg"
                while (pch != NULL && arg_no < MAXARGS)
                {
		  arg[arg_no] = pch;
		  arg_no++;
                  pch = strtok (NULL, " ");
                }

		// Ends the array of strings "arg" with a NULL value
		// Remember, "arg" is of type array of strings where each string is stored as a pointer to char
		// Thus, you CAN initialize last element of "arg" with NULL to mark the endpoint
		arg[arg_no] = (char *) NULL;

		// User has not given any commands to command line
		// Shell will ignore this and start from next line
		if (arg[0] == NULL)  // "blank" command line
		  goto nextprompt;

                background = 0;      // not background process
                if (strcmp(arg[arg_no-1],"&") == 0){ // bg command			

			bg_number++;
			
			// Setup arguments for bg to call later
			int arg_index = 0;
			while(strcmp(arg[arg_index],"&") != 0){
				bg_arg[arg_index] = arg[arg_index];
				arg_index++;
			}
			bg_arg[arg_index] = NULL;

			background = 1;    // to background this command
		}

		// Interprocess Communications (IPC)
		// Piping mechanism
		if(piping){
			if(rstdout && rstderr){ // "|&"

				struct pathelement *path;
                                int pipechar_index, pid_command_1, pid_command_2;
                                int pipefd[2];
                                char *command_1, *command_2, *excmd_1, *excmd_2;

                                // Get PATH
                                path = get_path();

                                // Track the index of "|&"
                                for(pipechar_index = 0; strcmp(arg[pipechar_index],"|&") != 0; pipechar_index++);

                                // Get both the commands
                                command_1 = arg[0];                  /* First token of command line */
                                command_2 = arg[pipechar_index + 1]; /* Token immediately after "|&" */

                                // Create an unnamed pipe
                                if(pipe(pipefd) == -1){
                                        fprintf(stderr, "parent: Failed to create pipe\n");
                                        return -1;
                                }

                                // Fork a process to run command 2
                                  pid_command_2 = fork();

                                  if (pid_command_2 == -1) {
                                          fprintf(stderr, "parent: Could not fork process to run %s command\n",command_2);
                                          return -1;
                                  } else if (pid_command_2 == 0) {

                                          // fprintf(stdout, "child: %s child will now run\n",command_2);

                                          // Set fd[0] (stdin) to the read end of the pipe
                                          if (dup2(pipefd[READ_END], STDIN_FILENO) == -1) {
                                                  fprintf(stderr, "child: %s dup2 failed\n",command_2);
                                                  return -1;
                                          }

                                          // Close the pipe now that we've duplicated it
                                          close(pipefd[READ_END]);
                                          close(pipefd[WRITE_END]);


                                          /* Get full path for command 2 */
                                          excmd_2 = which(command_2, path);

                                          // Set up arguments to call
                                          char *new_argv[MAXARGS];
                                          int arg_index = pipechar_index + 2;
                                          int command2_args_index = 1;

                                          new_argv[0] = excmd_2;
                                          while(arg[arg_index]){
                                                  new_argv[command2_args_index] = arg[arg_index];
                                                  command2_args_index++;
                                                  arg_index++;
                                          }
                                          new_argv[command2_args_index] = NULL;

                                          // Environments would set to NULL

                                          // Call execve(2) which will replace the executable image of this process
                                          execve(excmd_2, new_argv, NULL);

                                          // Execution will never continue in this process unless execve returns because of an error
                                          fprintf(stderr, "child: Oops, %s failed!\n",command_2);
                                          return -1;
                                  }

                                  // Fork a process to run command 1
                                  pid_command_1 = fork();

                                  if (pid_command_1 == -1) {
                                          fprintf(stderr, "parent: Could not fork process to run %s\n",command_1);
                                          return -1;
                                  } else if (pid_command_1 == 0) {

                                         //  fprintf(stdout, "child: %s child will now run\n",command_1);
                                         //  fprintf(stdout, "---------------------\n");

                                          // Set fd[1] (stdout and stderr) to the write end of the pipe
                                          if (dup2(pipefd[WRITE_END], STDOUT_FILENO) == -1 || dup2(pipefd[WRITE_END], STDERR_FILENO) == -1) {
                                                  fprintf(stderr, "%s dup2 failed\n",command_1);
                                                  return -1;
                                          }

                                          // Close the pipe now that we've duplicated it
                                          close(pipefd[READ_END]);
                                          close(pipefd[WRITE_END]);

                                          // Check if the file exist AND if that file is executable
                                          if(access(command_1,F_OK) == 0 && access(command_1,X_OK) == 0){

                                                  // Set up arguments to call
                                                  char *new_argv[] = {command_1 , NULL};

                                                  // Call execve(2) which will replace the executable image of this process
                                                  execve(command_1, new_argv, NULL);
                                          }

                                          // Get the first instance of command through which(...) function
                                          else{
                                                  /* Get full path for command 1 */
                                                  excmd_1 = which(command_1, path);

                                                  // Set up arguments to call
                                                  char *new_argv[MAXARGS];
                                                  int arg_index = 1;
                                                  int command1_args_index = 1;

                                                  new_argv[0] = excmd_1;
                                                  while(strcmp(arg[arg_index],"|&") != 0){
                                                          new_argv[command1_args_index] = arg[arg_index];
                                                          command1_args_index++;
                                                          arg_index++;
                                                  }
                                                  new_argv[command1_args_index] = NULL;


                                                  // Environments would set to NULL

                                                  // Call execve(2) which will replace the executable image of this process
                                                  execve(excmd_1, new_argv, NULL);

                                                  // Execution will never continue in this process unless execve returns because of an error
                                                  fprintf(stderr, "child: Oops, %s failed!\n",command_1);
                                                  return -1;
                                          }
                                  }

                                  // Parent doesn't need the pipes
                                  close(pipefd[READ_END]);
                                  close(pipefd[WRITE_END]);


                                  // fprintf(stdout, "parent: Parent will now wait for children to finish execution\n");

                                  // Wait for all children to finish
                                  while (wait(NULL) > 0);

                                  // fprintf(stdout, "---------------------\n");
                                  // fprintf(stdout, "parent: Children has finished execution, parent is done\n");
                                  free(path);
                                  goto nextprompt;								
			}

			else{ // "|"		
				struct pathelement *path;	
				int pipechar_index, pid_command_1, pid_command_2;
				int pipefd[2];
				char *command_1, *command_2, *excmd_1, *excmd_2;							
				
				// Get PATH
				path = get_path();

				// Track the index of "|"
				for(pipechar_index = 0; strcmp(arg[pipechar_index],"|") != 0; pipechar_index++);

				// Get both the commands
				command_1 = arg[0];                  /* First token of command line */
				command_2 = arg[pipechar_index + 1]; /* Token immediately after "|" */

				// Create an unnamed pipe
				if(pipe(pipefd) == -1){
					fprintf(stderr, "parent: Failed to create pipe\n");
					return -1;
				}

				// Fork a process to run command 2
				  pid_command_2 = fork();

				  if (pid_command_2 == -1) {
				      	  fprintf(stderr, "parent: Could not fork process to run %s command\n",command_2);
				      	  return -1;
				  } else if (pid_command_2 == 0) {
			
					  // fprintf(stdout, "child: %s child will now run\n",command_2);
				      
					  // Set fd[0] (stdin) to the read end of the pipe
				      	  if (dup2(pipefd[READ_END], STDIN_FILENO) == -1) {
					    	  fprintf(stderr, "child: %s dup2 failed\n",command_2);
					    	  return -1; 
					  }
					
				      	  // Close the pipe now that we've duplicated it
				      	  close(pipefd[READ_END]);
				      	  close(pipefd[WRITE_END]);
				                                                                        

					  /* Get full path for command 2 */                                        
                                          excmd_2 = which(command_2, path);          				

					  // Set up arguments to call
					  char *new_argv[MAXARGS];
					  int arg_index = pipechar_index + 2;
					  int command2_args_index = 1;
					  
					  new_argv[0] = excmd_2;
					  while(arg[arg_index]){
						  new_argv[command2_args_index] = arg[arg_index];
						  command2_args_index++;
						  arg_index++;
					  }		
					  new_argv[command2_args_index] = NULL;
				      
					  // Environments would set to NULL

					  // Call execve(2) which will replace the executable image of this process
					  execve(excmd_2, new_argv, NULL);
				      
					  // Execution will never continue in this process unless execve returns because of an error 
				       	  fprintf(stderr, "child: Oops, %s failed!\n",command_2);
				      	  return -1;
				  }

				  // Fork a process to run command 1
				  pid_command_1 = fork();
				
				  if (pid_command_1 == -1) {
				      	  fprintf(stderr, "parent: Could not fork process to run %s\n",command_1);
				      	  return -1;
				  } else if (pid_command_1 == 0) {
				      	  
					 //  fprintf(stdout, "child: %s child will now run\n",command_1);
				      	 //  fprintf(stdout, "---------------------\n");
				      
					  // Set fd[1] (stdout) to the write end of the pipe
					  if (dup2(pipefd[WRITE_END], STDOUT_FILENO) == -1) {
					    	  fprintf(stderr, "%s dup2 failed\n",command_1);
					    	  return -1;
				      	  }
				      
					  // Close the pipe now that we've duplicated it
				      	  close(pipefd[READ_END]);
				      	  close(pipefd[WRITE_END]);				    					 

					  // Check if the file exist AND if that file is executable					  
					  if(access(command_1,F_OK) == 0 && access(command_1,X_OK) == 0){
						  
						  // Set up arguments to call
						  char *new_argv[] = {command_1 , NULL};

						  // Call execve(2) which will replace the executable image of this process
						  execve(command_1, new_argv, NULL);
					  }
					  
					  // Get the first instance of command through which(...) function
					  else{
						  /* Get full path for command 1 */     			  					
						  excmd_1 = which(command_1, path);  

						  // Set up arguments to call
						  char *new_argv[MAXARGS];
						  int arg_index = 1;
						  int command1_args_index = 1;
	
						  new_argv[0] = excmd_1;
						  while(strcmp(arg[arg_index],"|") != 0){
							  new_argv[command1_args_index] = arg[arg_index];
        	                                          command1_args_index++;
                	                                  arg_index++;
                        	                  }
                                	          new_argv[command1_args_index] = NULL;
                                    		 
				      
					  	  // Environments would set to NULL
						  
						  // Call execve(2) which will replace the executable image of this process
					  	  execve(excmd_1, new_argv, NULL);

						  // Execution will never continue in this process unless execve returns because of an error
					      	  fprintf(stderr, "child: Oops, %s failed!\n",command_1);
					      	  return -1;
					  }
				  }
		
				  // Parent doesn't need the pipes
				  close(pipefd[READ_END]);
  				  close(pipefd[WRITE_END]);

  
				  // fprintf(stdout, "parent: Parent will now wait for children to finish execution\n");
				
				  // Wait for all children to finish
				  while (wait(NULL) > 0);
			      
				  // fprintf(stdout, "---------------------\n");
			       	  // fprintf(stdout, "parent: Children has finished execution, parent is done\n");
				  free(path);
				  goto nextprompt;
			}
		}

		/* The following conditional statements checks which built-in command we have provided upon prompt */
		/* Executes that particular command thereafter */              

		
		if (strcmp(arg[0], "pwd") == 0) { // built-in command pwd 
		  printf("Executing built-in [pwd]\n");

		  // Prints current working directory on screen by calling getcwd(...) function
	          ptr = getcwd(NULL, 0);
                  
                  if (redirection) {
			  int fid;
			  if(noclobber){ // redirection with noclobber

                                  if (!append && rstdout && !rstderr) {  // ">"

					  // Check if the file already exists or not
					  // If it is, print the message refusing to overwrite 
					  if(access(arg[arg_no-1],F_OK) == 0){
						  printf("%s: File exists.\n",arg[arg_no-1]);
						  goto nextprompt;
					  }

					  // If it doesn't, then create a new file
					  else{
						  fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
					  }
 
				  }

                                  else if (!append && rstdout && rstderr){  // ">&"
                             
                                          // Check if the file already exists or not
                                          // If it is, print the message refusing to overwrite
                                          if(access(arg[arg_no-1],F_OK) == 0){
                                                  printf("%s: File exists.\n",arg[arg_no-1]);
						  goto nextprompt;
                                          }

                                          // If it doesn't, then create a new file
                                          else{
                                                  fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                          }					  
					  
				  }

                                  else if (append && rstdout && !rstderr) {   // ">>"
					  fid = open(arg[arg_no-1], O_WRONLY | O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

					  // Check if the file is already created to append the contents
					  // If not, then print the message refusing to create a new file
					  if(fid < 0){
						  printf("%s: No such file or directory.\n",arg[arg_no-1]);
						  goto nextprompt;
					  }
				  }
                                          

                                  else if (append && rstdout && rstderr) {   // ">>&"
                                          fid = open(arg[arg_no-1], O_WRONLY| O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);				

                                          // Check if the file is already created to append the contents
                                          // If not, then print the message refusing to create a new file		  			  
					  if(fid < 0){
						  printf("%s: No such file or directory.\n",arg[arg_no-1]);
						  goto nextprompt;
					  }
				  } 
			  }

			  else{	  // redirection without noclobber

    				  if (!append && rstdout && !rstderr)  // ">"
   					  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

                	          else if (!append && rstdout && rstderr)  // ">&"
                            		  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);
	      
				  else if (append && rstdout && !rstderr)   // ">>"
        	                          fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
	
                    		  else if (append && rstdout && rstderr)   // ">>&"
                            		  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
	      
			  }

		          /* Redirect STDOUT to file */
			  close(1); // Closes the file descriptor for STDOUT
			  dup(fid); // STDOUT points to new file descriptor "fid"	      


			  if(strcmp(arg[1],">&") == 0 || strcmp(arg[1],">>&") == 0){	      			

				  /* Redirect STDERR to file */
				  close(2); // Closes the file descriptor for STDERR
				  dup(fid); // STDERR points to new file descriptor "fid"
			  }	       			  
			  close(fid);	      			  

			  // Prints the CWD
			  printf("%s\n", ptr);	      			  

			  /* Redirect STDOUT back to terminal */
			  fid = open("/dev/tty", O_WRONLY);
			  close(1);
			  dup(fid);              
			  close(fid);			  
		  }
                   
		  
		  else
                    printf("%s\n", ptr);  // no redirection
		  

		  // Frees the space for pointer variable to avoid memory leak
                  free(ptr);
	        }

		else if (strcmp(arg[0], "watchuser") == 0) { // built-in command watchuser
			printf("Executing built-in [watchuser]\n");

			// Check if the watchuser has been runned for the first time
			// If that's the case, then create new watchuser thread via pthread_create(...)
			// This also makes sure that ONLY ONE watchuser thread should ever be running
			if(++count_watchuser_runs == 1){

				thread_handles = (pthread_t *) malloc(sizeof(pthread_t));

				/* Creates a watchuser thread executing thread_function() */
				pthread_create(thread_handles, NULL, &thread_function, NULL);
			}

                        // Check if second arg has been provided to watchuser command
                        // If not provided, then that means ONLY name of the user is given
                        // If that's the case, then just ADD that user into the "watchuser_list"
                        if(arg[2] == NULL) {

				// Check if user is already present in the global linked list
				// If user is not present, then add it to the global linked list
				if(!searchUser(arg[1]))
					addUser(arg[1]);

				// Else print error message saying that user is already present
				else
					printf("User %s is already present in the watchlist...\n",arg[1]);

                        }

                        // Check if "off" arg has been provided to watchuser command
                        // If provided, then remove the user in the first arg from the "watchuser_list"
                        else if(arg[2] != NULL && strcmp(arg[2], "off") == 0){
                                removeUser(arg[1]);
                        }

                        // This assumes that more than one user is provided for watchuser command
                        // This should produce an error
                        else{
                                printf("watchuser: Too many arguments.\n");
                        }

		}

                else if (strcmp(arg[0], "noclobber") == 0) { // built-in command noclobber
                  printf("Executing built-in [noclobber]\n");
                  noclobber = 1 - noclobber; // switch value
                  printf("%d\n", noclobber);
                }
		

		else if (strcmp(arg[0], "prompt") == 0){ // built-in prompt command
		  printf("Executing built-in [prompt]\n");

		  // Conditional Statements to check if prompt is given any arguments
		  
		  // This conditional statement assumes that no arguments are provided
		  // If no args are provided, then take input from user and store it as prefix in next line
		  if(arg[1] == NULL){
			  printf("input prompt prefix: ");
			  fgets(prompt_command_prefix,MAXLINE,stdin);
			  int len = (int) strlen(prompt_command_prefix);
			  prompt_command_prefix[len - 1] = '\0';
			  prompt_command_flag = 1;
		  }

		  // Check if second arg is given to "prompt" command or not
		  // If there is no second arg, then take the value of first arg and store it as prefix in next line
		  else if(arg[2] == NULL){
			 strcpy(prompt_command_prefix,arg[1]); 
			 prompt_command_flag = 1;
		  }

		  // This assumes that two or more than two args are provided
		  // Print an error message in this case
		  else{
			  printf("prompt: Too many arguments.\n");
		  }
		}

		else if (strcmp(arg[0],"pid") == 0){ // built-in pid command
			printf("Executing built-in [pid]\n");

      			if (redirection) { // redirection for "pid" command
				int fid;

                                if(noclobber){ // redirection with noclobber
					if (!append && rstdout && !rstderr) {  // ">"
      			
						// Check if the file already exists or not
      	                                        // If it is, print the message refusing to overwrite
            	                                if(access(arg[arg_no-1],F_OK) == 0){
      							printf("%s: File exists.\n",arg[arg_no-1]);
       							goto nextprompt;
      						}
      
						// If it doesn't, then create a new file
						else{
      							fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
      						}
      					}
      					else if (!append && rstdout && rstderr){  // ">&"
      						// Check if the file already exists or not
      	                                        // If it is, print the message refusing to overwrite
                                                if(access(arg[arg_no-1],F_OK) == 0){
							printf("%s: File exists.\n",arg[arg_no-1]);
							goto nextprompt;
	
						}

						// If it doesn't, then create a new file
						else{
      							fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
      	                                        }
      					}

       	                                else if (append && rstdout && !rstderr) {   // ">>"
   						fid = open(arg[arg_no-1], O_WRONLY | O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
      
						// Check if the file is already created to append the contents
						// If not, then print the message refusing to create a new file
						if(fid < 0){
      							printf("%s: No such file or directory.\n",arg[arg_no-1]);
      							goto nextprompt;
      						}
      					}
      					else if (append && rstdout && rstderr) {   // ">>&"
      						fid = open(arg[arg_no-1], O_WRONLY| O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
      
						// Check if the file is already created to append the contents
						// If not, then print the message refusing to create a new file
						if(fid < 0){
      							printf("%s: No such file or directory.\n",arg[arg_no-1]);
      							goto nextprompt;
      						}
      					}
      				}
      
				else{   // redirection without noclobber
      					if (!append && rstdout && !rstderr)  // ">"
      						fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);
      
					else if (!append && rstdout && rstderr)  // ">&"
      						fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);
       
					else if (append && rstdout && !rstderr)   // ">>
      						fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
      
					else if (append && rstdout && rstderr)   // ">>&"
      						fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
      				}
				


				/* Redirect STDOUT to file */
	    			close(1); // Closes the file descriptor for STDOUT
	    			dup(fid); // STDOUT points to new file descriptor "fid"

				if(strcmp(arg[1],">&") == 0 || strcmp(arg[1],">>&") == 0){

					/* Redirect STDERR to file */
					close(2); // Closes the file descriptor for STDERR
					dup(fid); // STDERR points to new file descriptor "fid"
				}
				close(fid);

				// Prints PID of the shell 
                                process_id();
	    			
				/* Redirect STDOUT back to terminal */
				fid = open("/dev/tty", O_WRONLY); 
            		        close(1);                         
                    		dup(fid);
                    		close(fid);
      			}
	    		else    // no redirection

				// Calls process_id() function to print out the Process ID(PID) of the shell
	      			process_id();  
		}

		else if (strcmp(arg[0],"kill") == 0){ // built-in kill command
			printf("Executing built-in [kill]\n");

			// If no args are provided, then print an error message
			if(arg[1] == NULL){ // empty "kill"
				printf("kill: Too few arguments.\n");
			}

			// If a single arg is provided, then that means ONLY PID is given
			// In such case, send a SIGTERM to the process with that PID by a call to kill(...)
			
			else if(arg[2] == NULL){ // ONLY PID is provided
				int pid = atoi(arg[1]);
				kill(pid,SIGTERM);
			}

                        // If more than one arg is provided, then that means BOTH signal number and PID are given
                        // In such case, send that particular signal to the process with the given PID by a call to kill(...)			
			
			else{ // BOTH PID and signal number are provided
				int signal = atoi(strtok(arg[1],"-"));
				int pid = atoi(arg[2]);	
				kill(pid,signal);
			}
		}

		else if (strcmp(arg[0],"cd") == 0){ // built-in command cd
			printf("Executing built-in [cd]\n");

			// We can use chdir(...) function to change from one working directory to another and thus to implement "cd"
                        // We can use OLDPWD env variable to keep track of previously visited directory
                        // Similarly, we can use PWD env variable to keep track of latest working directory
			
			
			// Check if any args are provided or not
			// If not provided, then CWD to HOME directory
			if(arg[1] == NULL){

				// Remember, that we have HOME environment variable which stores the directory for HOME
				// Thus, we can directly use getenv(...) to get the value for HOME environment variable
				// Updates OLDPWD env variable and PWD env variable
				if(strcmp(getenv("HOME")," ") != 0){
					
					setenv("OLDPWD",getenv("PWD"),1);

	                                // Sets an env OLDPWD with its name and value of PWD env in our global variable "dynamic_envvariables"
        	                        // Call to setenvvariable(...) will:                	              
                        	        //  -  Modify the existing env variable OLDPWD with the new value of PWD env within "dynamic_envvariables"	 
					setenvvariable("OLDPWD",getenv("PWD"));

					setenv("PWD",getenv("HOME"),1);

					// Sets an env PWD with its name and value of a HOME env in our global variable "dynamic_envvariables"
                                        // Call to setenvvariable(...) will:
                                        //  -  Modify the existing env variable PWD with the given new value of HOME env within "dynamic_envvariables"
					setenvvariable("PWD",getenv("HOME"));

					chdir(getenv("HOME"));
				}

				// If HOME env value is empty, print an error message
				else{
					printf("cd: Bad Directory.\n");
				}
			}

			// Check if second arg is provided or not
			// If not provided, then that means ONLY first arg is given
			// If first arg is "-", then go back to previously visited directory
			// If first arg is path of the directory, then go to that path directory
			else if(arg[2] == NULL){

				// Toggle between value of OLDPWD and value of PWD if "-" is provided as an arg
				if(strcmp(arg[1],"-") == 0){
					if(oldpwd_flag){
						oldpwd_flag = 0;
						chdir(getenv("OLDPWD"));
					}
					else{
						oldpwd_flag = 1;
						chdir(getenv("PWD"));
					}
				}
				else{
					// Print an error message if any files or executables are given instead of a directory
					if(chdir(arg[1]) == -1){
						printf("%s: Not a directory\n",arg[1]);
					}

					// Changes CWD to specified path given
					// Updates OLDPWD and PWD
					else{
						setenv("OLDPWD",getenv("PWD"),1);

	                                        // Sets an env OLDPWD with its name and value of PWD env in our global variable "dynamic_envvariables"
        	                                // Call to setenvvariable(...) will:
                	                        //  -  Modify the existing env variable OLDPWD with the new value of PWD env within "dynamic_envvariables"	
						 setenvvariable("OLDPWD",getenv("PWD"));

						chdir(arg[1]);

						char *tmp = getcwd(NULL,0);
						setenv("PWD",tmp,1);

						// Sets an env PWD with its name and value of a CWD in our global variable "dynamic_envvariables"
						// Call to setenvvariable(...) will:
       						//  -  Modify the existing env variable PWD with the given new value of CWD within "dynamic_envvariables"
						setenvvariable("PWD",tmp);
					
						free(tmp);
					}
				}
			}

			// This assumes that more than one arg is provided for "cd" command
			// Print an error message in such case
			else{
				printf("cd: Too many arguments.\n");
			}
		}

		else if (strcmp(arg[0], "printenv") == 0){ // built-in printenv command
			printf("Executing builtin [printenv]\n");

                        if (redirection) { // redirection for "printenv"
                                int fid;

                                if(noclobber){ // redirection with noclobber
                                        if (!append && rstdout && !rstderr) {  // ">"

                                                // Check if the file already exists or not
                                                // If it is, print the message refusing to overwrite
                                                if(access(arg[arg_no-1],F_OK) == 0){
                                                        printf("%s: File exists.\n",arg[arg_no-1]);
                                                        goto nextprompt;
                                                }

                                                // If it doesn't, then create a new file
                                                else{
                                                        fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                                }
                                        }
                                        else if (!append && rstdout && rstderr){  // ">&"
                                                // Check if the file already exists or not
                                                // If it is, print the message refusing to overwrite
                                                if(access(arg[arg_no-1],F_OK) == 0){
                                                        printf("%s: File exists.\n",arg[arg_no-1]);
                                                        goto nextprompt;

                                                }

                                                // If it doesn't, then create a new file
                                                else{
                                                        fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                                }
                                        }

                                        else if (append && rstdout && !rstderr) {   // ">>"
                                                fid = open(arg[arg_no-1], O_WRONLY | O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                                // Check if the file is already created to append the contents
                                                // If not, then print the message refusing to create a new file
                                                if(fid < 0){
                                                        printf("%s: No such file or directory.\n",arg[arg_no-1]);
                                                        goto nextprompt;
                                                }
                                        }
                                        else if (append && rstdout && rstderr) {   // ">>&"
                                                fid = open(arg[arg_no-1], O_WRONLY| O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                                // Check if the file is already created to append the contents
                                                // If not, then print the message refusing to create a new file
                                                if(fid < 0){
                                                        printf("%s: No such file or directory.\n",arg[arg_no-1]);
                                                        goto nextprompt;
                                                }
                                        }
                                }
				
             
				else{   // redirection without noclobber
                                        if (!append && rstdout && !rstderr)  // ">"
                                                fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

                                        else if (!append && rstdout && rstderr)  // ">&"
                                                fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

                                        else if (append && rstdout && !rstderr)   // ">>
                                                fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                        else if (append && rstdout && rstderr)   // ">>&"
                                                fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
                                }
				
				
				/* Redirect STDOUT to file */
				close(1); // Closes the file descriptor for STDOUT
				dup(fid); // STDOUT points to new file descriptor "fid"


	                        // Check if any arguments are provided or not
        	                // If not, then call printenv(...) function and print ALL environment variables with its value
				if(strcmp(arg[1],">")  == 0 || strcmp(arg[1],">&") == 0 || strcmp(arg[1],">>") == 0 || strcmp(arg[1],">>&") == 0){ 
					
					if(strcmp(arg[1],">&") == 0 || strcmp(arg[1],">>&") == 0){	
						
						/* Redirect STDERR to file */				
						close(2); // Closes the file descriptor for STDERR
						dup(fid); // STDERR points to new file descriptor "fid"
					}
					close(fid);

					printf("\n");
					printenv(dynamic_envvariables);
	 			}
	 			// Check if second arg is provided to "printenv" command
				// If not, then print associated value of environment variable name given in first arg 
				else if(strcmp(arg[2],">")  == 0 || strcmp(arg[2],">&") == 0 || strcmp(arg[2],">>") == 0 || strcmp(arg[2],">>&") == 0){

                                        if(strcmp(arg[2],">&") == 0 || strcmp(arg[2],">>&") == 0){

						/* Redirect STDERR to file */
                                                close(2); // Closes the file descriptor for STDERR
                                                dup(fid); // STDERR points to new file descriptor "fid"
                                        }
                                        close(fid);
				
					printf("%s\n",getenv(arg[1]));
                        	}
	 			// This assumes that two or more args are given for "printenv" command	                    
				else{
					if(strcmp(arg[3],">&") == 0 || strcmp(arg[3],">>&") == 0){ 

						/* Redirect STDERR to file */
						close(2); // Closes the file descriptor for STDERR
						dup(fid); // STDERR points to new file descriptor "fid"
					}
					close(fid);

					printf("printenv: Too many arguments.\n");
	 			}

				/* Redirect STDOUT back to terminal */
                                fid = open("/dev/tty", O_WRONLY); 
                                close(1);                        
                                dup(fid);
                                close(fid);

				// Prints the error message to the terminal without STDERR
				// Truncate the contents of file(in file redirection) to 0
				if(arg[3] != NULL && (strcmp(arg[3],">")  == 0 || strcmp(arg[3],">>") == 0)){
				
				       // Check for the existence of file before truncating file's contents	
				       if(access(arg[arg_no-1], F_OK) == 0){	
					       fid = open(arg[arg_no-1], O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);
					       close(fid);  
				       }				    
				       
				       printf("printenv: Too many arguments.\n");	
				}
                        }
                        else{	// no redirection

                                // Check if any arguments are provided or not
                                // If not, then call printenv(...) function and print ALL environment variables with its value
                                if(arg[1] == NULL){
                                        printf("\n");
                                        printenv(dynamic_envvariables);
                                }
                                // Check if second arg is provided to "printenv" command
                                // If not, then print associated value of environment variable name given in first arg
                                else if(arg[2] == NULL){
                                        printf("%s\n",getenv(arg[1]));
                                }
                                // This assumes that two or more than two args are given for "printenv" command
                                // In such case, print an error message to screen
                                else{
                                        printf("printenv: Too many arguments.\n");
                                }

			}				
		}

		else if (strcmp(arg[0], "setenv") == 0) { // built-in setenv command

			printf("Executing built-in [setenv]\n");

			// Check if any args are provided to "setenv" command or not
			// If none args are given, then call printenv(...) function to print ALL environment variables with its value
			if(arg[1] == NULL){
				printf("\n");
				printenv(dynamic_envvariables);
			}
			
			// Check if second arg is provided or not
			// If not provided, then that means ONLY name of environment variable is provided
			// Second arg is just the newline character
			// In such case, do the following steps:
			// 	1. Set an environment variable with its name and an empty value
			// 	2. Check if that name already exists from environment variable list or not
			// 	3. If it does, then modify its associated value
			// 	4. If not, then add that environment variable as a newly created variable
			else if(arg[2] == NULL){

                                // Special care must be given if PATH is changed
                                // We need to free up the space for old PATH before assigning new PATH value
                                if(strcmp(arg[1],"PATH") == 0){
                                        pathlist = get_path();
                                        free_path(pathlist);
                                 }
				
                                // Sets an environment variable with its name and an empty value
                                setenv(arg[1]," ",1);

				// Sets an environment variable with its name and an empty value in our global variable "dynamic_envvariables"
				// Call to setenvvariable(...) will EITHER:
				// 	1.  Add new env variable at the end of "dynamic_ennvariables" list OR
				// 	2.  Modify the existing env variable with the given new value
				setenvvariable(arg[1]," ");
			}

                        // Check if third arg is provided or not
                        // If not provided, then that means BOTH name of environment variable and value of env variable are provided
                        // In such case, do the following steps:
                        //      1. Set an environment variable with its name and value from arg[2]
                        //      2. Check if that name already exists from environment variable list or not
                        //      3. If it does, then modify its associated value
                        //      4. If not, then add that environment variable as a newly created variable to the end of list
			else if(arg[3] == NULL){

				// Special care must be given if PATH is changed
				// We need to free up the space for old PATH before assigning new PATH value
                                if(strcmp(arg[1],"PATH") == 0){
					pathlist = get_path();							 
					free_path(pathlist);
                                 }
				
				// Sets an environment variable with its name and the value provided by arg[2]
				setenv(arg[1],arg[2],1);

                                // Sets an environment variable with its name and an empty value in our global variable "dynamic_envvariables"
                                // Call to setenvvariable(...) will EITHER:
                                //      1.  Add new env variable at the end of "dynamic_ennvariables" list OR
                                //      2.  Modify the existing env variable with the given new value
				setenvvariable(arg[1],arg[2]);
				
			}
			// Print an error message if third argument is provided
			else{
				printf("setenv: Too many arguments.\n");
			}
		}
		else if (strcmp(arg[0], "list") == 0){ // built-in list command
			printf("Executing built-in [list]\n");

                        if (redirection) { // redirection for "list" command
                                int fid;

                                if(noclobber){ // redirection with noclobber
                                        if (!append && rstdout && !rstderr) {  // ">"

                                                // Check if the file already exists or not
                                                // If it is, print the message refusing to overwrite
                                                if(access(arg[arg_no-1],F_OK) == 0){
                                                        printf("%s: File exists.\n",arg[arg_no-1]);
                                                        goto nextprompt;
                                                }

                                                // If it doesn't, then create a new file
                                                else{
                                                        fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                                }
                                        }
                                        else if (!append && rstdout && rstderr){  // ">&"
                                                // Check if the file already exists or not
                                                // If it is, print the message refusing to overwrite
                                                if(access(arg[arg_no-1],F_OK) == 0){
                                                        printf("%s: File exists.\n",arg[arg_no-1]);
                                                        goto nextprompt;

                                                }

                                                // If it doesn't, then create a new file
                                                else{
                                                        fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                                }
                                        }

                                        else if (append && rstdout && !rstderr) {   // ">>"
                                                fid = open(arg[arg_no-1], O_WRONLY | O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                                // Check if the file is already created to append the contents
                                                // If not, then print the message refusing to create a new file
                                                if(fid < 0){
                                                        printf("%s: No such file or directory.\n",arg[arg_no-1]);
                                                        goto nextprompt;
                                                }
                                        }
                                        else if (append && rstdout && rstderr) {   // ">>&"
                                                fid = open(arg[arg_no-1], O_WRONLY| O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                                // Check if the file is already created to append the contents
                                                // If not, then print the message refusing to create a new file
                                                if(fid < 0){
                                                        printf("%s: No such file or directory.\n",arg[arg_no-1]);
                                                        goto nextprompt;
                                                }
                                        }
                                }								
				
                                else{   // redirection without noclobber
                                        if (!append && rstdout && !rstderr)  // ">"
                                                fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

                                        else if (!append && rstdout && rstderr)  // ">&"
                                                fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

                                        else if (append && rstdout && !rstderr)   // ">>
                                                fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                        else if (append && rstdout && rstderr)   // ">>&"
                                                fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
                                }
                                
				
				/* Redirect STDOUT to file */
                                close(1); // Closes the file descriptor for STDOUT
                                dup(fid); // STDOUT points to new file descriptor "fid"                  

        	                // Check if any args are provided to list
                	        // If no args are provided, then just list the files in the current working directory one per line
             			if(strcmp(arg[1],">")  == 0 || strcmp(arg[1],">&") == 0 || strcmp(arg[1],">>") == 0 || strcmp(arg[1],">>&") == 0){

                                        if(strcmp(arg[1],">&") == 0 || strcmp(arg[1],">>&") == 0){

                                                /* Redirect STDERR to file */
                                                close(2); // Closes the file descriptor for STDERR
                                                dup(fid); // STDERR points to new file descriptor "fid"
                                        }

					close(fid);

					ptr = getcwd(NULL,0);
	                                list(ptr);
        	                        free(ptr);
                	        }
				// Check how many args are provided to list
        	                // For each arg, list the files in each directory with a "blank line" then "the name of the directory"
                	        // and then followed by a ":" before the list of files in that directory
                        	else{
                                        if(strcmp(arg[arg_no-2],">&") == 0 || strcmp(arg[arg_no-2],">>&") == 0){

                                                /* Redirect STDERR to file */
                                                close(2); // Closes the file descriptor for STDERR
                                                dup(fid); // STDERR points to new file descriptor "fid"
                                        }
					close(fid);
					
					int dirnumber = 1;
					while(strcmp(arg[dirnumber],">")    != 0 &&  strcmp(arg[dirnumber],">>")   != 0 &&
					      strcmp(arg[dirnumber],">&")   != 0 &&  strcmp(arg[dirnumber],">>&")  != 0){

						printf("%s:\n",arg[dirnumber]);
                        	                list(arg[dirnumber]);
                                	        printf("\n");
                                        	dirnumber++;
					}
				}
                                
				/* Redirect STDOUT back to terminal */
				fid = open("/dev/tty", O_WRONLY); 
                                close(1);                        
                                dup(fid);
                                close(fid);
                        }
                        else{   // no redirection                             

                                // Check if any args are provided to list
                                // If no args are provided, then just list the files in the current working directory one per line
                                if(arg[1] == NULL){
                                        ptr = getcwd(NULL,0);
                                        list(ptr);
                                        free(ptr);
                                }
                                // Check how many args are provided to list
                                // For each arg, list the files in each directory with a "blank line" then "the name of the directory"
                                // and then followed by a ":" before the list of files in that directory
                                else{
                                        int dirnumber = 1;
                                        while(arg[dirnumber] != NULL){
                                                printf("%s:\n",arg[dirnumber]);
                                                list(arg[dirnumber]);
                                                printf("\n");
                                                dirnumber++;
                                        }
                                }

                        }
		}

		else if (strcmp(arg[0], "which") == 0) { // built-in command which
		  struct pathelement *p;
                  char *cmd;
                    
		  printf("Executing built-in [which]\n");

		  if (redirection) { // redirection for "which" command
			  int fid;

			  if(noclobber){ // redirection with noclobber
				  if (!append && rstdout && !rstderr) {  // ">"
					  
					  // Check if the file already exists or not
					  // If it is, print the message refusing to overwrite
					  if(access(arg[arg_no-1],F_OK) == 0){
    						  printf("%s: File exists.\n",arg[arg_no-1]);
                                                  goto nextprompt;
                                          }

                                          // If it doesn't, then create a new file
                                          else{
                                                  fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                          }
                                  }
                                  else if (!append && rstdout && rstderr){  // ">&"
                                                
					  // Check if the file already exists or not
                                          // If it is, print the message refusing to overwrite
                                          if(access(arg[arg_no-1],F_OK) == 0){
                                                  printf("%s: File exists.\n",arg[arg_no-1]);
                                                  goto nextprompt;
                                          }

                                          // If it doesn't, then create a new file
                                          else{
                                                  fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                          }
                                  }

                                  else if (append && rstdout && !rstderr) {   // ">>"
                                          
					  fid = open(arg[arg_no-1], O_WRONLY | O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
                                                
					  // Check if the file is already created to append the contents
                                          // If not, then print the message refusing to create a new file
                                          if(fid < 0){
                                                  
						  printf("%s: No such file or directory.\n",arg[arg_no-1]);
                                                  goto nextprompt;
                                          }
                                  }
                                  
				  else if (append && rstdout && rstderr) {   // ">>&"
                                                
					  fid = open(arg[arg_no-1], O_WRONLY| O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
                                          
				    	  // Check if the file is already created to append the contents
					  // If not, then print the message refusing to create a new file
                                          if(fid < 0){
                                                  
						  printf("%s: No such file or directory.\n",arg[arg_no-1]);                                                 
					   	  goto nextprompt;
                                          }
                                 }
                          }
			  
			  
			  else{   // redirection without noclobber
				  if (!append && rstdout && !rstderr)  // ">"
					  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

				  else if (!append && rstdout && rstderr)  // ">&"
					  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

				  else if (append && rstdout && !rstderr)   // ">>
					  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

				  else if (append && rstdout && rstderr)   // ">>&"
					  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
			  }
 			  
			 
			  /* Redirect STDOUT to file */
			  close(1); // Closes the file descriptor for STDOUT
			  dup(fid); // STDOUT points to file descriptor "fid"

			  if(strcmp(arg[1],">")  == 0 || strcmp(arg[1],">&") == 0 || strcmp(arg[1],">>") == 0 || strcmp(arg[1],">>&") == 0){  // "empty" which
				  if(strcmp(arg[1],">&") == 0 || strcmp(arg[1],">>&") == 0){

                                          /* Redirect STDERR to file */
                                          close(2); // Close the file descriptor for STDERR
                                          dup(fid); // STDERR points to file descriptor "fid"
                                  }
                                  close(fid);

                                  printf("which: Too few arguments.\n");
                  
			  }	  
                          // This will assume that there are 1 or more args provided to "which" command
                          // In such case, "which" command will locate first instance of ALL args if it would be possible
                          else{
				  if(strcmp(arg[arg_no-2],">&") == 0 || strcmp(arg[arg_no-2],">>&") == 0){

					  /* Redirect STDERR to file */
					  close(2); // Closes the file descriptor for STDERR
					  dup(fid); // STDERR points to new file descriptor "fid"
				  }
				  close(fid);
				  
                                  p = get_path();
                                  int curr_arg_no = 1;
                                  while(strcmp(arg[curr_arg_no],">")  != 0 && strcmp(arg[curr_arg_no],">>")  != 0 &&
					strcmp(arg[curr_arg_no],">&") != 0 && strcmp(arg[curr_arg_no],">>&") != 0) {

                                          cmd = which(arg[curr_arg_no], p);
                                          if (cmd) {
                                                  printf("%s\n", cmd);
                                                  free(cmd);
                                          }
                                          else {					
                                                  printf("%s: Command not found\n", arg[curr_arg_no]);                                          
					  }
					  curr_arg_no++;
                                  }				 

                                  // The implementation of function free_path(...) is in this file on the top
                                  free_path(p);
                          }

			  
			  /* Redirect STDOUT back to terminal */
  			  fid = open("/dev/tty", O_WRONLY);
			  close(1);                         
			  dup(fid);
			  close(fid);	
						  
			  // Print the error message to the terminal without STDERR
			  // Truncate the contents of file(in file redirection) to 0
			  if(strcmp(arg[1],">") == 0 || strcmp(arg[1],">>") == 0){

				  // Check for the existence of file before truncating file's contents
				  if(access(arg[arg_no-1], F_OK) == 0){
					  fid = open(arg[arg_no-1], O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);
					  close(fid);
				  }				                                
				  
				  printf("which: Too few arguments.\n");
			  }

		  }
		  else{  // no redirection

			  if (arg[1] == NULL) {  // "empty" which
	      			  printf("which: Too few arguments.\n");
              		          goto nextprompt;
			  }
			  // This will assume that there are 1 or more args provided to "which" command
			  // In such case, "which" command will locate first instance of ALL args if it would be possible
			  else{
				  p = get_path();
        	                  int curr_arg_no = 1;
                	          while(arg[curr_arg_no]){
                        	          cmd = which(arg[curr_arg_no], p);
                                	  if (cmd) {
						  printf("%s\n", cmd);
	                                          free(cmd);
        	                          }
                	                  else               // argument not found
                        	                  printf("%s: Command not found\n", arg[curr_arg_no]);
					  curr_arg_no++;
  				  }
				  // The implementation of function free_path(...) is in this file on the top
				  free_path(p);
			  }
		  }
		} 

		else if (strcmp(arg[0], "where") == 0) { // built-in command where
		  printf("Executing built-in [where]\n");

                  if (redirection) { // redirection for "where" command
                          int fid;

			  if(noclobber){ // redirection with noclobber

				  if (!append && rstdout && !rstderr) {  // ">"

					  // Check if the file already exists or not
					  // If it is, print the message refusing to overwrite

					  if(access(arg[arg_no-1],F_OK) == 0){
						  printf("%s: File exists.\n",arg[arg_no-1]);
						  goto nextprompt;
					  }


                                          // If it doesn't, then create a new file
                                          else{
                                                  fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                          }                                
				  }
                                        
				  else if (!append && rstdout && rstderr){  // ">&"
                          
		    			  // Check if the file already exists or not
					  // If it is, print the message refusing to overwrite

					  if(access(arg[arg_no-1],F_OK) == 0){
						  printf("%s: File exists.\n",arg[arg_no-1]);
                                                  goto nextprompt;
					  }
                                          
					  // If it doesn't, then create a new file
                                          else{                                          
				    		  fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                          }
                                
				  }
                                  
			    	  else if (append && rstdout && !rstderr) {   // ">>"
                                                
					  fid = open(arg[arg_no-1], O_WRONLY | O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
                                          
				    	  // Check if the file is already created to append the contents                                    
			      		  // If not, then print the message refusing to create a new file
					  if(fid < 0){                                                        
						  printf("%s: No such file or directory.\n",arg[arg_no-1]);
                                                  goto nextprompt;
                                          }
                                        
				  }
                                        
				  else if (append && rstdout && rstderr) {   // ">>&"
                                  
			    		  fid = open(arg[arg_no-1], O_WRONLY| O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
                            
		      			  // Check if the file is already created to append the contents                      
					  // If not, then print the message refusing to create a new file
                                                
					  if(fid < 0){      
      						  printf("%s: No such file or directory.\n",arg[arg_no-1]);
                                                  goto nextprompt;
                                          }
                                 
				  }
                          }
 			  
                          else{   // redirection without noclobber
                                  if (!append && rstdout && !rstderr)  // ">"
                                          fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

                                  else if (!append && rstdout && rstderr)  // ">&"
                                          fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

                                  else if (append && rstdout && !rstderr)   // ">>
                                          fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                  else if (append && rstdout && rstderr)   // ">>&"
                                          fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
                          }
                          
			  			 
			  /* Redirect STDOUT to file */
                          close(1); // Closes the file descriptor for STDOUT
                          dup(fid); // STDOUT points to file descriptor "fid"                       
                         			 
                          struct pathelement *p, *tmp;
	                  char **cmd;
	
			  if(strcmp(arg[1],">")  == 0 || strcmp(arg[1],">&") == 0 || strcmp(arg[1],">>") == 0 || strcmp(arg[1],">>&") == 0){  // "empty" where

				  if(strcmp(arg[1],">&") == 0 || strcmp(arg[1],">>&") == 0){

					  /* Redirect STDERR to file */
					  close(2); // Close the file descriptor for STDERR
					  dup(fid); // STDERR points to file descriptor "fid"
				  }
				  close(fid);

				  printf("where: Too few arguments.\n");			

			  }
			  // This will assume that there are 1 or more args provided to "where" command
        	          // In such case, "where" command will locate ALL instance of ALL args if it would be possible
                	  else{

				  if(strcmp(arg[arg_no-2],">&") == 0 || strcmp(arg[arg_no-2],">>&") == 0){

                                          /* Redirect STDERR to file */
                                          close(2); // Closes the file descriptor for STDERR
                                          dup(fid); // STDERR points to new file descriptor "fid"

				  }
				  close(fid);
				  
                        	  p = get_path();
                          	  int curr_arg_no = 1;
                          	  while(strcmp(arg[curr_arg_no],">")  != 0 && strcmp(arg[curr_arg_no],">>")  != 0 &&
				        strcmp(arg[curr_arg_no],">&") != 0 && strcmp(arg[curr_arg_no],">>&") != 0){

	                                  cmd = where(arg[curr_arg_no],p);
        	                          if(cmd) {
						  for(int i = 0; cmd[i] != NULL; i++){
                        	                          printf("%s\n",cmd[i]);
                                	                  free(cmd[i]);
                                        	  }
                                  	  }
                                  	  else              // argument not found
						  printf("%s: Command not found\n", arg[curr_arg_no]);
					 
					  curr_arg_no++;
					 
					  // Free the space used for storing the output to where(...) function call
        	                          free(cmd);
                	          }

                          	  // The implementation of function free_path(...) is in this file on the top
                          	  free_path(p);
                  	  }
        
			  /* Redirect STDOUT to terminal */
			  fid = open("/dev/tty", O_WRONLY); 
                          close(1);                        
                          dup(fid);
                          close(fid);

                          // Print the error message to the terminal without STDERR
			  // Truncate the contents of file(in file redirection) to 0
                          if(strcmp(arg[1],">") == 0 || strcmp(arg[1],">>") == 0){

				  // Check for the existence of file before truncating file's contents
				  if(access(arg[arg_no-1], F_OK) == 0){
					  fid = open(arg[arg_no-1], O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);
					  close(fid);
				  }			
				  			         	
                                  printf("where: Too few arguments.\n");
			  }
			 
		  }
		  else{  // no redirection

	                  struct pathelement *p, *tmp;
        	          char **cmd;
	
        	          if (arg[1] == NULL) {  // "empty" where
                	          printf("where: Too few arguments.\n");
                        	  goto nextprompt;
                  	  }
                  	  
			  // This will assume that there are 1 or more args provided to "where" command
                  	  // In such case, "where" command will locate ALL instance of ALL args if it would be possible
                	  else{
                        	  p = get_path();
                          	  int curr_arg_no = 1;
                          	  while(arg[curr_arg_no]){
                                  	cmd = where(arg[curr_arg_no],p);
                                  	if(cmd) {
						for(int i = 0; cmd[i] != NULL; i++){
      							printf("%s\n",cmd[i]);
      							free(cmd[i]);
      						}
      					}
      					else              // argument not found
      						printf("%s: Command not found\n", arg[curr_arg_no]);
      					curr_arg_no++;
      
					// Free the space used for storing the output to where(...) function call
                                        free(cmd);
       				  }
				  // The implementation of function free_path(...) is in this file on the top
				  free_path(p);
			  }					
		  }			  
		
		}

		else {  // external command
		  if ((pid = fork()) < 0) {
			printf("fork error");
		  } 
		  else if (pid == 0) {		/* child */
			               
		 	// an array of aguments for execve()
	                char    *execargs[MAXARGS]; 
		        glob_t  paths;
                        int     csource, j;
			char    **p;

			execargs[0] = malloc(strlen(arg[0])+1);
			strcpy(execargs[0], arg[0]);  // copy command

		        j = 1;
		        for (i = 1; i < arg_no; i++) // check arguments
			  if (strchr(arg[i], '*') != NULL) { // wildcard(*) is encountered as an arg

			    // Call to glob(...) function searches for all the pathnames matching the pattern given as "arg[i]"
			    // Matching pathnames are stored in structure of type "glob_t"
			    // On success, glob(...) function returns 0
			    csource = glob(arg[i], 0, NULL, &paths);
			    
                            if (csource == 0) {
                              for (p = paths.gl_pathv; *p != NULL; ++p) {
                                execargs[j] = malloc(strlen(*p)+1);
				strcpy(execargs[j], *p);
				j++;
                              }
                           
			      // Frees all the heap space used by previous glob(...) function
                              globfree(&paths);
                            }
                          }

			// Marks the end of pointer to char pointers array "execargs" by making the last element of "execargs" to NULL
                        execargs[j] = NULL;

			// Check for the existence of file AND execute permission 
			if(access(execargs[0],F_OK) == 0 && access(execargs[0],X_OK) == 0){
			
                                if(redirection) { // redirection for external commands
				  
                                  printf("Executing [%s]\n",execargs[0]);

                                  int fid;
				  if(noclobber){ // redirection with noclobber
					  if (!append && rstdout && !rstderr) {  // ">"
                                          
						  // Check if the file already exists or not
                                          	  // If it is, print the message refusing to overwrite

                                          	  if(access(arg[arg_no-1],F_OK) == 0){
                                                  							  
							  printf("%s: File exists.\n",arg[arg_no-1]);
                                                  	  goto nextprompt;                                          	  
						  }


                                          	  // If it doesn't, then create a new file
                                          	  else{
                                                  	  fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                          	  }                                  	  
					  }
                                  
					  else if (!append && rstdout && rstderr){  // ">&"

                                          
						  // Check if the file already exists or not
                                          	  // If it is, print the message refusing to overwrite

                                          	  if(access(arg[arg_no-1],F_OK) == 0){
                                                  	  printf("%s: File exists.\n",arg[arg_no-1]);
                                                  	  goto nextprompt;
                                          	  }

                                          	  // If it doesn't, then create a new file
                                          	  else{
                                                  	  fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                          	  }

                                  	  }

                                  	  else if (append && rstdout && !rstderr) {   // ">>"

                                          	  fid = open(arg[arg_no-1], O_WRONLY | O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                          	  // Check if the file is already created to append the contents
                                          	  // If not, then print the message refusing to create a new file
                                          	  if(fid < 0){
                                                  	  printf("%s: No such file or directory.\n",arg[arg_no-1]);
                                                  	  goto nextprompt;
                                          	  }

                                  	  }

                                  	  else if (append && rstdout && rstderr) {   // ">>&"

                                          	  fid = open(arg[arg_no-1], O_WRONLY| O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                          	  // Check if the file is already created to append the contents
                                          	  // If not, then print the message refusing to create a new file

                                          	  if(fid < 0){
                                                  	  printf("%s: No such file or directory.\n",arg[arg_no-1]);
                                                  	  goto nextprompt;
                                          	  }

                                  	  }

					  else // "<"
						  fid = open(arg[arg_no-1], O_RDONLY, S_IRUSR | S_IRGRP);
                          	  }
				  
				  else{   // redirection without noclobber
        
					  if (!append && rstdout && !rstderr)  // ">"                
						  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

					  else if (!append && rstdout && rstderr)  // ">&"
						  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);
	 
					  else if (append && rstdout && !rstderr)   // ">>        
						  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                  	  else if (append && rstdout && rstderr)   // ">>&"
                                          	  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);
					  
					  else  // "<"
						  fid = open(arg[arg_no-1], O_RDONLY, S_IRUSR | S_IRGRP);
                          	 
				  }
                                  
				  // STDIN File Redirection
				  if(strcmp(arg[arg_no - 2],"<") == 0){

                                          /* Redirect STDIN to file */
                                          close(0); // Close the file descriptor for STDIN
                                          dup(fid); // STDIN points to file descriptor "fid"
					  close(fid);


                                          // Execute the external command
					  // DON'T INCLUDE "<" AS A FILENAME FOR INPUT
					  arg[arg_no-2] = arg[arg_no-1];
					  arg[arg_no-1] = NULL;
                                          execve(execargs[0],arg,NULL);                                                            
					  
				  }

				  // STDOUT, OR (STDOUT AND STDERR) File Redirection
				  else{					 
					  /* Redirect STDOUT to file */
					  close(1); // Close the file descriptor for STDOUT
					  dup(fid); // STDOUT points to file descriptor "fid"

                                  
					  if(strcmp(arg[arg_no - 2],">&") == 0 || strcmp(arg[arg_no - 2],">>&") == 0){
                                          
						  /* Redirect STDERR to file */
                                          	  close(2); // Closes the file descriptor for STDERR
                                          	  dup(fid); // STDERR points to new file descriptor "fid"
                                  	  }
                                  	  close(fid);

                                  	  // Execute the external command
                                  	  execve(execargs[0],arg,NULL);

                                  	  /* Redirect STDOUT to terminal */
                                  	  fid = open("/dev/tty", O_WRONLY);
                                  	  close(1);
                                  	  dup(fid);
                                  	  close(fid);
					                                          
				  }                                            
				  
				}
				else { // no redirection
					
					// Check if external command is called with bg
					// If it is, then use the arguments that we set up for bg earlier
					if(background){
                                                printf("Background process number [%d] with pid [%d]\n",bg_number,getpid());
						execve(execargs[0],bg_arg,NULL);
					}

					// If not, then just use the original arguments tokenized
					else{
						printf("Executing [%s]\n",execargs[0]);
						execve(execargs[0],arg,NULL);

					}
				}
			}

			// This assumes that the command is either not found OR not an executable
			// In such case, call the which(...) function to get the first instance of command from PATH
			else{

				if(redirection) { // redirection for external commands
				  
				  printf("Executing [%s]\n",execargs[0]);

	                          int fid;
                                  if(noclobber){ // redirection with noclobber
                                          if (!append && rstdout && !rstderr) {  // ">"

                                                  // Check if the file already exists or not
                                                  // If it is, print the message refusing to overwrite

                                                  if(access(arg[arg_no-1],F_OK) == 0){

                                                          printf("%s: File exists.\n",arg[arg_no-1]);
                                                          goto nextprompt;
                                                  }


                                                  // If it doesn't, then create a new file
                                                  else{
                                                          fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                                  }
                                          }

                                          else if (!append && rstdout && rstderr){  // ">&"


                                                  // Check if the file already exists or not
                                                  // If it is, print the message refusing to overwrite

                                                  if(access(arg[arg_no-1],F_OK) == 0){
                                                          printf("%s: File exists.\n",arg[arg_no-1]);
                                                          goto nextprompt;
                                                  }

                                                  // If it doesn't, then create a new file
                                                  else{
                                                          fid = open(arg[arg_no-1], O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
                                                  }

                                          }

                                          else if (append && rstdout && !rstderr) {   // ">>"

                                                  fid = open(arg[arg_no-1], O_WRONLY | O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                                  // Check if the file is already created to append the contents
                                                  // If not, then print the message refusing to create a new file
                                                  if(fid < 0){
                                                          printf("%s: No such file or directory.\n",arg[arg_no-1]);
                                                          goto nextprompt;
                                                  }

                                          }

                                          else if (append && rstdout && rstderr) {   // ">>&"

                                                  fid = open(arg[arg_no-1], O_WRONLY| O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                                  // Check if the file is already created to append the contents
                                                  // If not, then print the message refusing to create a new file

                                                  if(fid < 0){
                                                          printf("%s: No such file or directory.\n",arg[arg_no-1]);
                                                          goto nextprompt;
                                                  }

                                          }

					  else  // "<"
                                                  fid = open(arg[arg_no-1], O_RDONLY, S_IRUSR | S_IRGRP);
					  
                                  }
				  
				  
                                  else{   // redirection without noclobber

                                          if (!append && rstdout && !rstderr)  // ">"
                                                  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

                                          else if (!append && rstdout && rstderr)  // ">&"
                                                  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);

                                          else if (append && rstdout && !rstderr)   // ">>
                                                  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                          else if (append && rstdout && rstderr)   // ">>&"
                                                  fid = open(arg[arg_no-1], O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP);

                                          else  // "<"
                                                  fid = open(arg[arg_no-1], O_RDONLY, S_IRUSR | S_IRGRP);					  
					  
                                  }				  

                                  // STDIN File Redirection
                                  if(strcmp(arg[arg_no - 2],"<") == 0){

                                          /* Redirect STDIN to file */
                                          close(0); // Close the file descriptor for STDIN
                                          dup(fid); // STDIN points to file descriptor "fid"
                                          close(fid);

                                          struct pathelement *path;
                                          char *excmd;
                                          path = get_path();

                                          excmd = which(execargs[0], path);

                                          // Check if you got any path for the given external command using the which(...) function call
                                          // If there is, then execute that using execve(...) function
                                          // Remember, which(...) function will ONLY give you FIRST instance of command from PATH env variable
                                          if(excmd) {
                                                  // Execute the external command found from which(...) function
						  // DON'T INCLUDE "<" AS A FILENAME FOR INPUT
						  arg[arg_no-2] = arg[arg_no-1];
						  arg[arg_no-1] = NULL;
                                                  execve(excmd,arg,NULL);
                                          }

                                          else   // external command not found
                                                  printf("%s: Command not found\n", execargs[0]);

                                          free(excmd);

                                          // The implementation of function free_path(...) is in this file on the top
                                          free_path(path);					  

                                  }

                                  // STDOUT, OR (STDOUT AND STDERR) File Redirection
                                  else{
                                          /* Redirect STDOUT to file */
                                          close(1); // Close the file descriptor for STDOUT
                                          dup(fid); // STDOUT points to file descriptor "fid"


                                          if(strcmp(arg[arg_no - 2],">&") == 0 || strcmp(arg[arg_no - 2],">>&") == 0){

                                                  /* Redirect STDERR to file */
                                                  close(2); // Closes the file descriptor for STDERR
                                                  dup(fid); // STDERR points to new file descriptor "fid"
                                          }
                                          close(fid);

	                                  struct pathelement *path;
        	                          char *excmd;
                	                  path = get_path();
		
                	                  excmd = which(execargs[0], path);

                        	          // Check if you got any path for the given external command using the which(...) function call
                                	  // If there is, then execute that using execve(...) function
	                                  // Remember, which(...) function will ONLY give you FIRST instance of command from PATH env variable
        	                          if(excmd) {
                	                          // Execute the external command found from which(...) function
                        	                  execve(excmd,arg,NULL);
                                	  }

	                                  else   // external command not found
        	                                  printf("%s: Command not found\n", execargs[0]);
	
        	                         

                	                  // The implementation of function free_path(...) is in this file on the top
                        	          free_path(path);

	                                  /* Redirect STDOUT to terminal */
        	                          fid = open("/dev/tty", O_WRONLY);
                	                  close(1);
                        	          dup(fid);
                                	  close(fid);

					  // Print external command not found error on screen if ">" OR ">>" is provided
					  // Truncate the contents of file(in file redirection) to 0
					  if((strcmp(arg[arg_no-2],">") == 0 || strcmp(arg[arg_no-2],">>") == 0) && !excmd){
	   					  
						  // Check for the existence of file before truncating file's contents					  
						  if(access(arg[arg_no-1], F_OK) == 0){
							  fid = open(arg[arg_no-1], O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);
        	                                          close(fid);
     						  }					  	  					               						                                         											  
						  printf("%s: Command not found\n",execargs[0]);
					  }
                                 
   				  }
				  					
				}

				else{ // no redirection

					struct pathelement *path;
	                                char *excmd;

        	                        path = get_path();
                	                excmd = which(execargs[0], path);

                        	        // Check if you got any path for the given external command using the which(...) function call
                                	// If there is, then execute that using execve(...) function
	                                // Remember, which(...) function will ONLY give you FIRST instance of command from PATH env variable
        	                        if(excmd) {

						// Check if external command is called with bg
						// If it is, then use the arguments that we set up for bg earlier
						if(background){
							printf("Background process number [%d] with pid [%d]\n",bg_number,getpid());
							execve(excmd,bg_arg,NULL);
						}

						// If not, then just use the original arguments tokenized
						else{
							printf("Executing [%s]\n",execargs[0]);

							// Execute the external command found from which(...) function
							execve(excmd,arg,NULL);
						}

                                	}

	                                else   // external command not found
                	                        printf("%s: Command not found\n", execargs[0]);
				
                                	free(excmd);
	
        	                        // The implementation of function free_path(...) is in this file on the top
                	                free_path(path);
	
				}
						
			}


			// Frees up the heap space for each element of "execargs"
			for(i = 0; i < j; i++)
				free(execargs[i]);
		  }	  
					
		  // parent
		  
		  if(!background){ // wait if not bg
			  
			  // Wait for child if not bg process
			  if ((pid = waitpid(pid, &status, 0)) < 0){
	  			  printf("non-bg waitpid error\n");
			  }			  
		  }

		  else{ // Calls SIGCHLD handler function via signal(...) to reap out MULTIPLE zombie processes that came from "bg" command
			  signal(SIGCHLD, sigchld_handler);
		  }
		}

           nextprompt:
		cwd_prompt_prefix = getcwd(NULL,0);
		if(!prompt_command_flag){
	        	fprintf(stdout, " [%s]> ",cwd_prompt_prefix); /* print prompt */
		}
		else{
                        fprintf(stdout, "%s [%s]> ",prompt_command_prefix,cwd_prompt_prefix);
		}
		free(cwd_prompt_prefix);
                fflush(stdout);

		// Checks for END-OF-FILE CHARACTER(CTRL-D)
		// If END-OF-FILE CHARACTER provided, then shell would repeatedly remind to use "exit" to leave
		while(fgets(buf, MAXLINE, stdin) == NULL){
			printf("\n");
			printf("Use \"exit\" to leave shell.\n");
			cwd_prompt_prefix = getcwd(NULL,0);
			fprintf(stdout, " [%s]> ",cwd_prompt_prefix); /* print prompt */
			free(cwd_prompt_prefix);
			fflush(stdout);
		}
		buflen = (int) strlen(buf);
		buf[buflen - 1] = '\0';
	}

	// Upon exiting, call to this function will free up all the dynamically allocated space for environment variables
	// It will free up space of global variable "dynamic_envvariables"
	free_dynamic_envvariables();

	/* Check and make sure that watchuser command has been runned ATLEAST once to call pthread_cancel(...) and pthread_join(...) */
	/* Prevents Segmentation Fault error */
	if(count_watchuser_runs > 0) {

		/* Prevents memory leak from watchuser thread */
		pthread_cancel(*thread_handles);     /* Sends a cancellation request to the watchuser thread "thread_handles" */
		pthread_join(*thread_handles, NULL); /* Joining with a thread is the only way to know that cancellation has completed and thus avoiding memory leak */
	}

	/* Destroys MUTEX object */
	pthread_mutex_destroy(&m);          

	exit(0);

} // End of Shell Implementation
