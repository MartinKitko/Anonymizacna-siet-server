#include "definitions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

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

void * processMessage(void *arg) {
    /*
    // Download the URL
    char *url = msgData;

    // Create a curl handle
    CURL *curl_handle = curl_easy_init();

    // Set the URL to download
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    // Set the callback function to write the contents to a string
    char *html_content = malloc(1);
    *html_content = '\0';
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_to_string);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, html_content);

    // Download the contents of the URL
    curl_easy_perform(curl_handle);

    // Clean up
    curl_easy_cleanup(curl_handle);*/
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
            int received = recv(clientSocket, buffer, BUFFER_SIZE, 0);//doesnt stopg
            if (received < 0) {
                printError("Chyba - recv numNodes of nodes.");
            }
            printf("%s\n", buffer);
            char *endptr;
            numNodes = (int)strtol(buffer, &endptr, 10);
            if (*endptr != '\0') {  // the string contains invalid characters
                printError("Chyba - zle zadane udaje.");
            }
            if (numNodes < 3 || numNodes > NUM_NODES) {  // the integer is out of range
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

                exitNodeSocket = accept(serverSocket, (struct sockaddr *) &exitNodeAddress, &exitNodeAddressLength);
                if (exitNodeSocket < 0) {
                    printError("Chyba - server node accept.");
                }
                pthread_create(&processing, NULL, processMessage, &exitNodeSocket);

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

            data_init(&clients[numClients].data, userName, exitNodeSocket);
            //pthread_create(&clients[numClients].thread, NULL, data_writeData, (void *) &clients[numClients].data);
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