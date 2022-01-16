#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define MAX_SIZE 1024
#define ID_SIZE 11 //10 byte per la tessera sanitaria più un byte per il terminatore

//Legge esattamente count byte s iterando opportunamente le letture. Legge anche se viene interrotta da una System Call.
ssize_t full_read(int fd, void *buffer, size_t count) {
    size_t n_left;
    ssize_t n_read;
    n_left = count;
    while (n_left > 0) {
        if ((n_read = read(fd, buffer, n_left)) < 0) {
            if (errno == EINTR) continue;
            else exit(n_read);
        } else if (n_read == 0) break;
        n_left -= n_read;
        buffer += n_read;
    }
    buffer = 0;
    return n_left;
}


//Scrive esattamente count byte s iterando opportunamente le scritture. Scrive anche se viene interrotta da una System Call.
ssize_t full_write(int fd, const void *buffer, size_t count) {
    size_t n_left;
    ssize_t n_written;
    n_left = count;
    while (n_left > 0) {
        if ((n_written = write(fd, buffer, n_left)) < 0) {
            if (errno == EINTR) continue;
            else exit(n_written);
        }
        n_left -= n_written;
        buffer += n_written;
    }
    buffer = 0;
    return n_left;
}

int main() {
    int socket_fd, welcome_size, package_size;
    struct sockaddr_in server_addr;
    char buffer[MAX_SIZE], ID[ID_SIZE];

    //Creazione del descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }

    //Valorizzazione struttura
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1026);

    //Conversione dell’indirizzo IP, preso in input come stringa in formato dotted, in un indirizzo di rete in network order.
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("inet_pton() error");
        exit(1);
    }

    //Effettua connessione con il server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() error");
        exit(1);
    }

    //Riceve il benevenuto dal ServerVerifica
    if (full_read(socket_fd, buffer, 109) < 0) {
        perror("full_read() error");
        exit(1);
    }
    printf("%s\n", buffer);

    while (1) {
        printf("Inserisci il numero di tessera sanitaria da scansionare [Massimo 10 caratteri]: ");
        if (fgets(ID, ID_SIZE, stdin) == NULL) {
            perror("fgets() error");
            exit(1);
        }
        ID[ID_SIZE - 1] = 0;
        if (strlen(ID) != ID_SIZE - 1) printf("Numero caratteri tessera sanitaria non corretto, devono essere esattamente 10! Riprovare\n\n");
        else break;
    }

    //Invio del numero di tessera sanitaria da convalidare al server verifica
    if (full_write(socket_fd, ID, ID_SIZE)) {
        perror("full_write() error");
        exit(1);
    }

    /*
        //Ricezione dell'ack
        if (full_read(socket_fd, &welcome_size, sizeof(int)) < 0) {
            perror("full_read error");
            exit(1);
        }
        if (full_read(socket_fd, buffer, welcome_size) < 0) {
            perror("full_read error");
            exit(1);
        }
        printf("%s\n\n", buffer);
    */

    exit(0);
}
