all: pvm
pvm: pvm.c
	gcc -Wall -o pvm pvm.c