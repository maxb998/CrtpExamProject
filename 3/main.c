#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>

#include <pthread.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_THREADS 256

static void *client(void *threanNum);
static void *producer(void *fname);
static void *actor(void *portPtr);
void randomWait(long max);

// buffer and output from clients
unsigned long *buffer, *clientOut;
// represent the current position where the next item must be written in the buffer
size_t bufferWritePos = 0, bufferSize = 0, clientsCount = 0;
// used by the producer to signal the clients that it won't be producing any more data
bool finish = false;
// millisecond in nanoseconds
const long millisec = 1000000L;

// changes to true when the server respondes to the actor with a command to stop and the producer is stopped
// like if the file was over
bool stopCommand = false;

// values that will be sended over tcp to server
uint32_t *clientMsgCount, producerMsgCount = 0;


struct ActorData
{
    uint16_t clientsCount;
    uint16_t msgQueueLen;
    uint32_t msgProduced;
    uint32_t msgConsumed[MAX_THREADS];
};


// mutex
pthread_mutex_t mutex;
pthread_cond_t consumeReady, produceReady;

int main(int argc, char *argv[])
{
    // parse arguments
    if (argc != 5)
    {
        printf("Usage: main <BUFFER_SIZE> <CLIENTS_COUNT(THREADS)> <INPUT_FILE_PATH> <TCP_SOCKET_PORT>\n");
        exit(EXIT_SUCCESS);
    }
    uint16_t port = 0;  // tcp connection port
    sscanf(argv[1], "%ld", &bufferSize);
    sscanf(argv[2], "%ld", &clientsCount);
    sscanf(argv[4], "%hu", &port);
    if (clientsCount > MAX_THREADS)
    {
        printf("ERROR: Cannot process more than 255 threads");
        exit(EXIT_FAILURE);
    }
    if (access(argv[3], F_OK) != 0)
    {
        printf("ERROR: File specified does not exists");
        exit(EXIT_FAILURE);
    }

    // init buffer and output arrays
    buffer = (unsigned long *)malloc(bufferSize * sizeof(long));
    clientOut = (unsigned long *)malloc(clientsCount * sizeof(long));
    clientMsgCount = (uint32_t *)malloc(clientsCount * sizeof(uint32_t));
    memset(clientOut, 0, clientsCount * sizeof(long));
    memset(clientMsgCount, 0, clientsCount * sizeof(uint32_t));

    // init pthread values
    pthread_t clientThreads[MAX_THREADS], producerThread;
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&consumeReady, NULL);
    pthread_cond_init(&produceReady, NULL);

    // array needed to let each client identify itself
    size_t *nparange = (size_t *)malloc(clientsCount * sizeof(size_t));
    for (size_t i = 0; i < clientsCount; i++)
        nparange[i] = i;

    // start producer and clients
    pthread_create(&producerThread, NULL, producer, (void *)argv[3]);
    for (size_t i = 0; i < clientsCount; i++)
        pthread_create(&clientThreads[i], NULL, client, (void *)&nparange[i]);

    // start actor thread
    pthread_t actorThread;
    pthread_create(&actorThread, NULL, actor, (void *)&port);

    // wait for termination
    pthread_join(producerThread, NULL);
    // printf("PRODUCER HAS FINISHED\n");
    for (size_t i = 0; i < clientsCount; i++)
        pthread_join(clientThreads[i], NULL);

    unsigned long sum = 0;
    for (size_t i = 0; i < clientsCount; i++)
    {
        // printf("Thread %lu returned as output %lu\n", i, clientOut[i]);
        sum += clientOut[i];
    }
    printf("\nThe sum of all elements is %lu\n\n", sum);

    free(buffer);
    free(clientOut);
    return EXIT_SUCCESS;
}

