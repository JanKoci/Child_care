###############################
# Makefile
# IOS-projekt 2
# Autor: Jan Koci
# Datum: 17.4.2017
# # # # # # # # # # # # # # # #

CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic
LFLAGS=-lpthread

all: proj2

proj2: proj2.c
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) 

clean:
	rm proj2
