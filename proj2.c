#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <time.h>
#include <signal.h>

//int semctl(int semid, int semnum, int cmd, ...)
#define SEM_NAME "/woodies_lights"

void print_help()
{
	fprintf(stdout, "Run the program with these arguments:\n\t$ ./proj2 A C AGT CGT AWT CWT\n\n \
A = number of adult processes to generate\n \
C = number of child processes to generate\n \
AGT = maximal time for generating adult process\n \
CGT = maximal time for generating child process\n \
AWT = maximal time for which adult remains in the centre\n \
CWT = maximal time for which child remains in the centre\n");
}

int main(int argc, char **argv)
{
	sem_t* semaphore;
//	pid_t adult_gen;
//	pid_t child_gen;
	int adult_count;
	int child_count;
	int adult_gen_time;
	int child_gen_time;
	int adult_work_time;
	int child_work_time;
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	if (argc != 7)
	{
		fprintf(stderr, "Error: wrong arguments passed.\n");
		print_help();
		return 1;
	}

	adult_count = atoi(argv[1]);
	child_count = atoi(argv[2]);

	if ((adult_count < 0) || (child_count < 0))
	{
		fprintf(stderr, "Error: number of adult and child processes must be grater than 0.\n");
		print_help();
		return 1;
	}

	adult_gen_time = atoi(argv[3]);
	child_gen_time = atoi(argv[4]);

	if ((adult_gen_time < 0) || (adult_gen_time >= 5001))
	{
		fprintf(stderr, "Error: maximal time for generating adult process (AGT) must be within 0 and 5001 milisecunds.\n");
		print_help();
		return 1;
	}

	if ((child_gen_time < 0) || (child_gen_time >= 5001))
	{
		fprintf(stderr, "Error: maximal time for generating child process (CGT) must be within 0 and 5001 milisecunds.\n");
		print_help();
		return 1;
	}
	

	adult_work_time = atoi(argv[5]);
	child_work_time = atoi(argv[6]);

	if ((adult_work_time < 0) || (adult_work_time >= 5001))
	{
		fprintf(stderr, "Error: maximal time for which adult remains in the centre (AWT) must be within 0 and 5001 milisecunds.\n");
		print_help();
		return 1;
	}
	

	if ((child_work_time < 0) || (child_work_time >= 5001))
	{
		fprintf(stderr, "Error: maximal time for which child remains in the centre (CWT) must be within 0 and 5001 milisecunds.\n");
		print_help();
		return 1;
	}


	printf("a = %d\nc = %d\nagt = %d\ncgt = %d\nawt = %d\ncwt = %d\n", adult_count, child_count, adult_gen_time, child_gen_time, adult_work_time, child_work_time);

	if ((semaphore = sem_open(SEM_NAME, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED)
	{
		fprintf(stderr, "Error: unable to creat a semaphore\n");
		// perror("Semaphore");
		return 1;
	}



	return 0;
}

