###############################
# Makefile
# IOS-projekt 2
# Autor: Jan Koci
# Datum: 17.4.2017
# # # # # # # # # # # # # # # #

CC 		= gcc
CFLAGS 	= -std=gnu99 -Wall -Wextra -Werror -pedantic
LFLAGS 	= -lpthread

all: proj2

.PHONY: clean

proj2: proj2.c proj2.h
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS) 

proj2.zip:
	zip proj2.zip proj2.c proj2.h Makefile

pack: proj2.zip

clean:
	rm proj2
