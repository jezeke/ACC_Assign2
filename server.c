/* server.c
*AUTHOR: Jhi Morris (19173632)
*MODIFIED: 24/10/2020
*PURPOSE: Handles server communication and processing
*/

#include "server.h"

/* main
*PURPOSE: Reads in and validates the server parameters from the command line
*  arguments and binds to a socket for server().
*INPUT: argv[1] initial threads, argv[2] thread increment, argv[3] seconds of idle
*  before connection timeout.
*OUTPUTS: -
*/
int main(int argc, char *argv[])
{
  int error = false;
  int sock = -1;
  long port;
  long threads, threadIncrement, timeout;

  if(argc != 4)
  {
    printf("Invalid number of arguments.\n");
    error = true;
  }

  if(!error)
  {
    threads = strtol(argv[1], NULL, 10);

    if(!error && threads < 1)
    {
      printf("First argument (number of initial threads) must be an integer greater than zero.\n");
      error = true;
    }

    threadIncrement = strtol(argv[2], NULL, 10);

    if(!error && (threadIncrement < 1))
    {
      printf("Second argument (number of threads per increment) must be a positive integer.\n");
      error = true;
    }

    timeout = strtol(argv[3], NULL, 10);

    if(!error && (timeout < 0 || timeout > 120))
    {
      printf("Third argument (timeout time) must be a positive integer.\n");
      error = true;
    }
  }

  port = SERVER_PORT;
  //port = strtol(argv[4], NULL, 10);

  if(!error && (port < 1 || port > 65535))
  {
    printf("Forth argument (port) must be an integer between 1 and 65535, inclusive.\n");
    error = true;
  }

  if(!error && (sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    printf("Socket error.\n");
    error = true;
  }

  int val = 1;
  struct sockaddr_in ip;
  if(!error && setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val)) < 0)
  {
    printf("Sockopt error.\n");
    error = true;
  }
  else
  {
    //bind to ipv4
    memset(&ip, 0, sizeof(ip));
    ip.sin_family = AF_INET;
    ip.sin_port = htons(port);
    ip.sin_addr.s_addr = INADDR_ANY;
  }

  if(!error && bind(sock, (struct sockaddr*)&ip, sizeof(ip)) < 0)
  {
    printf("Failed to bind to the port. Is there already a service running at this port?\n");
    error = true;
  }

  if(!error && listen(sock, MAX_BACKLOG))
  {
    printf("Failed to listen on port.\n"); //no clue how it might reach this condition
    error = true;
  }

  if(!error)
  {
    printf("Starting server. . .\n");
    server(threads, threadIncrement, timeout, sock);
    printf("Server shutting down. . .\n");
  }
  else
  {
    printf("Expected usage: './server n m wait_time', where n is the initial "\
    "number of threads for handling client connections, m is the number of "\
    "threads added to the threadpool when it reaches capacity, and wait_time "\
    "is the number of seconds an idle client will remain before being timed out.\n"\
    "Example: './server 10 4 120' to start the server with ten threads that will "\
    "gain four additional threads if nine clients are connected concurrently; "\
    "each of which will be disconnected if they do not send a command for more than 120 seconds.\n");
  }

  if(sock != -1)
  {
    close(sock);
  }

  return !error;
}

