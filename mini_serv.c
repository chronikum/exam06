#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>

int maxSocket = 0, serverSocket = -1, next_id = 0;
int clients[65536], currmsg[65536];
char bufferRead[4096 * 42], bufferWrite[4096 * 42 + 42], bufferWriteMessage[4096 * 42];
fd_set readSockets, writeSockets, activeSockets;

void sendAll(int sender)
{
    for (int i = 0; i <= maxSocket; i++)
        if (FD_ISSET(i, &writeSockets) && i != sender)
            send(i, bufferWrite, strlen(bufferWrite), 0);
}

void fatal()
{
    write(2, "Fatal error\n", strlen("Fatal error\n"));
    close(serverSocket);
    exit(1);
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
        exit(1);
    }

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serverAddr;
    socklen_t len;

    serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	serverAddr.sin_port = htons(atoi(argv[1]));

    if (serverSocket < 0)
        fatal();
    if ((bind(serverSocket, (const struct sockaddr *) &serverAddr, sizeof(serverAddr))) < 0)
        fatal();
    if (listen(serverSocket, 128) < 0)
        fatal();

    FD_ZERO(&activeSockets);
    FD_SET(serverSocket, &activeSockets);
    maxSocket = serverSocket;
    bzero(clients, sizeof(clients));

    while (1)
    {
        readSockets = writeSockets = activeSockets;
        if (select(maxSocket + 1, &readSockets, &writeSockets, NULL, NULL) <= 0)
            continue;
        for (int socketId = 0; socketId <= maxSocket; socketId++)
        {
            if (FD_ISSET(socketId, &readSockets))
            {
                if (serverSocket == socketId)
                {
                    int clientSocket = accept(serverSocket, (struct sockaddr *) &serverAddr, &len);
                    if (clientSocket < 0)
                        continue;
                    FD_SET(clientSocket, &activeSockets);
                    clients[clientSocket] = next_id++;
                    currmsg[clientSocket] = 0;
                    maxSocket = clientSocket > maxSocket ? clientSocket : maxSocket;
                    sprintf(bufferWrite, "server: client %d just arrived\n", clients[clientSocket]);
                    sendAll(clientSocket);
                    break;
                }
                else
                {
                    int bytesRead = recv(socketId, bufferRead, 4096 * 42, 0);
                    if (bytesRead <= 0)
                    {
                        sprintf(bufferWrite, "server: client %d just left\n", clients[socketId]);
                        sendAll(socketId);
                        close(socketId);
                        FD_CLR(socketId, &activeSockets);
                        break;
                    }
                    else
                    {
                        for (int i = 0, j = 0; i < bytesRead; i++, j++)
                        {
                            bufferWriteMessage[j] = bufferRead[i];
                            if (bufferWriteMessage[j] == '\n')
                            {
                                bufferWriteMessage[j + 1] = '\0';
                                if (currmsg[socketId])
                                    sprintf(bufferWrite, "%s", bufferWriteMessage);
                                else
                                    sprintf(bufferWrite, "client %d: %s", clients[socketId], bufferWriteMessage);
                                currmsg[socketId] = 0;
                                j = -1;
                                sendAll(socketId);
                            }
                            else if (i == (bytesRead - 1))
                            {
                                bufferWriteMessage[j + 1] = '\0';
                                if (currmsg[socketId])
                                    sprintf(bufferWrite, "%s", bufferWriteMessage);
                                else
                                    sprintf(bufferWrite, "client %d: %s", clients[socketId], bufferWriteMessage);
                                currmsg[socketId] = 1;
                                sendAll(socketId);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}