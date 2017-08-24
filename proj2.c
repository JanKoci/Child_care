/**
****************************************************************************************************************
* IOS-projekt2, Child Care
* @file proj2.c
* @author Jan Koci
* @date 28.4.2017
* @brief This program is a implementation of the Child Care problem using posix semaphores.
* @details There is a centre for children at which state regulations require that there is always one adult present 
	for every three children. At the begining the main process creates two other processes, one generates
	childres and the other for generating adults. Both children and adults have their terms they have to follow
	and respect when either entering or leaving the centre. The main process then waits for all processes to terminate
	and then terminates itself.
****************************************************************************************************************
*/
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
#include "proj2.h"

// Prototypes of functions defined below
void print_help();
void set_resources();
void clean_resources();
void child();
void adult();


// global variables used by semaphores (read only)
int AWT; // adult-work-time
int CWT; // child-work-time
int child_count; // number of child processes to be created
int adult_count; // number of adult processes to be created
FILE* logfile = NULL; // output file

/**
* Posix semaphores used for synchronization
* mutex = mutual exclusion, only one process at a time can access shared variables --> preventing race condition
* child_queue = queue of children that want to enter, but have to wait for an adult to come
* adult_queue = queue of adults that want to leave the centre but have to wait for some children to leave before them
* after_you = when an adult process entered and there is a child waiting in the queue, the adult waits for the child to
			  enter before he tries to leave
* finish = all process have to wait for the others before they finish
*/
sem_t* mutex = NULL, *child_queue = NULL, *adult_queue = NULL, *after_you = NULL, *finish = NULL;

/**
* shm_adult = number of adults at the centre
* shm_adultID = identifier of the shared memory segment
* shm_child = number of children at the centre
* shm_leaving = number of adults waiting in the adult_queue
* shm_waiting = number of children waiting in the child_queue
* shm_cpnum = Child Process NUMber, counts child processes
* shm_apnum = Adult Process NUMber, counts adults
* shm_counter = counts logs written to logfile
* shm_sync_finish = counts processes that left the centre and wait for others to finish
* shm_child_day = is "1" when all adults generated left the centre and so children can enter with no rules
*/
int *shm_adult = NULL, shm_adultID = 0, *shm_child = NULL, shm_childID = 0, \
	*shm_leaving = NULL, shm_leavingID = 0, *shm_waiting = NULL, shm_waitingID = 0, *shm_cpnum = NULL, shm_cpnumID = 0, \
	*shm_apnum = NULL, shm_apnumID = 0, *shm_counter = NULL, shm_counterID = 0, *shm_sync_finish = NULL, shm_sync_finishID = 0, \
	*shm_child_day = NULL, shm_child_dayID = 0;

