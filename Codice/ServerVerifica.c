#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <netdb.h>
#define MAX_SIZE 1024
#define ID_SIZE 11

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

void verify_ID(char ID[]) {
    int socket_fd, welcome_size, package_size;
    struct sockaddr_in server_addr;
    char buffer[MAX_SIZE], report, start_bit;

    start_bit = '0';

    //Creazione del descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        exit(1);
    }

    //Valorizzazione struttura
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1025);

     //Conversione dell’indirizzo IP, preso in input come stringa in formato dotted, in un indirizzo di rete in network order.
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("inet_pton error");
        exit(1);
    }

    //Effettua connessione con il server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() error");
        exit(1);
    }

    if (full_write(socket_fd, &start_bit, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Invia il numero di tessera sanitaria ricevuto dall'AppVerifica al SeverVaccinale
    if (full_write(socket_fd, ID, ID_SIZE)) {
        perror("full_write() error");
        exit(1);
    }

    //Ricezione dell'esito della verifica dal ServerVaccinale, se 0 non valido se 1 valido
    if (full_read(socket_fd, &report, sizeof(char)) < 0) {
        perror("full_read() error");
        exit(1);
    }
    printf("Convalida in corso, attendere...\n");
    sleep(3);
    printf("%c\n\n", report);
    
    close(socket_fd);
}

//Funzione per la gestione della comunicazione con l'utente
void receive_ID(int connect_fd) {
    char buffer[MAX_SIZE], ID[ID_SIZE];
    int index, welcome_size, package_size;

    //Stampa un messaggo di benvenuto da inviare al clientS quando si collega ServerVerifica.
    snprintf(buffer, MAX_SIZE, "***Benvenuto nel server di verifica***\nInserisci il numero di tessera sanitaria per testarne la validità.\n");
    welcome_size = sizeof(buffer);
    if(full_write(connect_fd, buffer, welcome_size) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Riceve il numero di codice fiscale dal clientS
    if(full_read(connect_fd, ID, ID_SIZE) < 0) {
        perror("full_read error");
        exit(1);
    }

    //Funzione che invia il numero di tessera sanitaria al ServerVaccinale, riceve l'esito da questo e lo invia al clientS
    verify_ID(ID);
    
    //Notifica all'utente la corretta ricezione dei dati che aveva inviato.
    snprintf(buffer, MAX_SIZE, "\nIl numero di tessera sanitaria è stato correttamente ricevuto\n");
    welcome_size = sizeof(buffer);
    if(full_write(connect_fd, buffer, welcome_size) < 0) {
        perror("full_write() error");
        exit(1);
    }

    close(connect_fd);
}

int main() {
    int listen_fd, connect_fd;
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
    serv_addr.sin_port = htons(1026);

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

        printf("In attesa di nuove richieste di vaccinazioni\n");

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

            //Riceve informazioni dall'utente 
            receive_ID(connect_fd);
          
            close(connect_fd);
            exit(0);
        } else close(connect_fd);
    }
    exit(0);
}
