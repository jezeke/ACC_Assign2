/* common.h
*AUTHOR: Jhi Morris (19173632)
*MODIFIED: 16/10/2020
*PURPOSE: Header for common.c. Provides many structs and defines used by server and client executables.
*/

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <netdb.h>

//server-destined commands:
#define INVALID 0
#define JOIN 1 //username, realname
#define NICK 2 //nickname
#define WHO 3 // -
#define WHOIS 4 //target username
#define TIME 5 // -
#define PRIVMSG 6 //target username, message
#define BCASTMSG 7 //message
#define QUIT 8 //exit message
//client-destined commands:
#define RECVPRIV 9 //source username, message
#define RECVBCAST 10 //source username, message
#define RECVQUIT 11 //source username, message
#define SERVMSG 12 //error or info from server
#define RECVTERM 13 //error or info from server

typedef struct Message {
  uint8_t command;
  uint64_t length;
  char *body;
} Message;

extern const char* const commands[];
extern const size_t commandsLen;

int sendMessage(const Message msg, int sock);

int recieveMessage(Message* msg, int sock);

uint64_t findDelim(char *str, uint64_t maxLen);
