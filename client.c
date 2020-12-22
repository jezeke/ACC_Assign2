/* client.c
*AUTHOR: Jhi Morris (19173632)
*MODIFIED: 24/10/2020
*PURPOSE: Handles client user interface and communication
*/

#include "client.h"

/* main
*PURPOSE: Reads in and validates the connection parameters from the command line
*  arguments, then attempts to connect to the server using them.
*INPUT: argv[1] IPv4 address or hostname, argv[2] port.
*OUTPUTS: -
*/
int main(int argc, char *argv[])
{
  int error = false;
  int sock;
  struct sockaddr_in ip;
  struct hostent *hostname;
  long portTest;

  if(argc != 3)
  {
    printf("Invalid number of arguments.\n");
    error = true;
  }
  else
  {
    portTest = strtol(argv[2], NULL, 10); //used to check for negative or out of range values
    ip.sin_port = htons(strtol(argv[2], NULL, 10));
    hostname = gethostbyname(argv[1]);
  }

  if(!error && hostname != NULL)
  {
    ip.sin_addr = *((struct in_addr*)hostname->h_addr_list[0]);
    ip.sin_family = hostname->h_addrtype;
  }
  else
  { //not routable hostname
    error = true;
    if(!error && inet_pton(AF_INET, argv[1], &ip.sin_addr) > 0)
    { //check ip validity and type
      ip.sin_family = AF_INET;
    }
    else
    { //not valid IPv4 address
      printf("First argument is not a valid hostname or IP address. IP addresses must be in IPv4 dot-decimal notation.\n");
    }
  }

  if(!error && (sock = socket(ip.sin_family, SOCK_STREAM, 0)) < 0)
  { //check able to make socket for ip type
    printf("Socket error. Check network connectivity of this machine.\n");
    error = true;
  }

  if(!error && (portTest < 1 || portTest > 65535))
  { //check port validity
    printf("Second argument (port) must be an integer between 1 and 65535, inclusive.\n");
    error = true;
  }

  if(!error && connect(sock, (struct sockaddr*)&ip, sizeof(ip)) < 0)
  {
    printf("Connection error. Check network connectivity of this machine and the remote server.\n");
    error = true;
  }

  if(!error)
  {
    signal(SIGPIPE, SIG_IGN); //failed socket operations are handled as they occur

    RecieveThread recvData;
    recvData.sock = sock;
    recvData.error = false;

    pthread_t recvThread;
    pthread_create(&recvThread, NULL, recieveThread, &recvData);
    client(sock, &recvData.error);

    pthread_cancel(recvThread);
  }
  else
  {
    printf("Expected usage: './client ip port', where ip is the hostname or "\
    "IP address of the server, (IP addresses must be in IPv4 dot-decimal notation, "\
    "and port is the network port that the server is running at.\n"\
    "Example: './client 192.168.1.234 1234' to connect to a "\
    "server running at 192.168.1.234 on port 1234\n"\
    "Example: './client localhost 52001' to connect to a server running on "\
    "the same machine as the client, on port 52001.\n");
  }

  return !error;
}