static void *client(void *threadNum)
{
    while (true)
    {
        pthread_mutex_lock(&mutex);

        // check if buffer is empty
        while (bufferWritePos == 0)
        {
            if (finish)  // if producer has read all file and buffer is empty quit
            {
                pthread_cond_signal(&consumeReady);
                pthread_mutex_unlock(&mutex);
                goto client_end;
            }
            pthread_cond_wait(&consumeReady, &mutex);
        }

        bufferWritePos--;
        clientOut[*(size_t *)threadNum] += buffer[bufferWritePos];

        // reset buffer value because it has been consumed... hehehe
        buffer[bufferWritePos] = 0;

        pthread_cond_signal(&produceReady);
        pthread_mutex_unlock(&mutex);

        // count +1 on message recived by a client
        clientMsgCount[*(size_t *)threadNum]++;

        // wait some time... plus some randomess
        // using nanosleep, on average we want to let each thread wait around
        randomWait(millisec * 4L);
    }
client_end:
    // printf("Thread %lu finished\n", *(size_t*)threadNum);
    return NULL;
}

static void *producer(void *fname)
{
    FILE *f;
    unsigned long n = 0;

    f = fopen((char *)fname, "r");
    while ((fscanf(f, "%lu", &n) != EOF) && (!stopCommand))
    {
        // lock mutex
        pthread_mutex_lock(&mutex);

        // check if buffer is full
        while (bufferWritePos >= bufferSize)
            pthread_cond_wait(&produceReady, &mutex);

        buffer[bufferWritePos] = n;
        bufferWritePos++;

        pthread_cond_signal(&consumeReady);
        pthread_mutex_unlock(&mutex);   // exit critical region

        producerMsgCount++;     // msg counter

        // wait some time... plus some randomess
        // using nanosleep, on average we want to let each thread wait around
        randomWait(millisec);
    }
    fclose(f);

    pthread_mutex_lock(&mutex);
    finish = true; // signal clients that no more messages will be produced
    pthread_cond_signal(&consumeReady);
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void randomWait(long max)
{
    long r = random();
    r = r * max / RAND_MAX;

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = r;

    nanosleep(&ts, NULL);
}

static void *actor(void *portPtr)
{
    uint16_t port = *(uint16_t*)portPtr;

    // setup message data
    struct ActorData m;
    //m.clientsCount = htons((uint16_t)clientsCount);
    m.clientsCount = (uint16_t)clientsCount;
    memset(m.msgConsumed, 0, MAX_THREADS * sizeof(uint32_t));
    
    // *** SETUP TCP CLIENT ***
    const char * hostname = "localhost";
    struct hostent *hp;
    if ((hp = gethostbyname(hostname)) == 0)
    {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    // init socket data with host information
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = ((struct in_addr *)(hp->h_addr_list[0]))->s_addr;
    sin.sin_port = htons(port);

    // create new socket
    int sd;
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // connnect socket to port and host
    if (connect(sd,(struct sockaddr *)&sin, sizeof(sin)) == -1)
    {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    char response;
    while (!finish)
    {
        // wait 0.5 seconds to send the next message to the server
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = millisec * 500L;
        nanosleep(&ts, NULL);

        // refresh data to send and conversion
        /*m.msgQueueLen = htons((uint16_t)(bufferWritePos*100/bufferSize));
        m.msgProduced = htonl(producerMsgCount);
        for (size_t i = 0; i < clientsCount; i++)
            m.msgConsumed[i] = htonl(clientMsgCount[i]);//*/
        m.msgQueueLen = (uint16_t)(bufferWritePos*100/bufferSize);
        m.msgProduced = producerMsgCount;
        for (size_t i = 0; i < clientsCount; i++)
            m.msgConsumed[i] = clientMsgCount[i];

        // send data
        if(send(sd, &m, sizeof(m), 0) == -1)
        {
            perror("send");
            exit(EXIT_FAILURE);
        }

        // recieve response, since it's a char with lenght 1 there should be no need to check if all bytes have been sent
        // since it's either 1 which means operation is successful or 0/-1 which both mean some kind of error
        if (recv(sd, &response, 1, 0) <= 0)
        {
            perror("recv");
            exit(EXIT_FAILURE);
        }

        // check response value
        if (response != 0)
        {
            stopCommand = true;
            break;
        }
    }

    close(sd);

    return NULL;
}