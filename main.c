#include "definitions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

int serverSocket;
NODE nodes[NUM_NODES];
bool keepRunning = true;

void *receiveAndForward(void *arg) {
    NODE *node = arg;
    int BUFFER_SIZE = 1024;
    char buffer[BUFFER_SIZE];
    printf("Noda %d zacina.\n", node->id);

    struct sockaddr_in connectingNodeAddress;
    socklen_t connNodeAddrLength = sizeof(connectingNodeAddress);
    int newSocket = accept(node->socketIn, (struct sockaddr *) &connectingNodeAddress, &connNodeAddrLength);
    if (newSocket < 0) {
        char msg[32];
        sprintf(msg, "Chyba - node %d accept.", node->id);
        printError(msg);
    }

    fd_set readfds;
    struct timeval timeout;
    int recvFrom, sendTo, received;

    while (keepRunning) {
        FD_ZERO(&readfds);
        FD_SET(newSocket, &readfds);
        FD_SET(node->socketOut, &readfds);

        timeout.tv_sec = 5;

        int result = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
        if (result < 0) {
            printError("Chyba - node select.");
            break;
        } else if (result == 0) { // timeout
            continue;
        }

        if (FD_ISSET(newSocket, &readfds)) {
            recvFrom = newSocket;
            sendTo = node->socketOut;
        }

        if (FD_ISSET(node->socketOut, &readfds)) {
            recvFrom = node->socketOut;
            sendTo = newSocket;
        }

        received = recv(recvFrom, buffer, BUFFER_SIZE, 0);

        if (received < 0) {
            char msg[32];
            sprintf(msg, "Chyba - node %d recv.", node->id);
            printError(msg);
        } else if (received == 0) {
            printf("Spojenie ukoncene.\n");
            break;
        }

        printf("Noda %d prijala spravu a posiela ju dalej.\n", node->id);

        if (send(sendTo, buffer, received, 0) < 0) {
            printError("Chyba - node send.");
        }
    }

    close(newSocket);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printError("Sever je nutne spustit s nasledujucimi argumentmi: port pouzivatel.");
    }
    int port = atoi(argv[1]);
    if (port <= 0) {
        printError("Port musi byt cele cislo vacsie ako 0.");
    }
    char *userName = argv[2];

    //vytvorenie TCP socketu <sys/socket.h>
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        printError("Chyba - socket.");
    }
    int option = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
        printError("Chyba - setsockopt.");
    }

    //definovanie adresy servera <arpa/inet.h>
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;         //internetove sockety
    serverAddress.sin_addr.s_addr = INADDR_ANY; //prijimame spojenia z celeho internetu
    serverAddress.sin_port = htons(port);       //nastavenie portu

    //prepojenie adresy servera so socketom <sys/socket.h>
    if (bind(serverSocket, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        printError("Chyba - bind.");
    }

    //server bude prijimat nove spojenia cez socket serverSocket <sys/socket.h>
    listen(serverSocket, 10);

    for (int i = 0; i < NUM_NODES; i++) {
        nodes[i].id = i;
        nodes[i].socketIn = socket(AF_INET, SOCK_STREAM, 0);
        if (nodes[i].socketIn < 0) {
            printError("Chyba - node socket in.");
        }

        nodes[i].socketOut = socket(AF_INET, SOCK_STREAM, 0);
        if (nodes[i].socketOut < 0) {
            printError("Chyba - node socket out.");
        }

        bzero((char *) &nodes[i].address, sizeof(nodes[i].address));
        nodes[i].address.sin_family = AF_INET;
        nodes[i].address.sin_addr.s_addr = INADDR_ANY;
        nodes[i].address.sin_port = htons(port + i + 1);

        if (setsockopt(nodes[i].socketIn, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) < 0) {
            printError("Chyba - node setsockopt.");
        }
        if (bind(nodes[i].socketIn, (struct sockaddr *) &nodes[i].address, sizeof(nodes[i].address)) < 0) {
            printError("Chyba - node bind in.");
        }

        listen(nodes[i].socketIn, 10);
    }

    pthread_t nodeThreads[NUM_NODES];
    CLIENT clients[MAX_CLIENTS];
    int numClients = 0;

    int serverNodeSocket;
    struct sockaddr_in serverNodeAddress;
    socklen_t serverNodeAddressLength = sizeof(serverNodeAddress);

    int clientSocket;
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    fd_set readfds;
    struct timeval timeout;

    while (keepRunning) {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);

        timeout.tv_sec = 5;
        timeout.tv_sec = 0;
        int result = select(serverSocket+1, &readfds, NULL, NULL, &timeout);

        if (result > 0) {
            clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &clientAddressLength);
            if (clientSocket < 0) {
                printError("Chyba - accept.");
            }

            if (numClients == 0) {
                for (int i = 0; i < NUM_NODES; i++) {
                    pthread_create(&nodeThreads[i], NULL, receiveAndForward, (void *) &nodes[i]);
                }
                for (int i = 0; i < NUM_NODES - 1; i++) {
                    if (connect(nodes[i].socketOut, (struct sockaddr *) &nodes[i + 1].address, sizeof(nodes[i + 1].address)) < 0) {
                        printError("Chyba - node connect to next node.");
                    }
                }

                if (connect(nodes[NUM_NODES - 1].socketOut, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
                    printError("Chyba - node connect to server.");
                }

                serverNodeSocket = accept(serverSocket, (struct sockaddr *) &serverNodeAddress, &serverNodeAddressLength);
                if (serverNodeSocket < 0) {
                    printError("Chyba - server node accept.");
                }

                struct sockaddr_in entryNodeAddress = nodes[0].address;
                send(clientSocket, &entryNodeAddress, sizeof(entryNodeAddress), 0);
                // v tento moment sa klient odpaja od servera a pripaja na entry node
                close(clientSocket);

                /*int BUFFER_SIZE = 1024;
                char buffer[BUFFER_SIZE];
                int received = recv(nodeConnectData.socket, buffer, BUFFER_SIZE, 0);
                if (received < 0) {
                    printError("Chyba - recv ennod.");
                }
                printf("%s\n", buffer);*/
            }

            data_init(&clients[numClients].data, userName, serverNodeSocket);
            pthread_create(&clients[numClients].thread, NULL, data_writeData, (void *) &clients[numClients].data);
            pthread_create(&clients[numClients].thread, NULL, data_readData, (void *) &clients[numClients].data);

            if (numClients < MAX_CLIENTS) {
                clients[numClients].socket = clientSocket;
                numClients++;
            } else {
                printf("Dosiahnuty maximalny pocet klientov!\n");
                close(clientSocket);
            }
        } else if (result != 0) {
            printError("Chyba - select.");
        }
    }

    close(serverSocket);

    for (int i = 0; i < numClients; i++) {
        pthread_join(clients[i].thread, NULL);
        data_destroy(&clients[i].data);
        close(clients[i].socket);
    }

    for (int i = 0; i < NUM_NODES; ++i) {
        pthread_join(nodeThreads[i], NULL);
        close(nodes[i].socketIn);
        close(nodes[i].socketOut);
    }

    return (EXIT_SUCCESS);
}