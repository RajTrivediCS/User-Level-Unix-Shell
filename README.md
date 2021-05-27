# User-Level Unix Shell Description
   - This project involved learning how a Unix shell works, writing a simple shell, creating processes, handling signals, using Unix system calls and C library function      calls, and becoming a better programmer in general
   - The user-level shell implements built-in commands like "cd", "pwd", "which", "where", "kill", "watchuser" etc., external commands like "ls", "grep", "vim", "rm"          etc. using exec() system call of Unix, Inter-Process Communication(IPC) mechanism using Piping functionality, and File Redirection mechanism
   - This project utilizes high level C language for implementation

# Compilation and Running Instructions

   - This project uses Makefile to compile and run the Unix Shell program. Basically, by typing "make clean" and "make" commands sequentially you can able to compile          this program
   - After typing "make" command, an executable called "mysh" will be created in the same directory on the terminal. After having "mysh" executable, you can basically        run "./mysh" to run this program. Also, make sure when you type every command using this instructions, there shouldn't be any double quotes
   - You DO NOT need to worry about any command-line arguments either when running "./mysh" executable 
