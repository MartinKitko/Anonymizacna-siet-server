#include "definitions.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

int serverSocket;
NODE nodes[NUM_NODES];
bool keepRunning = true;

int main(int argc, char *argv[]) {
    srand(time(NULL));
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
        nodes[i].id = i + 1;
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
    //CLIENT clients[MAX_CLIENTS];
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
    NODE randomNodes[NUM_NODES];

    DATA exitNodeD;
    pthread_t endThread, exitNodeThread;
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

            //ziskanie poctu uzlov od klienta
            int BUFFER_SIZE = 1024;
            char buffer[BUFFER_SIZE];
            int received = recv(clientSocket, buffer, BUFFER_SIZE, 0);
            if (received < 0) {
                printError("Chyba - recv numNodes of nodes.");
            } else if (received == 0) {
                printError("Uzol");
            }
            char *endptr;
            numNodes = (int) strtol(buffer, &endptr, 10);
            if (*endptr != '\0') {
                printError("Chyba - zle zadane udaje.");
            }
            if (numNodes != 0 && (numNodes < 3 || numNodes > NUM_NODES)) {
                printError("Chyba - pocet uzlov musi byt v rozsahu 3-20.");
            }

            if (numNodes == 0) { // klient sa pripaja ako uzol

            }

            //vyber daneho poctu nahodnych uzlov
            int indexes[NUM_NODES];
            for (int i = 0; i < NUM_NODES; i++) {
                indexes[i] = i;
            }

            for (int i = NUM_NODES - 1; i > 0; i--) {
                int j = rand() % (i + 1);
                int temp = indexes[i];
                indexes[i] = indexes[j];
                indexes[j] = temp;
            }

            for (int i = 0; i < numNodes; i++) {
                randomNodes[i] = nodes[indexes[i]];
            }

            //spojenie vybranych uzlov
            if (numClients == 0) {
                for (int i = 0; i < numNodes; i++) {
                    pthread_create(&nodeThreads[i], NULL, receiveAndForward, (void *) &randomNodes[i]);
                }
                for (int i = 0; i < numNodes - 1; i++) {
                    if (connect(randomNodes[i].socketOut, (struct sockaddr *) &randomNodes[i + 1].address,
                                sizeof(randomNodes[i + 1].address)) < 0) {
                        printError("Chyba - node connect to next node.");
                    }
                }

                if (connect(randomNodes[numNodes - 1].socketOut, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) <
                    0) {
                    printError("Chyba - node connect to server.");
                }

                exitNodeSocket = accept(serverSocket, (struct sockaddr *) &exitNodeAddress, &exitNodeAddressLength);
                if (exitNodeSocket < 0) {
                    printError("Chyba - server node accept.");
                }
                pthread_create(&processing, NULL, processMessage, &exitNodeSocket);

                data_init(&exitNodeD, userName, exitNodeSocket);
                pthread_create(&exitNodeThread, NULL, data_readData, (void *) &exitNodeD);
            }

            struct sockaddr_in entryNodeAddress = randomNodes[0].address;
            send(clientSocket, &entryNodeAddress, sizeof(entryNodeAddress), 0);
            // v tento moment sa klient odpaja od servera a pripaja na entry node
            close(clientSocket);

            if (numClients < MAX_CLIENTS) {
                //clients[numClients].socket = clientSocket;
                numClients++;
            } else {
                printf("Dosiahnuty maximalny pocet klientov!\n");
                close(clientSocket);
            }
        } else if (result != 0) {
            printError("Chyba - select.");
        }
    }

    pthread_join(processing, NULL);
    pthread_join(exitNodeThread, NULL);
    pthread_join(endThread, NULL);
    data_destroy(&exitNodeD);
    close(exitNodeSocket);
    close(serverSocket);

    if (numClients > 0) {
        for (int i = 0; i < numNodes; ++i) {
            pthread_join(nodeThreads[i], NULL);
        }
        /*for (int i = 0; i < numClients; i++) {
            pthread_join(clients[i].thread, NULL);
            data_destroy(&clients[i].data);
            close(clients[i].socket);
        }*/
    }

    for (int i = 0; i < NUM_NODES; ++i) {
        close(nodes[i].socketIn);
        close(nodes[i].socketOut);
    }

    return (EXIT_SUCCESS);
}