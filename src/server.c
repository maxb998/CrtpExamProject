#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define MAX_THREADS 256

int main(int argc, char *argv[]);

void *serverThread(void *portPtr);
void *displayRecievedData(void *arg);

struct ActorData
{
    uint16_t clientsCount;
    uint16_t msgQueueLen;
    uint32_t msgProduced;
    uint32_t msgConsumed[MAX_THREADS];
};

bool stop = false;
struct ActorData receivedData;

pthread_mutex_t mutex;
pthread_cond_t newDataAvailable;

int main(int argc, char *argv[])
{
    memset(&receivedData, 0, sizeof(receivedData));

    // get port number as argument
    int port = 0;
    if (argc < 2)
    {
        printf("Usage: server <port>\n");
        exit(0);
    }
    sscanf(argv[1], "%d", &port);

    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&newDataAvailable, NULL);

    // start server in another thread
    pthread_t serverT;
    pthread_create(&serverT, NULL, serverThread, (void *)&port);

    pthread_t displayT;
    pthread_create(&displayT, NULL, displayRecievedData, NULL);

    // start managing terminal output
    printf("+--------------------+--------------------------+-------------------------------------------------------\n");
    printf("|   Queue Length %%   |     Produced Messages    |           Consumed Messages (per client)              \n");
    printf("+--------------------+--------------------------+-------------------------------------------------------\n");
    char command;
    while (true)
    {
        switch (getchar())
        {
        case 'c': // reset ouput
            // clear console window
            system("clear");

            // print table header again
            printf("+--------------------+--------------------------+-------------------------------------------------------\n");
            //              20                      26
            printf("|   Queue Length %%   |     Produced Messages    |           Consumed Messages (per client)              \n");
            printf("+--------------------+--------------------------+-------------------------------------------------------\n");
            break;

        case 'q': // quit
            stop = true;
            pthread_join(serverT, NULL);
            pthread_join(displayT, NULL);
            exit(EXIT_SUCCESS);
            break;

        default:
            break;
        }
    }

    return EXIT_SUCCESS;
}

void *serverThread(void *portPtr)
{
    int port = *(int *)portPtr;

    // create new socket
    int sd;
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // set socket options
    int reuse = 1;
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");
#ifdef SO_REUSEPORT
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, (const char *)&reuse, sizeof(reuse)) < 0)
        perror("setsockopt(SO_REUSEPORT) failed");
#endif

    // init socket data with server informations
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    // bind socket to specified port number
    if (bind(sd, (struct sockaddr *)&sin, sizeof(sin)) == -1)
    {
        perror("bind");
        exit(1);
    }

    // Set maximum queue lenght for clients to 1, we only use one actor
    if (listen(sd, 1) == -1)
    {
        perror("listen");
        exit(1);
    }

    struct sockaddr_in actorSin;
    int sizeOfActorSin = sizeof(actorSin), actorSd;
    int totSize = 0, curSize = 0;
    // void * receivedStream = &receivedData;
    while (!stop)
    {
        //printf("waiting connection\n");
        // accept actor connection to server
        if ((actorSd = accept(sd, (struct sockaddr *)&actorSin, &sizeOfActorSin)) == -1)
        {
            perror("accept");
            exit(1);
        }
        //printf("connection established\n");

        // now receive messages until end of stream
        while (true)
        {
            // recv may not be able to capture all bytes on the first go, so, even if it shouldn't happen we take precaution
            totSize = 0;
            curSize = 0;
            while (totSize < sizeof(receivedData))
            {
                curSize = recv(actorSd, &receivedData + totSize, sizeof(receivedData) - totSize, 0);
                if (curSize <= 0)
                    break;
                totSize += curSize;
            }
            if (curSize <= 0)
                break;

            // respond first, then process the data
            char stopCmd = 0;
            if (stop)
                stopCmd = 255;
            if (send(actorSd, &stopCmd, 1, 0) == -1)
                break;

            // signal other thread that new data is avaliable and it can show it
            pthread_mutex_lock(&mutex);
            pthread_cond_signal(&newDataAvailable);
            pthread_mutex_unlock(&mutex);

            // covert from network byte order
            /*receivedData.clientsCount = ntohs(receivedData.clientsCount);
            receivedData.msgQueueLen = ntohs(receivedData.msgQueueLen);
            receivedData.msgProduced = ntohl(receivedData.msgProduced);
            for (size_t i = 0; i < receivedData.clientsCount; i++)
                receivedData.msgConsumed[i] = ntohl(receivedData.msgConsumed[i]);// */
        }
    }

    close(sd);
    printf("Server is closed\n");

    return NULL;
}

void *displayRecievedData(void *arg)
{
    char queueBar[20];
    while (!stop)
    {
        // wait for new data
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&newDataAvailable, &mutex);
        pthread_mutex_unlock(&mutex);

        // draw new data line
        for (size_t i = 0; i < 20; i++)
        {
            if (receivedData.msgQueueLen >= 5 * i)
                queueBar[i] = '=';
            else
                queueBar[i] = ' ';
        }

        printf("|%s|        %10u        |", queueBar, receivedData.msgProduced);
        for (size_t j = 0; j < receivedData.clientsCount; j++)
            printf("  %10u", receivedData.msgConsumed[j]);
        printf("\n");
    }

    return NULL;
}