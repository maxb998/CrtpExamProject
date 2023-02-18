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

#define MAX_THREADS 256


typedef struct
{
    sem_t mutexSem;
    sem_t produceSem;
    sem_t producedSem;
    int value;
    bool quit;
} SharedObj;

typedef struct
{
    pthread_t threads[MAX_THREADS];
    pthread_cond_t mutex;
    pthread_cond_t consumeCond;
} ThreadManagementValues;




int main(int argc, char * argv[]);


// init memory sharing to communicate with producer process
SharedObj startMemSharing();

// start the producer as a process and returns its pid
pid_t startProducer();

// producer task
void producer();

// start client's threads and return a struct containing values to manage those threads
void startClients(int n);

// clients task
void * client();

// actor monitoring task, as a separate thread
void * actor();


char * buffer;
size_t buffId;
SharedObj s;
ThreadManagementValues t;
bool clientsQuit;



int main(int argc, char * argv[])
{
    // PARSE ARGUMENTS: TODO
    if (argc != 3)
    {
        printf("Usage: main <BUFFER_SIZE> <CLIENTS_COUNT(THREADS)>");
        exit(EXIT_SUCCESS);
    }
    size_t bufferSize = 0, clientCount = 0;
    sscanf("%d", argv[1], &bufferSize);
    sscanf("%d", argv[2], &clientCount);

    // setup buffer
    buffer = (char*)malloc(bufferSize * sizeof(char));

    // setup mem sharing
    //s = startMemSharing();

    // start producer process and get its pid
    //pid_t producerPid = startProducer(&s);

    // now with the clients
    clientsQuit = false;
    startClients(clientCount);

    // finally the actor monitoring task on a separate thread
    pthread_t actorThread;
    pthread_create(&actorThread, NULL, actor, NULL);

    // wait producer to finish
    //waitpid(producerPid, NULL, 0);

    free(buffer);

    return EXIT_SUCCESS;
}



SharedObj startMemSharing()
{
    // setup shared memory
    int shmid = shmget(IPC_PRIVATE, sizeof(SharedObj),  SHM_R | SHM_W);
    if (shmid == -1) // check for errors
    {
        printf("Error on shmget\n");
        exit(EXIT_FAILURE);
    }

    // points to the shared memory starting address
    SharedObj * sl = (SharedObj*)shmat(shmid, NULL, 0666);
    if ((!sl)) // check for errors
    {
        printf("Error on shmat\n");
        exit(EXIT_FAILURE);
    }

    return *sl;
}

pid_t startProducer()
{
    // init mutex semaphore to avoid race condition between actor and producer process
    sem_init(&s.mutexSem, 1, 1);

    // init semaphores that regulates production rate
    sem_init(&s.produceSem, 1, 0);
    sem_init(&s.producedSem, 1, 1);
    
    // init other shared values
    s.value = 0;
    s.quit = false;

    // fork producer process
    pid_t producerPid = fork();
    if (producerPid == 0)
    {
        producer();
        exit(EXIT_FAILURE);    // shouldn't be necessary but if it gets here there is an error
    }

    return producerPid;
}

void producer()
{
    while(true)
    {
        sem_wait(&s.produceSem);
        sem_wait(&s.mutexSem);

        if (s.quit)
        {
            sem_post(&s.producedSem);
            sem_post(&s.mutexSem);
            break;
        }
        else
        {
            s.value++;
            sem_post(&s.producedSem);
            sem_post(&s.mutexSem);
        }
    }
    exit(0);
}

void startClients(int n)
{  
    // init mutex & conditions
    pthread_mutex_init(&t.mutex, NULL);
    pthread_cond_init(&t.consumeCond, NULL);

    // start clients
    for (int i = 0; i < n; i++)
        pthread_create(t.threads[i], NULL, client(), NULL);
}

void * client()
{
    while (!clientsQuit)
    {
        pthread_mutex_lock(&t.mutex);

        
    }
    
}

void * actor()
{
    /*while (true)
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
    }*/
}