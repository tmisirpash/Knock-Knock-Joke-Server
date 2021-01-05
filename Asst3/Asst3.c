#include<stdlib.h>
#include<stdio.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<string.h>
#include<netdb.h>
#include<netinet/in.h>
#include<unistd.h>
#include<sys/ioctl.h>
#include<ctype.h>
#include<time.h>
#include<signal.h>

//Given a size_t number, returns the number of digits in its decimal representation.
int numDigits(size_t n)
{
  int count = 0;
  while (n != 0)
    {
      n /= 10;
      count++;
    }
  return count;
}
//Given a size_t number, returns its decimal representation as a char *.
char * numToString(size_t n)
{
  char * result = malloc(numDigits(n) + 1);
  sprintf(result, "%zu", n);
  return result;
}
//Given a file pointer to the joke list, returns a char * array containing all of the responses (in REG||| format) and updates a pointer to the size of the list accordingly.
char ** getJokeList(FILE * fp, int * listSizePtr)
{
  char * line = NULL;
  size_t len = 0;
  ssize_t read;
  int size = 0;
  while ((read = getline(&line, &len, fp)) != -1)
    {
      if (strcmp(line, "\n") != 0) size++;
    }
  rewind(fp);
  int i = 0;
  char ** jokelist = (char**)malloc(sizeof(char*) * size);
  while ((read = getline(&line, &len, fp)) != -1)
  {
    if (strcmp(line, "\n") != 0)
        {
	  //Some string manipulation to bring the message to the wanted format.
          int allocationSize = strlen(line) + numDigits(strlen(line)-1) + 6;

          jokelist[i] = malloc(allocationSize);
          jokelist[i][0] = 'R';
          jokelist[i][1] = 'E';
          jokelist[i][2] = 'G';
          jokelist[i][3] = '|';

          char * lengthField = numToString(strlen(line)-1);
          int j;

          for (j = 0; j < strlen(lengthField); j++)
            {
              jokelist[i][4+j] = lengthField[j];
            }

          free(lengthField);

          jokelist[i][4+j] = '|';
          int midPipe = 4+j;

          for (j = 0; j < strlen(line)-1; j++)
            {
              jokelist[i][midPipe + j + 1] = line[j];
            }
          jokelist[i][midPipe + j + 1] = '|';
          jokelist[i][midPipe + j + 2] = '\0';
          i++;
        }
  }
  free(line);
  *listSizePtr = size;
  return jokelist;
}
//Frees the char ** array pointed to by jokelist.
void freeJokeList(char ** jokelist, int listSize)
{
  int i;
  for (i = 0; i < listSize; i++)
    {
      free(jokelist[i]);
    }
  free(jokelist);
}
//Generates a random even number that corresponds to the index of a response in jokelist.
int getRandJoke(char ** jokelist, int listSize)
{
  time_t t;
  srand((unsigned) time(&t));
  return (rand() % ((listSize-2)/2+1))*2;
}
//Given a char *, appends ", who?". Note that the last character of the original string will be replaced by the ','.
char * whoAppend(char * original)
{
  char * result = malloc(strlen(original)+6);
  int i;
  for (i = 0; i < strlen(original)-1; i++)
    {
      result[i] = original[i];
    }
  result[i] = ',';
  result[i+1] = ' ';
  result[i+2] = 'w';
  result[i+3] = 'h';
  result[i+4] = 'o';
  result[i+5] = '?';
  result[i+6] = '\0';
  free(original);                                                                                                  
  return result;
}
//Given a buffer with the read data and the number of bytes of successfully read, returns 1 if an error was recorded and 0 otherwise. The parameter messageNo is used to print informative error messages.
int errorCheck(char * buffer, int n, int messageNo)
{
  //ERR|Mx__|, where x = messageNo.
  if (n == 9 && buffer[0] == 'E' && buffer[1] == 'R'&& buffer[2] == 'R'
      && buffer[3] == '|' && buffer[4] == 'M' && buffer[5] == (messageNo+'0') && buffer[8] == '|')
    {
      //M_CT
      if (buffer[6] == 'C' && buffer[7] == 'T')
	{
	  printf("ERROR M%dCT: Message %d content was not correct.\n", messageNo, messageNo);
	  return 1;
	}
      //M_LN
      else if (buffer[6] == 'L' && buffer[7] == 'N')
	{
	  printf("ERROR M%dLN: Message %d length was not correct.\n", messageNo, messageNo);
	  return 1;
	}
      //M_FT
      else if (buffer[6] == 'F' && buffer[7] == 'T')
	{
	  printf("ERROR M%dFT: Message %d format was broken.\n", messageNo, messageNo);
	  return 1;
	}
    }
  //Server did not receive an error message from the client.
  return 0;
}
//Checks the basic format of a REG message. (It must be in the form "REG|x|y|".) Returns 1 if the message is in incorrect form and 0 otherwise.
int formatCheck(char * buffer, int n, int * pipeIndexPtr)
{
  if (n < 6 || buffer[0] != 'R' || buffer[1] != 'E' ||
      buffer[2] != 'G' || buffer[3] != '|' || buffer[n-1] != '|')
    {
      return 1;
    }
  int i;
  int pipeCount = 0;
  //There must be exactly one '|' character between the other two.
  for (i = 4; i < n-1; i++)
    {
      if (buffer[i] == '|')
	{
	  pipeCount++;
	  *pipeIndexPtr = i;
	}
    }
  if (pipeCount != 1)
    {
      return 1;
    }
  return 0;
}
//Given a char buffer and the index of the second-to-last '|' character (it must exist at this point), returns the length parameter of the message.
int getMessageLength(char * buffer, int pipeIndex)
{
  if (pipeIndex == 4) return 0;

  char length[pipeIndex-3];
  int i;
  for (i = 4; i < pipeIndex; i++)
    {
      length[i-4] = buffer[i];
    }
  length[i-4] = '\0';
  return atoi(length);
}
//Given a char buffer, the index of the second-to-last '|' character and the number of bytes in the message, returns the message content.
char * getMessageContent(char * buffer, int pipeIndex, int n)
{
  char * message = (char*)malloc((n-pipeIndex-1)*sizeof(char));
  int i = pipeIndex + 1;
  for (i = pipeIndex + 1; i < (n-1); i++)
    {
      message[i-pipeIndex-1] = buffer[i];
    }
  message[i-pipeIndex-1] = '\0';
  return message;
}
//Given a char buffer and its size, copies the data into a new buffer of twice the size. The new buffer is returned and the old buffer is freed.
char * newBuffer(char * oldBuffer, int oldSize)
{
  char * newBuf = malloc(2*oldSize);
  bzero(newBuf, 2*oldSize);
  int i;
  for (i = 0; i < oldSize-1; i++)
    {
      newBuf[i] = oldBuffer[i];
    }
  free(oldBuffer);
  return newBuf;
}
//Attempts to read in a regular message into a buffer and returns it.
char * regRead(int newsockfd)
{
  int bufferSize = 256;
  char * buffer = malloc(256);
  bzero(buffer, 256);
  buffer[0] = 'R';
  buffer[1] = 'E';
  buffer[2] = 'G';
  int n = 3;
  n += read(newsockfd, buffer+n, 1);

  //REG|
  if (buffer[3] != '|') return buffer;

  int read_size = read(newsockfd, buffer+n, 1);
  //REG|xx...
  while (read_size > 0 && isdigit(buffer[n]))
    {
      n += read_size;
      if (n == bufferSize-1)
	{
	  buffer = newBuffer(buffer, bufferSize);
	  bufferSize *= 2;
	}
      read_size = read(newsockfd, buffer+n, 1);
    }
  //REG|xx..|
  if (buffer[n] != '|' || n == 4) return buffer;

  int length = getMessageLength(buffer, n);

  n++;
  int i = 0;
  read_size = read(newsockfd, buffer+n, 1);
  while (read_size > 0 && i < length)
    {
      //An early closing bar.
      if (buffer[n] == '|') return buffer;
      n += read_size;
      if (n == bufferSize-1)
	{
	  buffer = newBuffer(buffer, bufferSize);
	  bufferSize *= 2;
	}
      i++;
      read_size = read(newsockfd, buffer+n, 1);
    }
  return buffer;
}
//Reads one byte at a time, checking to see if the stream matches a legitimate error code.
void errRead(int newsockfd, char * buffer, int errorNo)
{
  int n = 3;
  n += read(newsockfd, buffer+n, 1);

  //ERR|
  if (buffer[3] != '|') return;

  n += read(newsockfd, buffer+n, 1);
  //ERR|M
  if (buffer[4] != 'M') return;

  n += read(newsockfd, buffer+n, 1);

  //ERR|Mx
  if (buffer[5] != ('0' + errorNo)) return;

  n += read(newsockfd, buffer+n, 1);

  //ERR|MxF OR ERR|MxL OR ERR|MxC
  if (buffer[6] != 'F' && buffer[6] != 'L' && buffer[6] != 'C') return;

  n += read(newsockfd, buffer+n, 1);

  //ERR|MxF OR ERR|MxLN OR ERR|MxCT
  if (!(buffer[6] == 'F' && buffer[7] == 'T') &&
      !(buffer[6] == 'L' && buffer[7] == 'N') &&
      !(buffer[6] == 'C' && buffer[7] == 'T')) return;

  n += read(newsockfd, buffer+n, 1);

  return;
}
//Reads one byte at a time, checking to see if the message stream begins with REG or ERR. As soon as an incorrect character is received, no more data is read. Returns a buffer containing all read characters.
char * newRead(int newsockfd, int messageNo)
{
  int bufferSize = 256;
  char * buffer = malloc(256);
  int n = 0;
  
  bzero(buffer, bufferSize);

  //Read first byte.
  n += read(newsockfd, buffer, 1);
  if (buffer[0] == 'R')
    {
      n += read(newsockfd, buffer+n, 1);
      if (buffer[1] == 'E')
	{
	  n += read(newsockfd, buffer+n, 1);
	  if (buffer[2] == 'G')
	    {
	      free(buffer);
	      return regRead(newsockfd);
	    }
	}
    }
  else if (buffer[0] == 'E')
    {
      n += read(newsockfd, buffer+n, 1);
      if (buffer[1] == 'R')
	{
	  n += read(newsockfd, buffer+n, 1);
	  if (buffer[2] == 'R') errRead(newsockfd, buffer, messageNo-1);
	}
    }
  return buffer;
}
//Runs the KKJ Protocol. The setup and punch pointers are chosen randomly from the joke list.
void KKJ(int newsockfd, char * setup, char * punch)
{
  int n;
  int pipeIndex = -1;
  int serverPipeIndex = -1;
  char * buffer;
  
  //Knock, knock.
  n = write(newsockfd, "REG|13|Knock, knock.|", 21);
  printf("SERVER wrote message 0 \"REG|13|Knock, knock.|\" to CLIENT.\n");
  
  //Reads client response.
  buffer = newRead(newsockfd, 1);
  n = strlen(buffer);
  printf("SERVER read message 1 \"%s\" from CLIENT.\n", buffer);
  
  //Returns if an error message is read.
  if (errorCheck(buffer, n, 0) == 1)
    {
      free(buffer);
      return;
    }
  //If the client message is in incorrect form, writes an error message and returns.
  if (formatCheck(buffer, n, &pipeIndex) == 1)
    {
      n = write(newsockfd, "ERR|M1FT|", 9);
      free(buffer);
      printf("ERROR M1FT: Message 1 format was broken.\n");
      return;
    }
  //Gets client message content.
  char * messageContent = getMessageContent(buffer, pipeIndex, n);
  
  //If the client message length value != strlen(messageContent), writes an error message and returns.
  if (getMessageLength(buffer, pipeIndex) != strlen(messageContent))
    {
      n = write(newsockfd, "ERR|M1LN|", 9);
      free(messageContent);
      free(buffer);
      printf("ERROR M1LN: Message 1 length was not correct.\n");
      return;
    }
  //If the client message content is not "Who's there?", writes an error message and returns.
  if (strcmp(messageContent, "Who's there?") != 0)
    {
      free(messageContent);
      free(buffer);
      n = write(newsockfd, "ERR|M1CT|", 9);
      printf("ERROR M1CT: Message 1 content was not correct.\n");
      return;
    }
  free(messageContent);
  free(buffer);
  
  //////////////////////////////////////////
  //SUCCESS: Received REG|12|Who's there?|//
  //////////////////////////////////////////
  
  //Sends initial setup.
  n = write(newsockfd, setup, strlen(setup));
  printf("SERVER wrote message 2 \"%s\" to CLIENT.\n", setup);
  
  //Waits for client response.
  buffer = newRead(newsockfd, 3);
  n = strlen(buffer);
  printf("SERVER read message 3 \"%s\" from CLIENT.\n", buffer);
  
  //Returns if an error message is read.
  if (errorCheck(buffer, n, 2) == 1)
    {
      free(buffer);
      return;
    }
  //If the client message is in incorrect form, writes an error message and returns.
  if (formatCheck(buffer, n, &pipeIndex) == 1)
    {
      n = write(newsockfd, "ERR|M3FT|", 9);
      free(buffer);
      printf("ERROR M3FT: Message 3 format was broken.\n");
      return;
    }
  //Gets client message content.
  messageContent = getMessageContent(buffer, pipeIndex, n);
 
  //If the client message length value != strlen(messageContent), writes an error message and returns.
  if (getMessageLength(buffer, pipeIndex) != strlen(messageContent))
    {
      n = write(newsockfd, "ERR|M3LN|", 9);
      free(messageContent);
      free(buffer);
      printf("ERROR M3LN: Message 3 length was not correct.\n");
      return;
    }
  
  //Prepares the expected response.
  formatCheck(setup, strlen(setup), &serverPipeIndex);
  char * expectedResponse = whoAppend(getMessageContent(setup, serverPipeIndex, strlen(setup)));
  
  //If the client message content is not the expected response, writes an error message and returns.
  if (strcmp(messageContent, expectedResponse) != 0)
    {
      free(expectedResponse);
      free(messageContent);
      free(buffer);
      n = write(newsockfd, "ERR|M3CT|", 9);
      printf("ERROR M3CT: Message 3 content was not correct.\n");
      return;
    }
  free(expectedResponse);
  free(messageContent);
  free(buffer);

  //////////////////////////////////////////////////////////
  //SUCCESS: Received REG|expectedLength|expectedResponse|//
  //////////////////////////////////////////////////////////
  
  //Sends punchline.
  n = write(newsockfd, punch, strlen(punch));
  printf("SERVER wrote message 4 \"%s\" to CLIENT.\n", punch);
  
  //Waits for the client response.
  buffer = newRead(newsockfd, 5);
  n = strlen(buffer);
  printf("SERVER read message 5 \"%s\" from CLIENT.\n", buffer);

  //Returns if an error message is read.
  if (errorCheck(buffer, n, 4) == 1)
    {
      free(buffer);
      return;
    }
  //If the client message is in incorrect form, writes an error message and returns.
  if (formatCheck(buffer, n, &pipeIndex) == 1)
    {
      n = write(newsockfd, "ERR|M5FT|", 9);
      free(buffer);
      printf("ERROR M5FT: Message 5 format was broken.\n");
      return;
    }
  //Gets client message content.
  messageContent = getMessageContent(buffer, pipeIndex, n);
  
  //If the client message length value != strlen(messageContent), writes an error message and returns.
  if (getMessageLength(buffer, pipeIndex) != strlen(messageContent))
    {
      n = write(newsockfd, "ERR|M5LN|", 9);
      free(messageContent);
      free(buffer);
      printf("ERROR M5LN: Message 5 length was not correct.\n");
      return;
    }
  //If the client message content does not end in punctuation, writes an error message and returns.
  if (strlen(messageContent) == 0 || !ispunct(messageContent[strlen(messageContent)-1]))
    {
      free(messageContent);
      free(buffer);
      n = write(newsockfd, "ERR|M5CT|", 9);
      printf("ERROR M5CT: Message 5 content was not correct.\n");
      return;
    }
  free(messageContent);
  free(buffer);
  return;
}
//main.
int main(int argc, char ** argv)
{
  //No port number specified.
  if (argc == 1)
    {
      printf("ERROR: No port number specified.\n");
      return 0;
    }
  //Too many arguments.
  if (argc > 3)
    {
      printf("ERROR: Too many arguments.\n");
      return 0;
    }
  
  //Takes port from argv[1], converts it to an int and checks whether it is out of bounds.
  int port = atoi(argv[1]);
  if (port <= 5000 || port >= 65536)
    {
      printf("ERROR: Port number out of bounds.\n");
      return 0;
    }

  char ** jokelist;
  int listSize;
  
  //Loads the joke list from the path specified in argv[2].
  FILE * fp = fopen(argv[2], "r");
  if (fp == NULL)
    {
      printf("ERROR: Could not load jokelist.txt from argv[2].\n");
      return 0;
    }
  else
    {
      jokelist = getJokeList(fp, &listSize);
      fclose(fp);
    }

  
  //Creates socket descriptor.
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    {
      printf("ERROR: Cannot open socket.\n");
      freeJokeList(jokelist, listSize);
      return 0;
    }
  //Initializes socket address info struct.
  struct sockaddr_in serverAddressInfo;
  bzero((char*) &serverAddressInfo, sizeof(serverAddressInfo));
  serverAddressInfo.sin_port = htons(port);
  serverAddressInfo.sin_family = AF_INET;
  serverAddressInfo.sin_addr.s_addr = INADDR_ANY;

  //Binds the server socket to local port.
  if (bind(sockfd, (struct sockaddr *) &serverAddressInfo, sizeof(serverAddressInfo)) < 0)
    {
      printf("ERROR: Problem binding server socket to local port.\n");
      freeJokeList(jokelist, listSize);
      return 0;
    }
  int newsockfd;
  int clilen;
  int pid;
  struct sockaddr_in clientAddressInfo;

  listen(sockfd, 0);
  clilen = sizeof(clientAddressInfo);

  //Keeps handling connections.
  while(1)
  {
      newsockfd = accept(sockfd, (struct sockaddr *)&clientAddressInfo, &clilen);
      
      if (newsockfd < 0)
	{
	  printf("ERROR: Problem with client connection.\n");
	}

      //Creates client process.
      pid = fork();
      if (pid < 0)
	{
	  printf("ERROR: Could not fork() off of parent process.\n");
	}
      if (pid == 0)
	{
	  close(sockfd);

	  //Client process receives a random joke.
	  int jokeIndex = getRandJoke(jokelist, listSize);
	  KKJ(newsockfd, jokelist[jokeIndex], jokelist[jokeIndex+1]);
	  freeJokeList(jokelist, listSize);
	  return 0;
	}
      else
	{
	  close(newsockfd);
	}
  }
  return 0;
}