/* server
*PURPOSE: Manages the threadpool and handles incoming connections to the socket.
*INPUT: unsigned int initial threads, unsigned int thread increment, unsigned int idle time limit, int socket descriptor
*OUTPUTS: -
*/
void server(const unsigned int threads, const unsigned int threadIncrement, const unsigned int timeout, int sock)
{
  ConnectionData *cData = calloc(1, sizeof(ConnectionData));
  cData->timeout = timeout;
  cData->exit = false;
  cData->ownedClients.head = NULL;
  cData->newClientQueue.head = NULL;
  pthread_mutex_init(&(cData->ownedClients.mutex), NULL);
  pthread_mutex_init(&(cData->newClientQueue.mutex), NULL);
  pthread_cond_init(&(cData->newClientSignal), NULL);

  LinkedList threadList; //pthread_t* type
  threadList.head = NULL;

  for(int i = 0; i < threads; i++)
  { //create initial threadpool
    pthread_t *thread = calloc(1, sizeof(pthread_t));
    pthread_create(thread, NULL, clientThread, (void*)cData);
    enqueue(thread, &threadList);
  }

  while(!(cData->exit))
  {
    int *sd = calloc(1, sizeof(int));

    if((*sd = accept(sock, NULL, NULL)) < 0)
    {
      printf("Connection error.\n");
      cData->exit = true;
    }
    else
    {
      pthread_mutex_lock(&(cData->newClientQueue.mutex));

      printf("SERVER Info: New connection.\n");
      enqueue(sd, &(cData->newClientQueue));

      pthread_mutex_lock(&(cData->ownedClients.mutex));

      if(length(&(threadList)) - length(&(cData->ownedClients)) - length(&(cData->newClientQueue)) <= 1)
      { //if idle threads minus available jobs is 1 or less
        printf("SERVER Info: Adding new threads to pool.\n");
        for(int i = 0; i < threadIncrement; i++)
        {
          pthread_t *thread = calloc(1, sizeof(pthread_t));
          pthread_create(thread, NULL, clientThread, (void*)cData);
          enqueue(thread, &threadList);
        }
      }

      pthread_cond_signal(&(cData->newClientSignal));
      pthread_mutex_unlock(&(cData->newClientQueue.mutex));
      pthread_mutex_unlock(&(cData->ownedClients.mutex));
    }
  }
}

