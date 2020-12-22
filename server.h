/* server.h
*AUTHOR: Jhi Morris (19173632)
*MODIFIED: 19/10/2020
*PURPOSE: Header for server.c
*/

#include "common.h"
#include "linkedlist.h"
#include <time.h>
#include <poll.h>
#include <errno.h>
#include <sys/eventfd.h>

#define SERVER_PORT 52000
#define MAX_BACKLOG 10

typedef struct ConnectionData
{
  long timeout;
  LinkedList ownedClients; //EstablishedConnection* type
  LinkedList newClientQueue; //int* (sock descriptor) type
  pthread_cond_t newClientSignal;
  int exit;
} ConnectionData;

typedef struct EstablishedConnection
{
  int hasJoined;
  time_t timeJoined;
  char username[11];
  char realName[21];
  char nickname[11];
  char hostname[256]; //stores ipv4 ip if no hostname found
  LinkedList messageQueue; //Message* type
  int msgEventFd; //only the owner thread will read from this
  pthread_mutex_t mutex;
} EstablishedConnection;

EstablishedConnection* findName(LinkedList *list, const char *name, int length);

void broadcastMessage(Message bcast, LinkedList *connList);

void server(const unsigned int threads, const unsigned int threadIncrement, const unsigned int timeout, int sock);

void* clientThread(void* connectionData);

void joinc(Message *msgIn, Message *msgOut, LinkedList *connList, EstablishedConnection *con);

void nickc(Message *msgIn, Message *msgOut, LinkedList *connList, EstablishedConnection *con);

void whoc(Message *msgOut, LinkedList *connList);

void whoisc(Message *msgIn, Message *msgOut, LinkedList *connList);

void timec(Message *msgOut);

void privmsgc(Message *msgIn, Message *msgOut, LinkedList *connList, EstablishedConnection *con);

void bcastmsgc(Message *msgIn, Message *msgOut, LinkedList *connList, EstablishedConnection *con);

void quitc(Message *msgIn, Message *msgOut, LinkedList *connList, EstablishedConnection *con);
