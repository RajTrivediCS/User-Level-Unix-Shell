CC=gcc
# CC=gcc -Wall

mysh: get_path.o which.o where.o printenv.o list.o pid.o setenvvariables.o shell-with-builtin.o
	$(CC) -g shell-with-builtin.c get_path.o which.o where.o printenv.o list.o pid.o setenvvariables.o -o mysh -pthread

shell-with-builtin.o: shell-with-builtin.c sh.h
	$(CC) -g -c shell-with-builtin.c 

get_path.o: get_path.c get_path.h
	$(CC) -g -c get_path.c

which.o: which.c get_path.h
	$(CC) -g -c which.c

where.o: where.c get_path.h
	$(CC) -g -c where.c

printenv.o: printenv.c
	$(CC) -g -c printenv.c

list.o: list.c
	$(CC) -g -c list.c

pid.o: pid.c
	$(CC) -g -c pid.c

setenvvariables.o: setenvvariables.c
	$(CC) -g -c setenvvariables.c
clean:
	rm -rf shell-with-builtin.o get_path.o which.o where.o printenv.o list.o pid.o setenvvariables.o mysh
