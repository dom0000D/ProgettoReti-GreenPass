#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#define MAX_SIZE 1024

//Struct del pacchetto che il centro vaccinale deve ricevere dall'utente contentente nome, cognome e numero di tessera sanitaria dell'utente
typedef struct {
    char name[MAX_SIZE];
    char surname[MAX_SIZE];
    char ID[MAX_SIZE];
} VAX_REQUEST;

//Struct contente la data del giorno di inizio validità del green pass formato dai campi giorno, mese ed anno
typedef struct {
    int day;
    int month;
    int year;
} START_DATE;

//Struct contente la data del giorno della scadenza della validità del green pass formato dai campi giorno, mese ed anno
typedef struct {
    int day;
    int month;
    int year;
} EXPIRE_DATE;

//Struct del pacchetto inviato dal centro vaccinale al server vaccinale contentente il numero di tessera sanitaria dell'utente, la data di inizio e fine validità del GP 
typedef struct {
    char ID[MAX_SIZE];
    START_DATE start_date;
    EXPIRE_DATE expire_date; 
} GP_REQUEST;

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

int main(int argc, char const *argv[]) {
    int listen_fd, connect_fd, package_size;
    struct sockaddr_in serv_addr;
    pid_t pid;
    GP_REQUEST package;

    //Creazione descrizione del socket
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }

    //Valorizzazione strutture
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); //INADDR_ANY: Viene utilizzato come indirizzo del server, l’applicazione accetterà connessioni da qualsiasi indirizzo associato al server.
    serv_addr.sin_port = htons(1025);

    //Assegnazione della porta al server
    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind() error");
        exit(1);
    }

    //Mette il socket in ascolto in attesa di nuove connessione
    if (listen(listen_fd, 1024) < 0) {
        perror("listen() error");
        exit(1);
    }

    for (;;) {

        //Accetta una nuova connessione
        if ((connect_fd = accept(listen_fd, (struct sockaddr *)NULL, NULL)) < 0) {
            perror("accept() error");
            exit(1);
        }

        //Creazione del figlio;
        if ((pid = fork()) < 0) {
            perror("fork() error");
            exit(1);
        }

        if (pid == 0) {
            close(listen_fd);

            if(full_read(connect_fd, &package_size, sizeof(int)) < 0) {
                perror("full_read error");
                exit(1);
            }
            if(full_read(connect_fd, &package, package_size) < 0) {
                perror("full_read error");
                exit(1);
            }

            printf("\nID: %s\n", package.ID);
            printf("Day: %d\n", package.expire_date.day);
            printf("Day: %d\n", package.start_date.day);

            close(connect_fd);
            exit(0);
        } else {
            close(connect_fd);
        }
    }
    exit(0);
}

