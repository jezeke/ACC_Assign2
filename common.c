/* common.c
*AUTHOR: Jhi Morris (19173632)
*MODIFIED: 19/10/2020
*PURPOSE: Provides functions used by both the client and server executables
*/

#include "common.h"

const char *const commands[] = {"join", "nick", "who", "whois", "time", "privmsg", "bcastmsg", "quit"};
const size_t commandsLen = sizeof(commands) / sizeof(commands[0]);

/* sendMessage
*PURPOSE: Sends the contents of the Message struct over the socket connection. It returns 'true' if an error occurs.
*INPUT: Message message, int sock descriptor
*OUTPUTS: int error occured (boolean)
*/
int sendMessage(Message msg, int sock)
{
  int error = false;

  if(msg.length == 0)
  {
    msg.length = 1;
    msg.body = calloc(1, sizeof(char));
    msg.body[0] = '\0';
  }

  if(send(sock, &(msg.command), sizeof(msg.command), 0))
  {
    if(send(sock, &(msg.length), sizeof(msg.length), 0))
    {
      if(!send(sock, msg.body, sizeof(char) * msg.length, 0))
      { //failed to send body
        error = true;
      }
    }
    else
    { //failed to send length
      error = true;
    }
  }
  else
  { //failed to send command
    error = true;
  }

  return !error;
}

/* recieveMessage
*PURPOSE: Recieves the contents of a message from the socket connection and writes it into the Message struct at the pointer passed into it. It returns 'true' if an error occurs.
*INPUT: int sock descriptor
*OUTPUTS: int error occured (boolean), Message msg
*/
int recieveMessage(Message *msg, int sock)
{
  int error = false;

  if(recv(sock, &(msg->command), sizeof(msg->command), MSG_WAITALL) == sizeof(msg->command))
  {
    if(recv(sock, &(msg->length), sizeof(msg->length), MSG_WAITALL) == sizeof(msg->length))
    {
      msg->body = calloc(msg->length + 1, sizeof(char));

      if(recv(sock, msg->body, sizeof(char) * msg->length, MSG_WAITALL) == sizeof(char) * msg->length)
      {
        msg->body[msg->length] = '\0'; //ensure null termination
        error = false;
      }
      else
      { //failed to get full body
        error = true;
      }
    }
    else
    { //failed to get length info
      error = true;
    }
  }
  else
  { //failed to get command info or connection closed
    error = true;
  }

  return !error;
}

/* findDelim
*PURPOSE: Searches the string, up to the max length, for '\2' and returns its offset. Returns -1 if not found.
*INPUT: char* string to be searched, uint64_t max search width
*OUTPUTS: uint64_t delimiter offset
*/
uint64_t findDelim(char *str, uint64_t maxLen)
{
  uint64_t ret = 0;

  while(ret <= maxLen && str[ret] != '\2')
  {
    ret++;
  }

  if(ret == maxLen)
  {
    ret = -1;
  }

  return ret;
}
