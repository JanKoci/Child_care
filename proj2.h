#ifndef PROJ2_H
#define PROJ2_H

// Documentation in source file
void print_help();
void set_resources();
void clean_resources();
void child();
void adult();

// Names of used semaphores
#define MUTEX_NAME "/woodies_mutex"
#define ADULT_QUEUE_NAME "/woodies_adult_queue"
#define CHILD_QUEUE_NAME "/woodies_child_queue"
#define AFTER_YOU_NAME "/woodies_gentle_semaphore"
#define FINISH_SEM "/woodies_finisher"

#endif // PROJ2_H