#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#define MAX_SIZE 100

typedef struct {
    char name[MAX_SIZE];
    char surname[MAX_SIZE];
    char ID[MAX_SIZE];
} VAX_REQUEST; 

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
    int listen_fd, connect_fd, welcome_size, package_size, index;
    VAX_REQUEST package;
    char buffer[MAX_SIZE];
    char *hub_name[] = {"Milano", "Napoli", "Roma", "Torino", "Firenze", "Palermo", "Bari", "Catanzaro", "Bologna", "Udine"};
    struct sockaddr_in serv_addr;
    pid_t pid;

    //Creazione descrizione del socket
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }

    //Valorizzazione strutture
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(1024);

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
            
            //Scegliamo un centro vaccinale casuale
            srand(time(NULL));
            index = rand() % 10;

            snprintf(buffer, MAX_SIZE, "Benvenuto nel centro vaccinale %s\nInserire nome, cognome, numero di tessera sanitaria\n", hub_name[index]);
            welcome_size = sizeof(buffer);
            if(full_write(connect_fd, &welcome_size, sizeof(int)) < 0) {
                perror("full_write error");
                exit(1);
            }
            if(full_write(connect_fd, buffer, welcome_size) < 0) {
                perror("full_write error");
                exit(1);
            }

            if(full_read(connect_fd, &package_size, sizeof(int)) < 0) {
                perror("full_read error");
                exit(1);
            }
            if(full_read(connect_fd, &package, package_size) < 0) {
                perror("full_read error");
                exit(1);
            }

            printf("\nnome: %s\n", package.name);
            printf("cognome: %s\n", package.surname);
            printf("numero tessera sanitaria: %s\n", package.ID);

            snprintf(buffer, MAX_SIZE, "\nI tuoi dati sono stati correttamente inseriti in piattaforma\n");
            welcome_size = sizeof(buffer);
            if(full_write(connect_fd, &welcome_size, sizeof(int)) < 0) {
                perror("full_write error");
                exit(1);
            }
            if(full_write(connect_fd, buffer, welcome_size) < 0) {
                perror("full_write error");
                exit(1);
            }

            close(connect_fd);
            exit(0);
        } else {
            close(connect_fd);
        }
    }    
    exit(0);
}
