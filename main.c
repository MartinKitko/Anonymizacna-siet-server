#include "definitions.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>

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

    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);
    fd_set readfds;
    struct timeval timeout;
    int numNodes;
    NODE randomNodes[NUM_NODES];

    pthread_t endThread;
    pthread_create(&endThread, NULL, stop, NULL);
    struct sockaddr_in clientNodeAddr;
    int numClientNodes = 0;
    int clientSockets[2];
    int clientNodeIndex = -1;

    while (keepRunning) {
        FD_ZERO(&readfds);
        FD_SET(serverSocket, &readfds);

        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        int result = select(serverSocket + 1, &readfds, NULL, NULL, &timeout);

        if (result > 0) {
            clientSockets[numClients] = accept(serverSocket, (struct sockaddr *) &clientAddress, &clientAddressLength);
            if (clientSockets[numClients] < 0) {
                printError("Chyba - accept.");
            }

            //ziskanie poctu uzlov od klienta
            int BUFFER_SIZE = 1024;
            char buffer[BUFFER_SIZE];
            int received = recv(clientSockets[numClients], buffer, BUFFER_SIZE, 0);
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
                // potvrdenie pripojenia klientskeho uzla
                char* confirmStr = "confirmation";
                if (send(clientSockets[0], confirmStr, strlen(confirmStr), 0) < 0) {
                    printError("Chyba - nextNode addr send");
                }

                // prijatie adresy klientskeho uzla
                printf("Prijimanie client node addr\n");
                if (recv(clientSockets[0], &clientNodeAddr, sizeof(clientNodeAddr), 0) < 0) {
                    printError("Chyba - clientNode addr recv");
                }
                numClientNodes++;
                numClients++;
                printf("Klient pripojeny ako noda\n");
            } else { // klinet sa pripaja ako koncovy pouzivatel
                //vyber daneho poctu nahodnych uzlov
                numClients++;
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

                // nahodny vyber indexu na nahradenie "umelej" nody klientskym uzlom
                if (numClientNodes == 1) {
                    clientNodeIndex = 1 + rand() % (numNodes - 3);
                }

                if (true) {
                    // spojenie uzlov pre vytvorenie cesty od klienta k serveru
                    for (int i = 0; i < numNodes - 1; i++) {
                        if (numClientNodes == 1) {
                            if (i == clientNodeIndex - 1) { // pridanie klientskej nody do cesty
                                if (send(clientSockets[0], &randomNodes[i + 2].address, sizeof(randomNodes[i + 2].address), 0) < 0) {
                                    printError("Chyba - nextNode addr send");
                                }
                                printf("Noda 100 zacina\n");
                                if (connect(randomNodes[i].socketOut, (struct sockaddr *) &clientNodeAddr,
                                            sizeof(clientNodeAddr)) < 0) {
                                    printError("Chyba - node connect to client.");
                                }
                                printf("Noda %d sa pripaja na klienta\n", randomNodes[i].id);
                                continue;
                            } else if (i == clientNodeIndex) { // preskoci vynechanu nodu ak existuje klientska noda
                                continue;
                            }
                        }
                        printf("Noda %d sa pripaja\n", randomNodes[i].id);
                        if (connect(randomNodes[i].socketOut, (struct sockaddr *) &randomNodes[i + 1].address,
                                    sizeof(randomNodes[i + 1].address)) < 0) {
                            printError("Chyba - node connect to next node.\n");
                        }
                    }
                    sleep(2);
                    // vytvorenie vlakien pre nody
                    for (int i = 0; i < numNodes; i++) {
                        if (numClientNodes == 1 && i == clientNodeIndex) { // vynechanie nody ak existuje klientska noda
                            continue;
                        }
                        pthread_create(&nodeThreads[i], NULL, receiveAndForward, (void *) &randomNodes[i]);
                    }

                    printf("Exit noda sa pripaja na server\n");
                    // pripojenie exit nody na server
                    if (connect(randomNodes[numNodes - 1].socketOut, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) <
                        0) {
                        printError("Chyba - node connect to server.");
                    }

                    exitNodeSocket = accept(serverSocket, (struct sockaddr *) &exitNodeAddress, &exitNodeAddressLength);
                    if (exitNodeSocket < 0) {
                        printError("Chyba - server node accept.");
                    }
                    printf("Exit noda sa pripojila na server\n");
                    pthread_create(&processing, NULL, processMessage, &exitNodeSocket);
                }

                // poslanie adresy prveho uzla cesty klientovi
                struct sockaddr_in entryNodeAddress = randomNodes[0].address;
                send(clientSockets[numClientNodes], &entryNodeAddress, sizeof(entryNodeAddress), 0);
                // klient sa odpaja od servera a pripaja na entry node
                close(clientSockets[0]);
                if (numClientNodes > 0) {
                    close(clientSockets[1]);
                }

                if (numClients == MAX_CLIENTS) {
                    printf("Dosiahnuty maximalny pocet klientov!\n");
                    close(clientSockets[numClientNodes]);
                }
            }
        } else if (result != 0) {
            printError("Chyba - select.");
        }
    }

    close(serverSocket);

    if (numClients - numClientNodes > 0) {
        pthread_join(processing, NULL);
        close(exitNodeSocket);
        for (int i = 0; i < numNodes; ++i) {
            if (i != clientNodeIndex) {
                pthread_join(nodeThreads[i], NULL);
            }
        }
        /*for (int i = 0; i < numClients; i++) {
            pthread_join(clients[i].thread, NULL);
            data_destroy(&clients[i].data);
            close(clients[i].socket);
        }*/
    }

    pthread_join(endThread, NULL);

    for (int i = 0; i < NUM_NODES; ++i) {
        close(nodes[i].socketIn);
        close(nodes[i].socketOut);
    }

    return (EXIT_SUCCESS);
}