int main(int argc, char **argv)
{
	pid_t pid1, pid2; // process identifiers
	int random_time;
	
	int adult_gen_time;
	int child_gen_time;
	int adult_work_time;
	int child_work_time;
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

//---------------------- COMMAND LINE ARGUMENTS -------------------------------------------------------------------

	if (argc != 7)
	{
		fprintf(stderr, "Error: wrong arguments passed.\n");
		print_help();
		exit(1);
	}

	adult_count = atoi(argv[1]);
	child_count = atoi(argv[2]);

	if ((adult_count < 0) || (child_count < 0))
	{
		fprintf(stderr, "Error: number of adult and child processes must be grater than 0.\n");
		print_help();
		exit(1);
	}

	adult_gen_time = atoi(argv[3]);
	child_gen_time = atoi(argv[4]);

	if ((adult_gen_time < 0) || (adult_gen_time >= 5001))
	{
		fprintf(stderr, "Error: maximal time for generating adult process (AGT) must be within 0 and 5001 milisecunds.\n");
		print_help();
		exit(1);
	}

	if ((child_gen_time < 0) || (child_gen_time >= 5001))
	{
		fprintf(stderr, "Error: maximal time for generating child process (CGT) must be within 0 and 5001 milisecunds.\n");
		print_help();
		exit(1);
	}
	

	adult_work_time = atoi(argv[5]);
	child_work_time = atoi(argv[6]);

	if ((adult_work_time < 0) || (adult_work_time >= 5001))
	{
		fprintf(stderr, "Error: maximal time for which adult remains in the centre (AWT) must be within 0 and 5001 milisecunds.\n");
		print_help();
		exit(1);
	}
	

	if ((child_work_time < 0) || (child_work_time >= 5001))
	{
		fprintf(stderr, "Error: maximal time for which child remains in the centre (CWT) must be within 0 and 5001 milisecunds.\n");
		print_help();
		exit(1);
	}

//======================================= END ARGUMENTS ===================================================================
	
	// open logfile
	if ((logfile = fopen("proj2.out", "w")) == NULL)
	{
		fprintf(stderr, "Error: cannot open file proj2.out\n");
		exit(2);
	}
	setbuf(logfile, NULL); // for printing without buffering

	AWT = adult_work_time;
	CWT = child_work_time;
	srandom(time(0));
	
	set_resources(); // creates all semaphores and shared variables

	// pids of all processes
	pid_t adults[adult_count];
	pid_t children[child_count];

	// first fork --> child (generates children)
	//			  --> adult 
 	if ((pid1 = fork()) < 0)
	{
	//------ ERROR ------------------------------------
		fprintf(stderr, "Error: unable to fork process\n");
		clean_resources();
		exit(2);
	}
	else if (pid1 == 0)
	{
	// --- CHILD ------(generating children)----
		for (int i = 0; i < child_count; i++)
		{
			pid_t local_pid1;

			// waits before generating
			if (child_gen_time > 0)
			{
				random_time = random() % child_gen_time * 1000;
				usleep(random_time);
			}
			// creating CHILDREN
			if ((local_pid1 = fork()) < 0)
			{
			// --- ERROR -------------------------------
				fprintf(stderr, "Error: unable to fork process\n");
				for (int i = 0; i < child_count; i++)
				{
					// need to kill all created processes
					kill(children[i], SIGKILL);
				}
				clean_resources();
				exit(2);
				
			}
			else if (local_pid1 == 0)
			{
			// --- CHILD -------------------- 
				child();
			}
			else
			{
			// --- PARENT --------------------
				children[i] = local_pid1;
			}
		}
	}
	else
	{
	// --- PARENT -------------------------------
		
		// second fork --> parent 
		//			   --> child (generating children)
		// 			   --> child (generating adults)
		if ((pid2 = fork()) < 0)
		{
		// --- ERROR -------------------------------
			fprintf(stderr, "Error: unable to fork process\n");
			for (int i = 0; i < child_count; i++)
			{
				// need to kill all created processes
				kill(children[i], SIGKILL);
			}
			kill(pid1, SIGKILL);
			exit(2);
		}
		else if (pid2 == 0)
		{
		// CHILD---------(generating adults)------
			if (adult_count == 0)
			{
				*shm_child_day = 1;
			}
			for (int j = 0; j < adult_count; j++)
			{
				pid_t local_pid2;

				// wait before generating
				if (adult_gen_time > 0)
				{
					random_time = random() % adult_gen_time * 1000;
					usleep(random_time);
				}

				if ((local_pid2 = fork()) < 0)
				{
				// --- ERROR ---------------------------
					fprintf(stderr, "Error: unable to fork process\n");
					for (int i = 0; i < child_count; i++)
					{
						// need to kill all created processes
						kill(children[i], SIGKILL);
					}
					for (int j = 0; j < adult_count; j++)
					{
						kill(adults[j], SIGKILL);
					}
					kill(pid1, SIGKILL);
					clean_resources();
					exit(2);
				}
				else if (local_pid2 == 0)
				{
				// --- CHILD -----------------------
					adult();
				}
				else
				{
				// --- PARENT ----------------------
					adults[j] = local_pid2;
				}
			}
		}
		else
		{
			;
		}
	}

	// main process waits for all processes to terminate
	for (int i = 0; i < child_count; i++)
	{
		waitpid(children[i], NULL, 0);
	}
	for (int j = 0; j < adult_count; j++)
	{
		waitpid(adults[j], NULL, 0);
	}
	waitpid(pid1, NULL, 0);
	waitpid(pid2, NULL, 0);

	// cleans semaphores and all shared variables
	clean_resources();
	// close logfile
	fclose(logfile);
	exit(0);
}