/* client
*PURPOSE: Handles user input for server communication
*INPUT: int sock descriptor, int* recieve error flag
*OUTPUTS: -
*/
void client(int sock, int *recvError)
{
  Message msg;
  msg.body = NULL;
  int exit = false;
  int valid;
  char input[MAXMSGLENGTH]; //space for command or max-length message

  while(!exit && !(*recvError))
  {
    valid = true;
    printf("USER>");
    if(scanf("%"MAXMSGLENGTHSTR"s", input) == 1) //handle command
    {
      for(int i = 0; i < strlen(input); i++)
      { //convert to lower case
        if(input[i] >= 'A' && input[i] <= 'Z')
        {
          input[i] += 'a' - 'A';
        }
      }

      int i = 0;
      int found = false;
      while(!found && i < commandsLen)
      { //compare to known command strings
        if(!strcmp(commands[i], input))
        {
          found = true;
        }

        i++; //even once found we increment, as the integers for commands starts at 1
      }

      if(found)
      {
        msg.command = i;

        switch(i)
        {
          case JOIN: //two arguments
          case PRIVMSG: //falls through
          if(scanf("%"MAXMSGLENGTHSTR"s", input) == 1)
          {
            char remainder[MAXMSGLENGTH];
            scanf(" "); //clear leading whitespace
            if(fgets(remainder, MAXMSGLENGTH, stdin) != NULL)
            {
              remainder[strlen(remainder)-1] = '\0'; //remove trailing newline from fgets

              msg.length = strlen(input) + strlen(remainder) + 1; //+1 for '\2'
              msg.body = calloc(msg.length + 1, sizeof(char)); //+1 for '\0'
              strcpy(msg.body, input);
              msg.body[strlen(input)] = '\2'; //delimits first and second arg
              strcpy(msg.body+strlen(input)+1, remainder);

              valid = true;
            }
            else
            { //no second argument to read
              valid = false;
              printf("LOCAL Error: %s requires more information.\n", commands[i-1]);
            }
          }
          else
          { //no first argument to read
            valid = false;
            printf("LOCAL Error: %s requires an argument.\n", commands[i-1]);
          }

          break;
          case NICK: //single argument
          case WHOIS: //falls through
          case BCASTMSG:
          case QUIT:
          if(fgets(input, MAXMSGLENGTH, stdin) != NULL)
          {
            input[strlen(input)-1] = '\0'; //remove trailing newline from fgets

            msg.length = strlen(input);
            msg.body = calloc(msg.length, sizeof(char)); //+1 for '\0', -1 to skip leading space
            strcpy(msg.body, input+1); //+1 to skip leading space

            valid = true;
          }
          else
          { //no argument to read
            valid = false;
            printf("LOCAL Error: %s requires more information.\n", commands[i-1]);
          }

          break;
          case WHO: //no arguments
          case TIME: //falls through
          msg.length = 0;
          if(msg.body != NULL)
          {
            free(msg.body);
            msg.body = NULL;
          }
          break;
        }
      }
      else
      { //unknown command
        valid = false;
        printf("LOCAL Error: Unknown command.\n");
      }
    }
    else
    { //no input
      valid = false;
      printf("LOCAL Error: Unknown command.\n");
    }

    fflush(stdin);

    if(valid && !(*recvError))
    {
      if(!sendMessage(msg, sock))
      { //if failed to send
        exit = true;
      }

      free(msg.body);
      msg.body = NULL;
    }
  }

  if(*recvError)
  {
    printf("NETWORK Info: Connection closed...\n");
  }
}

/* recieveThread
*PURPOSE: Parses and prints messages from the server
*INPUT: RecieveThread* thread data
*OUTPUTS: -
*/
void* recieveThread(void *recieveThread)
{
  RecieveThread *recv = (RecieveThread*)recieveThread;

  while(!(recv->error))
  {
    Message msg;

    if(recieveMessage(&msg, recv->sock))
    {
      uint64_t delimLen = findDelim(msg.body, msg.length);

      switch(msg.command)
      {
        case RECVPRIV:
        if(delimLen > 0)
        {
          printf("PM from %.*s: %.*s\n", delimLen, msg.body, msg.length-delimLen, msg.body+delimLen);
        }
        else
        { //lacking second operand
          recv->error = true;
          printf("LOCAL: Recieved invalid server command, closing...\n");
        }
        break;
        case RECVBCAST:
        if(delimLen > 0)
        {
          printf("%.*s: %.*s\n", delimLen, msg.body, msg.length-delimLen, msg.body+delimLen);
        }
        else
        { //lacking second operand
          recv->error = true;
          printf("LOCAL: Recieved invalid server command, closing...\n");
        }
        break;
        case RECVQUIT:
        if(delimLen > 0)
        {
          printf("%*s's last message: %*s\n", delimLen, msg.body, msg.length - delimLen, msg.body+delimLen);
        }
        else
        { //no quit message
          printf("There is no last message from %s!\n", msg.body);
        }
        break;
        case RECVTERM: //falls through
        recv->error = true; //time to shut down
        case SERVMSG:
        printf("SERVER: %.*s\n", msg.length, msg.body);
        break;
        default:
        //invalid command
        printf("LOCAL: Recieved invalid server command, closing...\n");
        recv->error = true;
        break;
      }

      free(msg.body);
    }
    else
    { //failed to recieve
      recv->error = true;
    }
  }
}
