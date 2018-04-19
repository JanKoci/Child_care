# Child_care
## C program performing process synchronization using posix semaphores.\\
This program is an implementation of the Child Care synchronization problem using posix semaphores and fork system call. \
There is a centre for children with a rule that there is always one adult present for every three children. \
At the begining the main process creates two other processes, one generates children the other generates adults. \
Both children and adults have their terms they have to follow and respect when either entering or leaving the centre. \
The main process then waits for all processes to terminate and then terminates itself.