/* clientThread
*PURPOSE: Thread function. Handles a connection with a client; sending and recieving messages to/from it
*INPUT: ConnectionData* connection data
*OUTPUTS: -
*/
void* clientThread(void* connectionData)
{
  ConnectionData *cData = (ConnectionData*)connectionData;

  while(!(cData->exit))
  {
    int *client = NULL;
    while(client == NULL)
    { //reduces thundering herd
      pthread_mutex_lock(&(cData->newClientQueue.mutex));

      if(cData->newClientQueue.head == NULL)
      { //ensure there is a new client
        pthread_cond_wait(&(cData->newClientSignal), &(cData->newClientQueue.mutex));
      }

      client = dequeue(&(cData->newClientQueue));

      pthread_mutex_unlock(&(cData->newClientQueue.mutex));
    }

    EstablishedConnection *estCon = calloc(1, sizeof(EstablishedConnection));
    estCon->messageQueue.head = NULL;
    estCon->timeJoined = time(NULL);
    estCon->hasJoined = false;
    pthread_mutex_init(&(estCon->messageQueue.mutex), NULL);
    estCon->msgEventFd = eventfd(0, 0);

    struct sockaddr_in addr;
    int len = sizeof(struct sockaddr_in);
    getsockname(*client, (struct sockaddr*)&addr, &len);
    if(!getnameinfo((struct sockaddr*)&addr, len, estCon->hostname, 256, NULL, 0, 0))
    { //failed to resolve ip to hostname
      char *tmp = inet_ntoa(addr.sin_addr);
      strncpy(estCon->hostname, tmp, 16);
    }

    struct pollfd polld[2];
    polld[0].events = POLLIN;
    polld[0].revents = 0;
    polld[0].fd = estCon->msgEventFd;
    polld[1].events = POLLIN;
    polld[1].revents = 0;
    polld[1].fd = *client;
    free(client);
    pthread_mutex_lock(&(cData->ownedClients.mutex));

    enqueue(estCon, &(cData->ownedClients));

    pthread_mutex_unlock(&(cData->ownedClients.mutex));

    int quit = false;
    int lastMsgTime = time(NULL);
    while(!quit)
    {
      int remTime = lastMsgTime - time(NULL) + cData->timeout;
      switch(poll(polld, 2, remTime*1000))
      {
        case -1: //error
        printf("NETWORK Error: Failed to poll connection.\n");
        quit = true;
        break;
        case 0: //timeout
        printf("NETWORK Info: Connection timed out.\n");
        quit = true;
        break;
        default: //data or message available or connection died
        if(polld[0].revents > 0)
        { //messages to be sent
          eventfd_t buf;
          polld[0].revents = 0;
          eventfd_read(estCon->msgEventFd, &buf); //clear eventfd

          pthread_mutex_lock(&(estCon->messageQueue.mutex));

          while(estCon->messageQueue.head != NULL && !quit)
          {
            Message *msgOut = dequeue(&(estCon->messageQueue));

            if(!sendMessage(*msgOut, polld[1].fd))
            { //failed to send message
              quit = true;
              printf("NETWORK Error: Failed to send message.\n");
            }

            free(msgOut->body);
            free(msgOut);
          }

          pthread_mutex_unlock(&(estCon->messageQueue.mutex));
        }

        if(polld[1].revents > 0)
        { //message to be recieved
          Message msgIn;
          polld[1].revents = 0;
          if(recieveMessage(&msgIn, polld[1].fd))
          {
            lastMsgTime = time(NULL);
            Message *msgOut;
            msgOut = calloc(1, sizeof(Message));

            if(!estCon->hasJoined)
            {
              if(msgIn.command == JOIN)
              {
                joinc(&msgIn, msgOut, &(cData->ownedClients), estCon);
              }
              else
              {
                if(msgIn.command == QUIT)
                {
                  msgOut->command = RECVTERM;
                  char msgText[512];
                  snprintf(msgText, sizeof(msgText), "You have been connected without login for %ld seconds. Bye.", time(NULL) - estCon->timeJoined);
                  msgOut->length = strnlen(msgText, sizeof(msgText));
                  msgOut->body = calloc(msgOut->length, sizeof(char));
                  strncpy(msgOut->body, msgText, msgOut->length);
                }
                else
                { //invalid command until JOINed
                  msgOut->command = SERVMSG;
                  char errorMsg[] = "Info: You must JOIN before you can do that.";
                  msgOut->length = sizeof(errorMsg);
                  msgOut->body = calloc(1, sizeof(errorMsg));
                  memcpy(msgOut->body, errorMsg, sizeof(errorMsg));
                }
              }
            }
            else
            {
              switch(msgIn.command)
              { //note QUIT and JOIN handled seperately above
                case NICK: nickc(&msgIn, msgOut, &(cData->ownedClients), estCon);
                break;
                case WHO: whoc(msgOut, &(cData->ownedClients));
                break;
                case WHOIS: whoisc(&msgIn, msgOut, &(cData->ownedClients));
                break;
                case TIME: timec(msgOut);
                break;
                case PRIVMSG: privmsgc(&msgIn, msgOut, &(cData->ownedClients), estCon);
                break;
                case BCASTMSG: bcastmsgc(&msgIn, msgOut, &(cData->ownedClients), estCon);
                break;
                case QUIT: quitc(&msgIn, msgOut, &(cData->ownedClients), estCon);
                break;
                default: //invalid command
                quit = true;
                printf("NETWORK Info: Invalid connection dropped.\n");
                break;
              }
            }

            if(!quit)
            {
              if(msgOut->command != INVALID)
              {
                pthread_mutex_lock(&(estCon->mutex));
                pthread_mutex_lock(&(estCon->messageQueue.mutex));

                enqueue(msgOut, &(estCon->messageQueue));

                pthread_mutex_unlock(&(estCon->messageQueue.mutex));

                eventfd_write(estCon->msgEventFd, 1);

                pthread_mutex_unlock(&(estCon->mutex));
              }
              else
              {
                free(msgOut);
              }

              if(msgIn.command == QUIT)
              { //wrap-up
                pthread_mutex_lock(&(estCon->messageQueue.mutex));

                //send remaining messages
                while(estCon->messageQueue.head != NULL && !quit)
                {
                  Message *msgOut = dequeue(&(estCon->messageQueue));

                  if(!sendMessage(*msgOut, polld[1].fd))
                  { //failed to send message
                    quit = true;
                    printf("NETWORK Error: Failed to send message to quitting user.\n");
                  }

                  free(msgOut->body);
                  free(msgOut);
                }

                pthread_mutex_unlock(&(estCon->messageQueue.mutex));
                quit = true;
              }
            }
            else
            {
              free(msgOut);
            }
          }
          else
          { //failed to recieve valid message or connection died
            quit = true;
            printf("NETWORK Info: Invalid connection dropped.\n");
          }
        }
        break;
      }

      if(quit)
      {
        if(polld[1].fd != -1)
        {
          close(polld[1].fd);
        }
      }
    }

    pthread_mutex_lock(&(cData->ownedClients.mutex));

    //clean up messageQueue
    while(estCon->messageQueue.head != NULL)
    {
      free(dequeue(&(estCon->messageQueue)));
    }

    pthread_mutex_destroy(&(estCon->messageQueue.mutex));

    //remove estcon from ownedClients list
    LinkedListNode *node = cData->ownedClients.head;

    if(node != NULL && node->data == estCon)
    {
      cData->ownedClients.head = node->next;
    }
    else
    { //not first element
      int found = false;

      if(node != NULL)
      { //may be null if list is empty. should never happen
        while(node->next != NULL && !found)
        {
          if(node->next->data == estCon)
          {
            found = true;
            node->next = node->next->next;
          }
          else
          {
            node = node->next;
          }
        }
      }

      if(!found)
      { //connection not found. should never happen
        printf("LOCAL: Critical error, failed to free connection memory.\n");
        cData->exit = true;
      }
    }

    //now no other thread can find estcon, ensure none are using it
    pthread_mutex_lock(&(estCon->mutex));
    pthread_mutex_unlock(&(estCon->mutex));
    pthread_mutex_destroy(&(estCon->mutex));

    free(estCon);

    pthread_mutex_unlock(&(cData->ownedClients.mutex));
  }

  return NULL;
}

