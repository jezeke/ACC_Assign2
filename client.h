/* client.h
*AUTHOR: Jhi Morris (19173632)
*MODIFIED: 19/10/2020
*PURPOSE: Header for client.c
*/

#include "common.h"

#define MAXMSGLENGTH 256
#define MAXMSGLENGTHSTR "255"

typedef struct RecieveThread
{
  int sock;
  int error;
} RecieveThread;

void client(int sock, int *recvError);

void* recieveThread(void *recieveThread);
