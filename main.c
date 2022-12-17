#include "definitions.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_CLIENTS 10

typedef struct {
    pthread_t thread;
    int clientSocket;
    DATA data;
} CLIENT;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printError("Sever je nutne spustit s nasledujucimi argumentmi: port pouzivatel.");
    }
    int port = atoi(argv[1]);
    if (port <= 0) {
        printError("Port musi byt cele cislo vacsie ako 0.");
    }
    char *userName = argv[2];

    //vytvorenie TCP socketu <sys/socket.h>
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        printError("Chyba - socket.");
    }

    //definovanie adresy servera <arpa/inet.h>
    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;         //internetove sockety
    serverAddress.sin_addr.s_addr = INADDR_ANY; //prijimame spojenia z celeho internetu
    serverAddress.sin_port = htons(port);       //nastavenie portu

    //prepojenie adresy servera so socketom <sys/socket.h>
    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        printError("Chyba - bind.");
    }

    //server bude prijimat nove spojenia cez socket serverSocket <sys/socket.h>
    listen(serverSocket, 10);

    CLIENT clients[MAX_CLIENTS];
    int numClients = 0;

    while (keepRunning) {
        int clientSocket;
        struct sockaddr_in clientAddress;
        socklen_t clientAddressLength = sizeof(clientAddress);
        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &clientAddressLength);

        if (clientSocket < 0) {
            printError("Chyba - accept.");
        }

        clients[numClients].clientSocket = clientSocket;

        data_init(&clients[numClients].data, userName, clientSocket);
        pthread_create(&clients[numClients].thread, NULL, data_writeData, (void *)&clients[numClients].data);
        pthread_create(&clients[numClients].thread, NULL, data_readData, (void *)&clients[numClients].data);

        numClients++;
    }

    for (int i = 0; i < numClients; i++) {
        pthread_join(clients[i].thread, NULL);
        data_destroy(&clients[i].data);
        close(clients[i].clientSocket);
    }

    printf("Uspesne ukoncene");

    return (EXIT_SUCCESS);
}