/* findName
*PURPOSE: Finds the client with the nickname matching the input name and returns a pointer to it.
*INPUT: LinkedList* list of EstablishConnection*s, string name to find, int length of name
*OUTPUTS: EstablishedConnection* found name. Returns NULL if not found
*NOTES: Assumes mutex lock on list already attained.
*/
EstablishedConnection* findName(LinkedList *list, const char *name, int length)
{
  int found = false;
  LinkedListNode *node = list->head;

  while(node != NULL && !found)
  {
    pthread_mutex_lock(&(((EstablishedConnection*)node->data)->mutex));

    if(!strncmp(name, ((EstablishedConnection*)node->data)->nickname, length))
    {
      found = true;
    }

    pthread_mutex_unlock(&(((EstablishedConnection*)node->data)->mutex));

    if(!found)
    {
      node = node->next;
    }
  }

  return found ? (EstablishedConnection*)(node->data) : NULL; //returns NULL if not found
}

/* broadcastMessage
*PURPOSE: Clones and appends enqueues message to each client in the list.
*INPUT: Message to be broadcasted, LinkedList* list of EstablishConnection*s
*OUTPUTS: -
*NOTES: Assumes mutex lock on client list already attained.
*/
void broadcastMessage(Message bcast, LinkedList *connList)
{
  LinkedListNode *node = connList->head;

  while(node != NULL)
  {
    EstablishedConnection *target = (EstablishedConnection*)node->data;

    Message *clonedBcast = calloc(1, sizeof(Message));
    clonedBcast->command = bcast.command;
    clonedBcast->length = bcast.length;
    clonedBcast->body = calloc(bcast.length, sizeof(char));
    memcpy(clonedBcast->body, bcast.body, bcast.length*sizeof(char));

    pthread_mutex_lock(&(target->mutex));
    pthread_mutex_lock(&(target->messageQueue.mutex));

    enqueue(clonedBcast, &(target->messageQueue));

    pthread_mutex_unlock(&(target->messageQueue.mutex));

    eventfd_write(target->msgEventFd, 1);

    pthread_mutex_unlock(&(target->mutex));

    node = node->next;
  }
}

/*
* Below (until the end of the file) are the functions which handle client
* commands. All of them take pointers for an input message and an output
* message. Some also take a pointer to the connection list and the current connection.
* All must write to msgOut, though if it is not required for msgOut to be sent
* then the command field should be set to INVALID.
*/


