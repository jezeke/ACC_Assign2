/* linkedlist.h
*AUTHOR: Jhi Morris (19173632)
*MODIFIED: 19/10/2020
*PURPOSE: Handles client user interface and communication
*/

#include <stdlib.h>
#include <pthread.h>

typedef struct LinkedListNode
{
  struct LinkedListNode *next;
  void *data;
} LinkedListNode;

typedef struct LinkedList
{
  LinkedListNode *head;
  pthread_mutex_t mutex;
} LinkedList;

void enqueue(void *data, LinkedList *list);

long length(LinkedList *list);

void* dequeue(LinkedList *list);
