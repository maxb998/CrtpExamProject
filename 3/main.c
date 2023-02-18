#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include <pthread.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <sys/wait.h>

//#include <sys/ipc.h>
//#include <sys/types.h>

#define MAX_THREADS 256
 
static void * client(void * threanNum);
static void * producer(void * fname);

// buffer and output from clients
unsigned long *buffer, *clientOut;
// represent the current position where the next item must be written in the buffer
size_t bufferWritePos = 0, bufferSize = 0;
// used by the producer to stop the program when the file has ended
bool finish = false;

// mutex
pthread_mutex_t mutex;
pthread_cond_t consumeReady, produceReady;

int main(int argc, char * argv[])
{
    // parse arguments
    if (argc != 4)
    {
        printf("Usage: main <BUFFER_SIZE> <CLIENTS_COUNT(THREADS)> <INPUT_FILE_PATH>\n");
        exit(EXIT_SUCCESS);
    }
    size_t clientCount = 0;
    sscanf(argv[1], "%ld", &bufferSize);
    sscanf(argv[2], "%ld", &clientCount);
    if (clientCount > MAX_THREADS)
    {
        printf("ERROR: Cannot process more than 256 threads");
        exit(EXIT_SUCCESS);
    }
    if (access(argv[3], F_OK) != 0)
    {
        printf("ERROR: File specified does not exists");
        exit(EXIT_SUCCESS);
    }

    // init buffer and output array
    buffer = (unsigned long*)malloc(bufferSize * sizeof(long));
    clientOut = (unsigned long*)malloc(bufferSize * sizeof(long));
    memset(clientOut, 0 , clientCount * sizeof(long));

    // init pthread values
    pthread_t clientThreads[MAX_THREADS], producerThread;
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&consumeReady, NULL);
    pthread_cond_init(&produceReady, NULL);
    
    // array needed to let each client identify itself
    size_t * nparange = (size_t*)malloc(clientCount * sizeof(size_t));
    for (size_t i = 0; i < clientCount; i++)
        nparange[i] = i;

    // start producer and clients
    pthread_create(&producerThread, NULL, producer, (void*)argv[3]);
    for (size_t i = 0; i < clientCount; i++)
        pthread_create(&clientThreads[i], NULL, client, (void*)&nparange[i]);
    

    // wait for termination
    pthread_join(producerThread, NULL);
    printf("PRODUCER HAS FINISHED\n");
    for (size_t i = 0; i < clientCount; i++)
        pthread_join(clientThreads[i], NULL);

    unsigned long sum = 0;
    for (size_t i = 0; i < clientCount; i++)
    {
        printf("Thread %lu returned as output %lu\n", i, clientOut[i]);
        sum += clientOut[i];
    }
    printf("\nThe sum of all elements is %lu\n\n", sum);

    free(buffer);
    free(clientOut);
    return EXIT_SUCCESS;
}


static void * client(void* threadNum)
{
    while (true)
    {
        pthread_mutex_lock(&mutex);

        // check if buffer is empty
        while (bufferWritePos == 0)
        {
            // if producer has read all file and buffer is empty quit
            if (finish)
            {
                pthread_cond_signal(&consumeReady);
                pthread_mutex_unlock(&mutex);
                goto client_end;
            }
            pthread_cond_wait(&consumeReady, &mutex);
        }
        
        bufferWritePos--;
        clientOut[*(size_t*)threadNum] += buffer[bufferWritePos];

        // reset buffer value because it has been consumed... hehehe
        buffer[bufferWritePos] = 0;
        
        pthread_cond_signal(&produceReady);
        pthread_mutex_unlock(&mutex);
    }
    client_end:
    return NULL;
}

static void * producer(void *fname)
{
    FILE *f;
    unsigned long n = 0;

    f = fopen((char*)fname, "r");
    while (fscanf(f, "%lu", &n) != EOF)
    {
        // lock mutex
        pthread_mutex_lock(&mutex);

        // check if buffer is full
        while (bufferWritePos >= bufferSize)
            pthread_cond_wait(&produceReady, &mutex);
        
        buffer[bufferWritePos] = n;
        bufferWritePos++;

        pthread_cond_signal(&consumeReady);
        pthread_mutex_unlock(&mutex);
    }
    fclose(f);

    pthread_mutex_lock(&mutex);
    finish = true;  // reached end of file
    pthread_mutex_unlock(&mutex);

    return NULL;
}

