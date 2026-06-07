#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
int getGuess(char *guess)
{
   char line[100];

   while (1)
   {
      printf(">>>Letter to guess: ");

      if (fgets(line, sizeof(line), stdin) == NULL)
      {
         printf("\n");
         return 0;
      }

      if (strlen(line) == 2 &&
          isalpha((unsigned char)line[0]) &&
          line[1] == '\n')
      {

         *guess = tolower((unsigned char)line[0]);
         return 1;
      }

      printf(">>>Error! Please guess one letter.\n");
   }
}

int main(int argc, char *argv[])
{
   if (argc < 3)
   {
      fprintf(stderr, "usage: hangman_client <ip> <port>\n");
      return 1;
   }
   setvbuf(stdout, NULL, _IONBF, 0);

   int sock = socket(AF_INET, SOCK_STREAM, 0);
   if (sock < 0)
   {
      perror("socket");
      return 1;
   }

   struct sockaddr_in addr;
   memset(&addr, 0, sizeof(addr));

   addr.sin_family = AF_INET;
   addr.sin_port = htons(atoi(argv[2]));

   if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0)
   {
      perror("inet_pton");
      close(sock);
      return 1;
   }

   if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
   {
      perror("connect");
      close(sock);
      return 1;
   }

   char line[100];

   while (1)
   {
      printf(">>>Ready to start game? (y/n): ");

      if (fgets(line, sizeof(line), stdin) == NULL)
      {
         printf("\n");
         close(sock);
         return 0;
      }

      if (line[0] == 'n')
      {
         close(sock);
         return 0;
      }

      if (line[0] == 'y')
      {
         unsigned char start = 0;
         send(sock, &start, 1, 0);
         break;
      }
   }
   while (1)
   {

      unsigned char msg_flag;

      if (recvall(sock, &msg_flag, 1) <= 0)
         break;

      /* Message packet */
      if (msg_flag > 0)
      {

         char msg[256];

         if (recvall(sock, msg, msg_flag) <= 0)
            break;

         msg[msg_flag] = '\0';

         printf(">>>%s\n", msg);

         if (strcmp(msg, "Game Over!") == 0)
            break;
      }

      /* Game control packet */
      else
      {

         unsigned char word_length;
         unsigned char num_incorrect;

         if (recvall(sock, &word_length, 1) <= 0)
            break;

         if (recvall(sock, &num_incorrect, 1) <= 0)
            break;

         char word[32];
         char incorrect[32];

         if (recvall(sock, word, word_length) <= 0)
            break;

         if (num_incorrect > 0)
         {
            if (recvall(sock, incorrect, num_incorrect) <= 0)
               break;
         }

         printf(">>>");

         for (int i = 0; i < word_length; i++)
         {
            printf("%c", word[i]);

            if (i != word_length - 1)
               printf(" ");
         }

         printf("\n");

         printf(">>>Incorrect Guesses: ");

         for (int i = 0; i < num_incorrect; i++)
            printf("%c", incorrect[i]);

         printf("\n>>>\n");

         char guess;

         if (!getGuess(&guess))
            break;

         unsigned char len = 1;

         send(sock, &len, 1, 0);
         send(sock, &guess, 1, 0);
      }
   }

   close(sock);

   return 0;
}
