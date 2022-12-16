#include "definitions.h"

#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

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

    int clientSocket[2];
    struct sockaddr_in clientAddress[2];

    //vytvorenie vlakna pre zapisovanie dat do socketu <pthread.h>
    pthread_t threads[2];
    DATA data[2];

    //server caka na pripojenie klienta <sys/socket.h>
    //a vytvara nove vlakna pre kazdeho klienta
    for (int i = 0; i < 2; i++) {
        socklen_t clientAddressLength = sizeof(clientAddress[i]);
        clientSocket[i] = accept(serverSocket, (struct sockaddr *)&clientAddress[i], &clientAddressLength);

        if (clientSocket[i] < 0) {
            printError("Chyba - accept.");
        }

        //inicializacia dat zdielanych medzi vlaknami

        data_init(&data[i], userName, clientSocket[i]);

        pthread_create(&threads[i], NULL, data_writeData, (void *)&data[i]);
        pthread_create(&threads[i], NULL, data_readData, (void *)&data[i]);
    }

    for (int i = 0; i < 2; i++) {
        pthread_join(threads[i], NULL);
        data_destroy(&data[i]);
        //uzavretie socketu klienta <unistd.h>
        close(clientSocket[i]);
    }

    return (EXIT_SUCCESS);
}