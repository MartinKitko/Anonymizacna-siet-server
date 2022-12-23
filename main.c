#include "definitions.h"
#include "downloader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <curl/curl.h>

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

void *processMessage(void *arg) {
    int *socket = arg;
    fd_set readfds;
    struct timeval timeout;
    int received;

    while (keepRunning) {
        FD_ZERO(&readfds);
        FD_SET(*socket, &readfds);

        timeout.tv_sec = 1;
        int result = select(FD_SETSIZE, &readfds, NULL, NULL, &timeout);
        if (result < 0) {
            printError("Chyba - exit node select");
            break;
        } else if (result == 0) { // timeout
            continue;
        }

        char buffer[BUFFER_LENGTH + 1];
        received = recv(*socket, buffer, sizeof(buffer), 0);
        if (received < 0) {
            printError("Chyba - exit node recv.");
        } else if (received == 0) {
            printf("Spojenie ukoncene.\n");
            return NULL;
        }
        buffer[received] = '\0';

        char username[BUFFER_LENGTH + 1];
        char *colon = strstr(buffer, ": ");
        if (colon != NULL) {
            strncpy(username, buffer, colon - buffer);
            username[colon - buffer] = '\0';
        }
        char *url = colon + 2;

        struct Downloader downloader;
        downloader_init(&downloader, url);
        result = downloader_download(&downloader);
        if (result == CURLE_OK) {
            printf("User: %s\n%s\n", username, downloader.content);
            strncpy(buffer, downloader.content, BUFFER_LENGTH);
        } else {
            strncpy(buffer, "Stiahnutie neuspesne, skontrolujte URL.", BUFFER_LENGTH);
        }
        downloader_free(&downloader);

        if (send(*socket, buffer, BUFFER_LENGTH, 0) < 0) {
            printError("Chyba - exit node send");
        }
    }

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
    pthread_t processing;
    CLIENT clients[MAX_CLIENTS];
    int numClients = 0;

    int exitNodeSocket;
    struct sockaddr_in exitNodeAddress;
    socklen_t exitNodeAddressLength = sizeof(exitNodeAddress);

    int clientSocket;
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    fd_set readfds;
    struct timeval timeout;
    int numNodes;

    pthread_t endThread;
    pthread_create(&endThread, NULL, stop, NULL);

    while (keepRunning) {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);

        timeout.tv_usec = 0;
        timeout.tv_sec = 1;
        int result = select(serverSocket + 1, &readfds, NULL, NULL, &timeout);

        if (result > 0) {
            clientSocket = accept(serverSocket, (struct sockaddr *) &clientAddress, &clientAddressLength);
            if (clientSocket < 0) {
                printError("Chyba - accept.");
            }

            int BUFFER_SIZE = 1024;
            char buffer[BUFFER_SIZE];
            int received = recv(clientSocket, buffer, BUFFER_SIZE, 0);
            if (received < 0) {
                printError("Chyba - recv numNodes of nodes.");
            }
            printf("%s\n", buffer);
            char *endptr;
            numNodes = (int) strtol(buffer, &endptr, 10);
            if (*endptr != '\0') {
                printError("Chyba - zle zadane udaje.");
            }
            if (numNodes < 3 || numNodes > NUM_NODES) {
                printError("Chyba - pocet uzlov musi byt v rozsahu 3-20.");
            }

            if (numClients == 0) {
                for (int i = 0; i < numNodes; i++) {
                    pthread_create(&nodeThreads[i], NULL, receiveAndForward, (void *) &nodes[i]);
                }
                for (int i = 0; i < numNodes - 1; i++) {
                    if (connect(nodes[i].socketOut, (struct sockaddr *) &nodes[i + 1].address,
                                sizeof(nodes[i + 1].address)) < 0) {
                        printError("Chyba - node connect to next node.");
                    }
                }

                if (connect(nodes[numNodes - 1].socketOut, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) <
                    0) {
                    printError("Chyba - node connect to server.");
                }
            }

            exitNodeSocket = accept(serverSocket, (struct sockaddr *) &exitNodeAddress, &exitNodeAddressLength);
            if (exitNodeSocket < 0) {
                printError("Chyba - server node accept.");
            }
            pthread_create(&processing, NULL, processMessage, &exitNodeSocket);

            struct sockaddr_in entryNodeAddress = nodes[0].address;
            send(clientSocket, &entryNodeAddress, sizeof(entryNodeAddress), 0);
            // v tento moment sa klient odpaja od servera a pripaja na entry node
            close(clientSocket);

            data_init(&clients[numClients].data, userName, exitNodeSocket);
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

    pthread_join(endThread, NULL);
    close(serverSocket);

    if (numClients > 0) {
        for (int i = 0; i < numNodes; ++i) {
            pthread_join(nodeThreads[i], NULL);
        }
        for (int i = 0; i < numClients; i++) {
            pthread_join(clients[i].thread, NULL);
            data_destroy(&clients[i].data);
            close(clients[i].socket);
        }
    }

    for (int i = 0; i < numNodes; ++i) {
        close(nodes[i].socketIn);
        close(nodes[i].socketOut);
    }

    return (EXIT_SUCCESS);
}