/**
* @brief function for all children 
* @details 1. child starts -> 2. child wants to enter the centre, it has to look at the number of adults at the centre 
	and according to that either enters of waits in the child queue until some adult enters the centre. ->
	-> 3. child sleeps at the centre -> 4. child trying to leave, looks whether there are some adults waiting
	in the adult_queue, if so child lets the adult in, but only in case it would not brake the rules of the centre 
	and then the child leaves, otherwise it will leave directly. -> 5. child increments the number of left processes 
	and waits for others to finish -> 6. when child left as the last process, it indicates others they can leave.
*/
void child()
{
	int random_time;
	int id;

	sem_wait(mutex);
	*shm_cpnum += 1;
	id = *shm_cpnum;
	fprintf(logfile, "%d\t\t: C %d\t: started\n", ++(*shm_counter), id);
	sem_post(mutex);

	sem_wait(mutex);
	// comming to the centre
	if (( (*shm_child) < (3 * (*shm_adult)) ) || (*shm_child_day))
	{
		*shm_child += 1;
		fprintf(logfile, "%d\t\t: C %d\t: enter\n", ++(*shm_counter), id);
		sem_post(mutex);
	}
	else
	{
		*shm_waiting += 1;
		fprintf(logfile, "%d\t\t: C %d\t: waiting : %d : %d\n", ++(*shm_counter), id, *shm_adult, *shm_child);
		sem_post(mutex);

		sem_wait(child_queue);

		sem_wait(mutex);
		fprintf(logfile, "%d\t\t: C %d\t: enter\n", ++(*shm_counter), id);
		sem_post(after_you);
		sem_post(mutex);
	}
	// simulates activity at the centre
	if (CWT > 0)
	{
		random_time = (random() % CWT) * 1000;
		usleep(random_time);
	}

	sem_wait(mutex);
	fprintf(logfile, "%d\t\t: C %d\t: trying to leave\n", ++(*shm_counter), id);
	*shm_child -= 1;

	// if there are any processes in the adult_queue
	if (*shm_leaving)
	{
		if ((*shm_child) <= ( 3 * ((*shm_adult) - 1)))
		{
			*shm_leaving -= 1;
			*shm_adult -= 1;
			sem_post(adult_queue);
		}
	}
	fprintf(logfile, "%d\t\t: C %d\t: leave\n", ++(*shm_counter), id);
	*shm_sync_finish += 1;
	sem_post(mutex);

	// if I am the last process
	if (*shm_sync_finish == adult_count + child_count)
	{
		*shm_sync_finish += 1;
		sem_post(finish);
		sem_wait(mutex);
		fprintf(logfile, "%d\t\t: C %d\t: finished\n", ++(*shm_counter), id);	
		sem_post(mutex);
	}
	else
	{
		sem_wait(finish);
		sem_wait(mutex);
		fprintf(logfile, "%d\t\t: C %d\t: finished\n", ++(*shm_counter), id);
		sem_post(mutex);
		sem_post(finish);
	}
	exit(0);
}

/**
* @brief function for all adults
* @details 1. adult starts -> 2. adult enters the centre, looks whether there are any children waiting in the child_queue,
	if so adult lets them in but not more than 3, then he waits for them to enter before he tries to leave, otherwise he 
	enters directly -> 3. adult sleeps at the centre -> 4. adult trying to leave, if his exit would break the rules 
	of the centre he waits in the adult_queue for some child to leave -> 5. adult leaves, increments the value of left
	processes and if he is the last adult generated decleres the child_day -> 6. have to wait for all other processes 
	to leave before he can finish, if he is the last process that left, he indicates others they can finish.
*/
void adult()
{
	int n;
	int random_time;
	int id;

	sem_wait(mutex);
	*shm_apnum += 1;
	id = *shm_apnum;
	fprintf(logfile, "%d\t\t: A %d\t: started\n", ++(*shm_counter), id);
	*shm_adult += 1;

	// comming to the centre
	if (*shm_waiting)
	{
		n = ((*shm_waiting) < 3) ? (*shm_waiting) : 3;
		for (int i = 0; i < n; i++)
		{
			sem_post(child_queue);
		}
		*shm_child += n;
		*shm_waiting -= n;
		fprintf(logfile, "%d\t\t: A %d\t: enter\n", ++(*shm_counter), id);
		sem_post(mutex);
		for (int i = 0; i < n; i++)
		{
			sem_wait(after_you);
		}
	}
	else
	{
		fprintf(logfile, "%d\t\t: A %d\t: enter\n", ++(*shm_counter), id);
		sem_post(mutex);
	}

	// simulates his activity at the centre
	if (AWT > 0)
	{
		random_time = (random() % AWT) * 1000;
		usleep(random_time);
	}

	// wants to leave
	sem_wait(mutex);
	fprintf(logfile, "%d\t\t: A %d\t: trying to leave\n", ++(*shm_counter), id);
	if ((*shm_child) <= (3 * ((*shm_adult) - 1)))
	{
		*shm_adult -= 1;
	}
	else
	{
		*shm_leaving += 1;
		fprintf(logfile, "%d\t\t: A %d\t: waiting : %d : %d\n", ++(*shm_counter), id, *shm_adult, *shm_child);
		sem_post(mutex);
		sem_wait(adult_queue);

		sem_wait(mutex);
	}
	fprintf(logfile, "%d\t\t: A %d\t: leave\n", ++(*shm_counter), id);
	*shm_sync_finish += 1;
	// if I am the last generated adult, all other children can wait with no rules -> child_day
	if (id == adult_count)
	{
		*shm_child_day = 1;
		if (*shm_waiting)
		{
			n = ((*shm_waiting) < 3) ? (*shm_waiting) : 3;
			for (int i = 0; i < n; i++)
			{
				sem_post(child_queue);
			}
			*shm_child += n;
			*shm_waiting -= n;
		}
	}
	sem_post(mutex);

	sem_wait(mutex);
	// wait for others to leave before finishing
	if (*shm_sync_finish == adult_count + child_count)
	{
		*shm_sync_finish += 1;
		sem_post(mutex);
		sem_post(finish);

		sem_wait(mutex);
		fprintf(logfile, "%d\t\t: A %d\t: finished\n", ++(*shm_counter), id);	
		sem_post(mutex);
	}
	else
	{
		sem_post(mutex);

		sem_wait(finish);
		sem_wait(mutex);
		fprintf(logfile, "%d\t\t: A %d\t: finished\n", ++(*shm_counter), id);
		sem_post(mutex);
		sem_post(finish);
	}
	exit(0);
}


