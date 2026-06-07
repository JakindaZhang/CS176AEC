#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define MAX_CLIENTS 3
#define MAX_WORDS 15
#define MAX_WORD_LEN 9
#define MAX_INCORRECT 6

char words[MAX_WORDS][MAX_WORD_LEN];
int word_count = 0;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
int active_clients = 0;

void loadWords()
{
    FILE *fp = fopen("hangman_words.txt", "r");

    if (fp == NULL)
    {
        perror("hangman_words.txt");
        exit(1);
    }

    while (word_count < MAX_WORDS &&
           fgets(words[word_count], MAX_WORD_LEN, fp))
    {

        words[word_count][strcspn(words[word_count], "\n")] = '\0';

        if (strlen(words[word_count]) > 0)
        {
            word_count++;
        }
    }

    fclose(fp);
}
int recvall(int fd, void *buf, int n)
{
    char *p = (char *)buf;
    int got = 0;

    while (got < n)
    {
        int r = recv(fd, p + got, n - got, 0);

        if (r <= 0)
            return -1;

        got += r;
    }

    return got;
}

void sendMessage(int fd, const char *msg)
{
    unsigned char len = strlen(msg);

    send(fd, &len, 1, 0);
    send(fd, msg, len, 0);
}

void sendGameState(int fd, char *display, int wordlen, char *wrong, int wrongCount)
{
    unsigned char flag = 0;
    unsigned char wl = wordlen;
    unsigned char ni = wrongCount;

    send(fd, &flag, 1, 0);
    send(fd, &wl, 1, 0);
    send(fd, &ni, 1, 0);
    send(fd, display, wordlen, 0);

    if (wrongCount > 0)
    {
        send(fd, wrong, wrongCount, 0);
    }
}
void playGame(int fd)
{

    unsigned char start;

    if (recvall(fd, &start, 1) <= 0)
        return;

    char *word = words[rand() % word_count];

    int len = strlen(word);

    char display[MAX_WORD_LEN];
    char wrong[MAX_INCORRECT];

    int wrongCount = 0;

    for (int i = 0; i < len; i++)
        display[i] = '_';

    display[len] = '\0';

    sendGameState(fd,
                  display,
                  len,
                  wrong,
                  wrongCount);

    while (1)
    {

        unsigned char msglen;
        char guess;

        if (recvall(fd, &msglen, 1) <= 0)
            return;

        if (msglen != 1)
            return;

        if (recvall(fd, &guess, 1) <= 0)
            return;

        int found = 0;

        for (int i = 0; i < len; i++)
        {

            if (word[i] == guess)
            {
                display[i] = guess;
                found = 1;
            }
        }

        if (!found)
        {
            wrong[wrongCount++] = guess;
        }

        if (strcmp(display, word) == 0)
        {

            char msg[64];

            sprintf(msg, "The word was %s", word);

            sendMessage(fd, msg);
            sendMessage(fd, "You Win!");
            sendMessage(fd, "Game Over!");

            return;
        }

        if (wrongCount >= MAX_INCORRECT)
        {

            char msg[64];

            sprintf(msg, "The word was %s", word);

            sendMessage(fd, msg);
            sendMessage(fd, "You Lose!");
            sendMessage(fd, "Game Over!");

            return;
        }

        sendGameState(fd,
                      display,
                      len,
                      wrong,
                      wrongCount);
    }
}
void *clientThread(void *arg)
{

    int fd = *(int *)arg;
    free(arg);

    playGame(fd);

    close(fd);

    pthread_mutex_lock(&lock);
    active_clients--;
    pthread_mutex_unlock(&lock);

    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: hangman_server <port>\n");
        return 1;
    }

    srand(time(NULL));

    /* TODO: Load words. */
    loadWords();
    if (word_count == 0)
    {
        fprintf(stderr, "No words loaded\n");
        return 1;
    }
    /* TODO: Create socket, bind, listen (see socket_server_example.c). */
    int srv = socket(AF_INET, SOCK_STREAM, 0);

    if (srv < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(srv,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(atoi(argv[1]));

    if (bind(srv,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0)
    {

        perror("bind");
        return 1;
    }

    if (listen(srv, 10) < 0)
    {
        perror("listen");
        return 1;
    }

    printf("Listening on port %s\n",
           argv[1]);
    while (1)
    {
        /* TODO: Accept a connection and spawn a thread to handle it. */

        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);

        int client =
            accept(srv,
                   (struct sockaddr *)&caddr,
                   &clen);

        if (client < 0)
            continue;

        pthread_mutex_lock(&lock);

        if (active_clients >= MAX_CLIENTS)
        {

            pthread_mutex_unlock(&lock);

            sendMessage(client,
                        "server-overloaded");

            close(client);

            continue;
        }

        active_clients++;

        pthread_mutex_unlock(&lock);

        pthread_t tid;

        int *fdptr = malloc(sizeof(int));
        if (fdptr == NULL)
        {
            pthread_mutex_lock(&lock);
            active_clients--;
            pthread_mutex_unlock(&lock);

            close(client);
            continue;
        }
        *fdptr = client;

        pthread_create(&tid,
                       NULL,
                       clientThread,
                       fdptr);

        pthread_detach(tid);
    }
    return 0;
}