/* joinc
*PURPOSE: Implements parsing and response to the JOIN command.
*INPUT: Message input message,  LinkedList* list of EstablishConnection*s, EstablishedConnection* client.
*OUTPUTS: Message* direct response
*/
void joinc(Message *msgIn, Message *msgOut, LinkedList *connList, EstablishedConnection *con)
{
  uint64_t offset = findDelim(msgIn->body, msgIn->length);

  if(offset > 0 && offset < sizeof(con->username))
  {
    if(offset != msgIn->length && msgIn->length - offset < sizeof(con->realName))
    {
      pthread_mutex_lock(&(connList->mutex));

      if(findName(connList, msgIn->body, offset) == NULL)
      { //if no duplicate name
        pthread_mutex_unlock(&(connList->mutex));
        pthread_mutex_lock(&(con->mutex));

        strncpy(con->username, msgIn->body, offset);
        strncpy(con->nickname, msgIn->body, offset);
        strncpy(con->realName, msgIn->body+offset+1, msgIn->length-offset);


        con->hasJoined = true;

        Message bcast;
        bcast.command = SERVMSG;
        char msgText[512];
        snprintf(msgText, sizeof(msgText), "JOIN %s - %s - %s", con->nickname, con->realName, con->hostname);
        bcast.length = strnlen(msgText, sizeof(msgText));
        bcast.body = calloc(bcast.length, sizeof(char));
        strncpy(bcast.body, msgText, bcast.length);

        pthread_mutex_unlock(&(con->mutex));
        pthread_mutex_lock(&(connList->mutex));

        broadcastMessage(bcast, connList);

        pthread_mutex_unlock(&(connList->mutex));

        msgOut->command = INVALID; //direct message back not required
      }
      else
      { //nickname or username already taken
        pthread_mutex_unlock(&(connList->mutex));
        msgOut->command = SERVMSG;
        char errorMsg[] = "Info: That name is already taken, please try again.";
        msgOut->length = sizeof(errorMsg);
        msgOut->body = calloc(1, sizeof(errorMsg));
        memcpy(msgOut->body, errorMsg, sizeof(errorMsg));
      }
    }
    else
    { //invalid real name length
      msgOut->command = SERVMSG;
      char errorMsg[] = "Error: Real name must be between 1 and 20 letters long.";
      msgOut->length = sizeof(errorMsg);
      msgOut->body = calloc(1, sizeof(errorMsg));
      memcpy(msgOut->body, errorMsg, sizeof(errorMsg));
    }
  }
  else
  { //invalid username length
    msgOut->command = SERVMSG;
    char errorMsg[] = "Error: Username must be between 1 and 10 letters long.";
    msgOut->length = sizeof(errorMsg);
    msgOut->body = calloc(1, sizeof(errorMsg));
    memcpy(msgOut->body, errorMsg, sizeof(errorMsg));
  }
}

/* nickc
*PURPOSE: Implements parsing and response to the NICK command.
*INPUT: Message input message,  LinkedList* list of EstablishConnection*s, EstablishedConnection* client.
*OUTPUTS: Message* direct response
*/
void nickc(Message *msgIn, Message *msgOut, LinkedList *connList, EstablishedConnection *con)
{
  if(msgIn->length > 0 && msgIn->length < 10)
  {
    pthread_mutex_lock(&(connList->mutex));

    if(findName(connList, msgIn->body, msgIn->length) == NULL)
    {
      pthread_mutex_unlock(&(connList->mutex));
      pthread_mutex_lock(&(con->mutex));

      strncpy(con->nickname, msgIn->body, msgIn->length);

      pthread_mutex_unlock(&(con->mutex));

      msgOut->command = SERVMSG;
      char msgText[512];
      snprintf(msgText, sizeof(msgText), "Your new nickname is %s.", con->nickname);
      msgOut->length = strnlen(msgText, sizeof(msgText));
      msgOut->body = calloc(msgOut->length, sizeof(char));
      strncpy(msgOut->body, msgText, msgOut->length);
    }
    else
    { //nickname or username already taken
      pthread_mutex_unlock(&(connList->mutex));
      msgOut->command = SERVMSG;
      char errorMsg[] = "Info: That name is already taken, please try again.";
      msgOut->length = sizeof(errorMsg);
      msgOut->body = calloc(1, sizeof(errorMsg));
      memcpy(msgOut->body, errorMsg, sizeof(errorMsg));
    }
  }
  else
  { //invalid nickname length
    msgOut->command = SERVMSG;
    char errorMsg[] = "Error: Nickname must be between 1 and 10 letters long.";
    msgOut->length = sizeof(errorMsg);
    msgOut->body = calloc(1, sizeof(errorMsg));
    memcpy(msgOut->body, errorMsg, sizeof(errorMsg));
  }
}

