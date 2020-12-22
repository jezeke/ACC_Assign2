/* linkedlist.c
*AUTHOR: Jhi Morris (19173632)
*MODIFIED: 24/10/2020
*PURPOSE: Handles client user interface and communication
*/

#include "linkedlist.h"

/* enqueue
*PURPOSE: Inserts a LinkedListNode to the head of the list containing the pointer to data
*INPUT: void* data, LinkedList* list
*OUTPUTS: -
*NOTES: Assumes mutex lock on list already attained.
*/
void enqueue(void *data, LinkedList *list)
{
  LinkedListNode *node = calloc(1, sizeof(LinkedListNode));
  node->data = data;
  node->next = list->head;
  list->head = node;
}

/* length
*PURPOSE: Iterates through the list to find the number of nodes in it
*INPUT: LinkedList* list
*OUTPUTS: long length
*NOTES: Assumes mutex lock on list already attained.
*/
long length(LinkedList *list)
{
  long length = 0;
  LinkedListNode *node = list->head;

  while(node != NULL)
  {
    length++;
    node = node->next;
  }

  return length;
}

/* enqueue
*PURPOSE: Pops off the LinkedListNode at the head of the list and returns its data
*INPUT: LinkedList* list
*OUTPUTS: void* data
*NOTES: Assumes mutex lock on list already attained.
*/
void* dequeue(LinkedList *list)
{
  LinkedListNode *node = list->head;
  void* data = NULL;

  if(node != NULL)
  {
    list->head = node->next;
    data = node->data;
    free(node);
  }

  return data;
}
