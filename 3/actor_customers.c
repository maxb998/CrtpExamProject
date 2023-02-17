#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include <pthread.h>
#include <sys/shm.h>
//#include <sys/ipc.h>
//#include <sys/types.h>

#include <errno.h>


#define SHARED_MEM_KEY 22

int main(int argc, char * argv[]);

void * getSharedMemProducer();


int main(int argc, char * argv[])
{
    int * c = (int*)getSharedMemProducer();
    
    *c = 100;

    while (true)
    {
        sleep(1);
        printf("%d\n", c[0]);
    }

    
}

void * getSharedMemProducer()
{
    key_t key = SHARED_MEM_KEY;
    int memId = shmget(key, sizeof(int), IPC_CREAT | 0666);

    if (memId == -1)
        memId = shmget(key, sizeof(int), 0);

    void * ptr;
    if (memId != -1)
        ptr = (void*)shmat(memId, NULL, 0666);

    if (!ptr)
    {
        printf("ERROR ON MEMORY SHARING WITH PRODUCER\n");
        exit(EXIT_FAILURE);
    }

    return ptr;
}