/* whoc
*PURPOSE: Implements response to the WHO command.
*INPUT: LinkedList* list of EstablishConnection*s
*OUTPUTS: Message* direct response
*/
void whoc(Message *msgOut, LinkedList *connList)
{
  pthread_mutex_lock(&(connList->mutex));

  LinkedListNode *node = connList->head;

  msgOut->command = SERVMSG;

  while(node != NULL)
  { //check body length required
    EstablishedConnection *estCon = (EstablishedConnection*)(node->data);
    if(estCon->hasJoined)
    {
      char buff[512]; //space for username, hostname, time, etc.
      snprintf(buff, sizeof(buff), "%s %s %s %s %lds\n", estCon->username, estCon->realName, estCon->nickname, estCon->hostname, time(NULL) - estCon->timeJoined);

      msgOut->length += strlen(buff);
    }

    node = node->next;
  }

  msgOut->body = calloc(msgOut->length, sizeof(char));
  unsigned long offset = 0;
  node = connList->head;

  while(node != NULL)
  { //fill body
    EstablishedConnection *estCon = (EstablishedConnection*)(node->data);
    if(estCon->hasJoined)
    {
      char buff[512]; //space for username, hostname, time, etc.
      snprintf(buff, sizeof(buff), "%s %s %s %s %lds\n", estCon->username, estCon->realName, estCon->nickname, estCon->hostname, time(NULL) - estCon->timeJoined);

      strncpy(msgOut->body + offset, buff, strnlen(buff, sizeof(buff)) - 1);
      offset += strnlen(buff, sizeof(buff)) - 1;
    }

    node = node->next;
  }

  pthread_mutex_unlock(&(connList->mutex));
}

/* whoisc
*PURPOSE: Implements parsing and response to the WHOIS command.
*INPUT: Message input message,  LinkedList* list of EstablishConnection*s
*OUTPUTS: Message* direct response
*/
void whoisc(Message *msgIn, Message *msgOut, LinkedList *connList)
{
  EstablishedConnection *user;
  if((user = findName(connList, msgIn->body, msgIn->length)) != NULL)
  {
    pthread_mutex_lock(&(user->mutex));

    char buff[512]; //space for realname & hostname, etc.
    snprintf(buff, sizeof(buff), "%s - %s", user->realName, user->hostname);

    pthread_mutex_unlock(&(user->mutex));

    msgOut->command = SERVMSG;
    msgOut->length = strnlen(buff, sizeof(buff));
    msgOut->body = calloc(msgOut->length, sizeof(char));
    strncpy(msgOut->body, buff, msgOut->length);
  }
  else
  { //user not found
    msgOut->command = SERVMSG;
    char errorMsg[] = "Info: Matching user not found.";
    msgOut->length = sizeof(errorMsg);
    msgOut->body = calloc(1, sizeof(errorMsg));
    memcpy(msgOut->body, errorMsg, sizeof(errorMsg));
  }
}

/* timec
*PURPOSE: Implements response to the TIME command.
*INPUT: -
*OUTPUTS: Message* direct response
*/
void timec(Message *msgOut)
{
  time_t timenow = time(NULL);
  struct tm *locTime = localtime(&timenow);

  msgOut->command = SERVMSG;
  msgOut->length = 42;
  msgOut->body = calloc(42, sizeof(char)); //space for below message
  strftime(msgOut->body, msgOut->length, "The current server local time is %H:%M:%S.", locTime);
}

