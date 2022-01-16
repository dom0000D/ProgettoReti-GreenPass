#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#define MAX_SIZE 2048
#define ID_SIZE 11

//Struct contenente la data del giorno di inizio validità del green pass formato dai campi giorno, mese ed anno
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
    char ID[ID_SIZE];
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

//Funzione che salva i dati ricevuti dal centro vaccinale in un filesystem.
void save_GP(GP_REQUEST gp) {
    int fd;
    GP_REQUEST temp;
    char buffer[MAX_SIZE];

    //Per ogni Tessera Sanitaria crea un file contenente i dati ricevuti.
    if ((fd = open(gp.ID, O_RDWR| O_CREAT | O_TRUNC, 0777)) < 0) {
        perror("fopen() error");
        exit(1);
    }
    snprintf(buffer, MAX_SIZE, "%02d:%02d:%02d\n", gp.start_date.day, gp.start_date.month, gp.start_date.year);
    write(fd, buffer, strlen(buffer));
    snprintf(buffer, MAX_SIZE, "%02d:%02d:%02d\n", gp.expire_date.day, gp.expire_date.month, gp.expire_date.year);
    write(fd, buffer, strlen(buffer));
}

void SV_comunication(int connect_fd) {
    char ID[ID_SIZE], report = '1';

    //Riceve il numero di tessera sanitaria dal ServerVerifica
    if (full_read(connect_fd, ID, ID_SIZE) < 0) {
        perror("full_read() error");
        exit(1);
    }
    if (full_write(connect_fd, &report, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

}

void CV_comunication(int connect_fd) {
    GP_REQUEST gp;

    if (full_read(connect_fd, gp.ID, ID_SIZE) < 0) {
        perror("full_write() error");
        exit(1);
    }
    if (full_read(connect_fd, &gp.start_date, sizeof(START_DATE)) < 0) {
        perror("full_write() error");
        exit(1);
    }
    if (full_read(connect_fd, &gp.expire_date, sizeof(EXPIRE_DATE)) < 0) {
        perror("full_write() error");
        exit(1);
    }
    printf("Nuovo green pass ricevuto\n");

    save_GP(gp);
}

int main() {
    int listen_fd, connect_fd, package_size;
    struct sockaddr_in serv_addr;
    pid_t pid;
    char start_bit;

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

        printf("In attesa di nuovi dati\n");

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

        //Porzione di codice eseguita dal figlio
        if (pid == 0) {
            close(listen_fd);

            /*
                Il ServerVaccinale riceve un bit come primo messaggio, che può avere valore 0 o 1, siccome due connessioni differenti. 
                Quando riceve come bit 1 allora il figlio gestirà la connessione con il CentroVaccinale.
                Quando riceve come bit 0 allora il figlio gestirà la connessione con il ServerVerifica.
            */
            if (full_read(connect_fd, &start_bit, sizeof(char)) < 0) {
                perror("full_read() error");
                exit(1);
            }
            printf("start bit: %c\n", start_bit);
            if (start_bit == '1') CV_comunication(connect_fd);
            else if (start_bit == '0') SV_comunication(connect_fd);
            else printf("Client non riconosciuto\n");

            close(connect_fd);
            exit(0);
        } else close(connect_fd); //Porzione di codice eseguita dal padre
    }
    exit(0);
}

