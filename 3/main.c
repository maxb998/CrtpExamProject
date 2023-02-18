#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include <pthread.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/wait.h>

//#include <sys/ipc.h>
//#include <sys/types.h>


typedef struct
{
    sem_t mutexSem;
    sem_t produceSem;
    sem_t producedSem;
    int value;
    bool quit;
} SharedObj;


int main(int argc, char * argv[]);

void producer(SharedObj * s);
void startClients(int n);
void client();
void actor(SharedObj * s);



int main(int argc, char * argv[])
{
    //  setup shared memory
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedObj),  SHM_R | SHM_W);

    // check for errors
    if (shmid == -1)
    {
        printf("Error on shmget\n");
        exit(EXIT_FAILURE);
    }

    // points to the shared memory starting address
    SharedObj * producer_actor = (SharedObj*)shmat(shmid, NULL, 0666);

    // check for errors
    if ((!producer_actor))
    {
        printf("Error on shmat\n");
        exit(EXIT_FAILURE);
    }

    // init mutex semaphore to avoid race condition between actor and producer process
    sem_init(&producer_actor->mutexSem, 1, 1);

    // init semaphores that regulates production rate
    sem_init(&producer_actor->produceSem, 1, 0);
    sem_init(&producer_actor->producedSem, 1, 1);
    
    // init other shared values
    producer_actor->value = 0;
    producer_actor->quit = false;

    // fork producer process
    pid_t producerPid = fork();
    if (producerPid == 0)
    {
        producer(producer_actor);
        exit(EXIT_FAILURE);    // shouldn't be necessary
    }

    actor(producer_actor);

    // wait producer to finish
    waitpid(producerPid, NULL, 0);

    // print result
    printf("%d\n", producer_actor->value);

    return EXIT_SUCCESS;
}



void producer(SharedObj * s)
{
    while(true)
    {
        sem_wait(&s->produceSem);
        sem_wait(&s->mutexSem);

        if (s->quit)
        {
            sem_post(&s->producedSem);
            sem_post(&s->mutexSem);
            break;
        }
        else
        {
            s->value++;
            sem_post(&s->producedSem);
            sem_post(&s->mutexSem);
        }
    }
    exit(0);
}

void startClients(int n)
{

}

void client()
{

}

void actor(SharedObj * s)
{
    while (true)
    {
        sem_wait(&s->producedSem);
        sem_wait(&s->mutexSem);
        
        if (s->value >= 1000)
        {
            s->quit = true;
            sem_post(&s->produceSem);
            sem_post(&s->mutexSem);
            break;
        }
        
        sem_post(&s->produceSem);
        sem_post(&s->mutexSem);
    }
}