void privmsgc(Message *msgIn, Message *msgOut, LinkedList *connList, EstablishedConnection *con)
{
  EstablishedConnection *user;
  uint64_t offset = findDelim(msgIn->body, msgIn->length);
  if((user = findName(connList, msgIn->body, offset)) != NULL)
  {
    Message *privMsg = calloc(1, sizeof(Message));

    privMsg->command = RECVPRIV;
    privMsg->length = strnlen(con->nickname, sizeof(con->nickname)) + msgIn->length - offset + 1; //+1 for '\2'
    privMsg->body = calloc(privMsg->length, sizeof(char));
    strncpy(privMsg->body, con->nickname, strnlen(con->nickname, sizeof(con->nickname)));
    strncpy(privMsg->body+strnlen(con->nickname, sizeof(con->nickname)), msgIn->body + offset, strnlen(msgIn->body + offset, msgIn->length - offset));

    pthread_mutex_lock(&(user->mutex));
    pthread_mutex_lock(&(user->messageQueue.mutex));

    enqueue(privMsg, &(user->messageQueue));

    pthread_mutex_unlock(&(user->messageQueue.mutex));

    eventfd_write(user->msgEventFd, 1);

    pthread_mutex_unlock(&(user->mutex));

    msgOut->command = SERVMSG;
    char msgText[] = "PM sent.";
    msgOut->length = sizeof(msgText);
    msgOut->body = calloc(1, sizeof(msgText));
    strncpy(msgOut->body, msgText, sizeof(msgText));
  }
  else
  { //user not found
    msgOut->command = SERVMSG;
    char errorMsg[] = "Info: Matching user not found.";
    msgOut->length = sizeof(errorMsg);
    msgOut->body = calloc(1, sizeof(errorMsg));
    memcpy(msgOut->body, errorMsg, sizeof(errorMsg));
  }
}

/* bcastmsgc
*PURPOSE: Implements parsing and response to the BCASTMSG command.
*INPUT: Message input message, LinkedList* list of EstablishConnection*s, EstablishedConnection* client.
*OUTPUTS: Message* direct response
*/
void bcastmsgc(Message *msgIn, Message *msgOut, LinkedList *connList, EstablishedConnection *con)
{
  Message bcast;
  bcast.command = RECVBCAST;
  bcast.length = msgIn->length + strnlen(con->nickname, sizeof(con->nickname)) + 1; //+1 for space for '\2'
  bcast.body = calloc(bcast.length, sizeof(char));
  strncpy(bcast.body, con->nickname, strnlen(con->nickname, sizeof(con->nickname)));
  bcast.body[strnlen(con->nickname, sizeof(con->nickname))] = '\2'; //delimiter
  strncpy(bcast.body+strnlen(con->nickname, sizeof(con->nickname))+1, msgIn->body, msgIn->length);

  pthread_mutex_lock(&(connList->mutex));

  broadcastMessage(bcast, connList);

  pthread_mutex_unlock(&(connList->mutex));

  msgOut->command = INVALID; //direct message back not required
}

/* quitc
*PURPOSE: Implements parsing and response to the QUIT command.
*INPUT: Message input message,  LinkedList* list of EstablishConnection*s, EstablishedConnection* client.
*OUTPUTS: Message* direct response
*/
void quitc(Message *msgIn, Message *msgOut, LinkedList *connList, EstablishedConnection *con)
{
  Message notice;
  char msgText[512];
  notice.command = SERVMSG;
  snprintf(msgText, sizeof(msgText), "%s is no longer in our chatting session.", con->nickname);
  notice.length = strnlen(msgText, sizeof(msgText));
  notice.body = calloc(notice.length, sizeof(char));
  strncpy(notice.body, msgText, notice.length);

  Message bcast;
  bcast.command = RECVQUIT;
  bcast.length = msgIn->length + strnlen(con->nickname, sizeof(con->nickname)) + 1; //+1 for space for '\2'
  bcast.body = calloc(bcast.length, sizeof(char));
  strncpy(bcast.body, con->nickname, strnlen(con->nickname, sizeof(con->nickname)));
  bcast.body[strnlen(con->nickname, sizeof(con->nickname))] = '\2'; //delimiter
  strncpy(bcast.body+strnlen(con->nickname, sizeof(con->nickname))+1, msgIn->body, msgIn->length);

  pthread_mutex_lock(&(connList->mutex));

  broadcastMessage(bcast, connList);
  broadcastMessage(notice, connList);

  pthread_mutex_unlock(&(connList->mutex));

  msgOut->command = RECVTERM;
  snprintf(msgText, sizeof(msgText), "You have been chatting for %ld seconds. Bye %s!", time(NULL) - con->timeJoined, con->nickname);
  msgOut->length = strnlen(msgText, sizeof(msgText));
  msgOut->body = calloc(msgOut->length, sizeof(char));
  strncpy(msgOut->body, msgText, msgOut->length);
}