/**
* @brief prepares all shared variables and semaphores
*/
void set_resources()
{
	// Initialize shared variables
	if ((shm_adultID = shmget(IPC_PRIVATE, sizeof (int), IPC_CREAT | 0666)) < 0)
	{
		perror("shmget");
		clean_resources();
        exit(2);
	}
	if ((shm_adult = (int *) shmat(shm_adultID, NULL, 0)) == NULL)
	{
		perror("shmget");
		clean_resources();
		exit(2);
	}
	else
	{
		*shm_adult = 0;
	}
	// ===========================================================================

	if ((shm_childID = shmget(IPC_PRIVATE, sizeof (int), IPC_CREAT | 0666)) < 0)
	{
		perror("shmget");
		clean_resources();
        exit(2);
	}
	if ((shm_child = (int *) shmat(shm_childID, NULL, 0)) == NULL)
	{
		perror("shmget");
		clean_resources();
		exit(2);
	}
	else
	{
		*shm_child = 0;
	}
	// ===========================================================================

	if ((shm_leavingID = shmget(IPC_PRIVATE, sizeof (int), IPC_CREAT | 0666)) < 0)
	{
		perror("shmget");
		clean_resources();
        exit(2);
	}
	if ((shm_leaving = (int *) shmat(shm_leavingID, NULL, 0)) == NULL)
	{
		perror("shmget");
		clean_resources();
		exit(2);
	}
	else
	{
		*shm_leaving = 0;
	}
	// ===========================================================================

	if ((shm_waitingID = shmget(IPC_PRIVATE, sizeof (int), IPC_CREAT | 0666)) < 0)
	{
		perror("shmget");
		clean_resources();
        exit(2);
	}
	if ((shm_waiting = (int *) shmat(shm_waitingID, NULL, 0)) == NULL)
	{
		perror("shmget");
		clean_resources();
		exit(2);
	}
	else
	{
		*shm_waiting = 0;
	}
	// ===========================================================================

	if ((shm_cpnumID = shmget(IPC_PRIVATE, sizeof (int), IPC_CREAT | 0666)) < 0)
	{
		perror("shmget");
		clean_resources();
        exit(2);
	}
	if ((shm_cpnum = (int *) shmat(shm_cpnumID, NULL, 0)) == NULL)
	{
		perror("shmget");
		clean_resources();
		exit(2);
	}
	else
	{
		*shm_cpnum = 0;
	}
	// ===========================================================================

	if ((shm_apnumID = shmget(IPC_PRIVATE, sizeof (int), IPC_CREAT | 0666)) < 0)
	{
		perror("shmget");
		clean_resources();
        exit(2);
	}
	if ((shm_apnum = (int *) shmat(shm_apnumID, NULL, 0)) == NULL)
	{
		perror("shmget");
		clean_resources();
		exit(2);
	}
	else
	{
		*shm_apnum = 0;
	}
	// ===========================================================================

	if ((shm_counterID = shmget(IPC_PRIVATE, sizeof (int), IPC_CREAT | 0666)) < 0)
	{
		perror("shmget");
		clean_resources();
        exit(2);
	}
	if ((shm_counter = (int *) shmat(shm_counterID, NULL, 0)) == NULL)
	{
		perror("shmget");
		clean_resources();
		exit(2);
	}
	else
	{
		*shm_counter = 0;
	}
	// ===========================================================================

	if ((shm_sync_finishID = shmget(IPC_PRIVATE, sizeof (int), IPC_CREAT | 0666)) < 0)
	{
		perror("shmget");
		clean_resources();
        exit(2);
	}
	if ((shm_sync_finish = (int *) shmat(shm_sync_finishID, NULL, 0)) == NULL)
	{
		perror("shmget");
		clean_resources();
		exit(2);
	}
	else
	{
		*shm_sync_finish = 0;
	}
	// ===========================================================================

	if ((shm_child_dayID = shmget(IPC_PRIVATE, sizeof (int), IPC_CREAT | 0666)) < 0)
	{
		perror("shmget");
		clean_resources();
        exit(2);
	}
	if ((shm_child_day = (int *) shmat(shm_child_dayID, NULL, 0)) == NULL)
	{
		perror("shmget");
		clean_resources();
		exit(2);
	}
	else
	{
		*shm_child_day = 0;
	}
// ===========================================================================
	// Initialize semaphores
// ===========================================================================
	if ((mutex = sem_open(MUTEX_NAME, O_CREAT | O_EXCL, 0666, 1)) == SEM_FAILED) 
    { 
    	clean_resources();
    	perror("shmget");
    	exit(2);
    }
    if ((adult_queue = sem_open(ADULT_QUEUE_NAME, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) 
    { 
    	clean_resources();
    	perror("shmget");
    	exit(2);
    }
    if ((child_queue = sem_open(CHILD_QUEUE_NAME, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) 
    { 
    	clean_resources();
    	perror("shmget");
    	exit(2);
    }
    if ((after_you = sem_open(AFTER_YOU_NAME, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) 
    { 
    	clean_resources();
    	perror("shmget");
    	exit(2);
    }
    if ((finish = sem_open(FINISH_SEM, O_CREAT | O_EXCL, 0666, 0)) == SEM_FAILED) 
    { 
    	clean_resources();
    	perror("shmget");
    	exit(2);
    }
}

/*
* @brief cleans all shared variables and semaphores
*/
void clean_resources()
{
	// just in case it was initialized
	if (shm_adultID)
	{
    	shmctl(shm_adultID, IPC_RMID, NULL);
    }
    if (shm_childID)
    {
    	shmctl(shm_childID, IPC_RMID, NULL);
    }
    if (shm_leavingID)
    {
    	shmctl(shm_leavingID, IPC_RMID, NULL);
    }
    if (shm_waitingID)
    {
    	shmctl(shm_waitingID, IPC_RMID, NULL);
    }
    if (shm_cpnumID)
    {
    	shmctl(shm_cpnumID, IPC_RMID, NULL);
    }
    if (shm_apnumID)
    {
    	shmctl(shm_apnumID, IPC_RMID, NULL);
    }
    if (shm_counterID)
    {
    	shmctl(shm_counterID, IPC_RMID, NULL);
    }
    if (shm_sync_finishID)
    {
    	shmctl(shm_sync_finishID, IPC_RMID, NULL);
    }
    if (shm_child_dayID)
    {
    	shmctl(shm_child_dayID, IPC_RMID, NULL);
    }

	// Semaphores
    if (mutex)
    {
    	sem_close(mutex);
		sem_unlink(MUTEX_NAME);
	}
	if (adult_queue)
	{
    	sem_close(adult_queue);
    	sem_unlink(ADULT_QUEUE_NAME);
    }
    if (child_queue)
    {
    	sem_close(child_queue);
    	sem_unlink(CHILD_QUEUE_NAME);
    }
    if (after_you)
    {
    	sem_close(after_you);
    	sem_unlink(AFTER_YOU_NAME);
    }
    if (finish)
    {
    	sem_close(finish);
    	sem_unlink(FINISH_SEM);
    }
}

/**
* @brief prints help, when wrong arguments are passed from the terminal
*/
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


