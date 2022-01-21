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
#include <signal.h>
#define MAX_SIZE 2048
#define ID_SIZE 11

//Handler che cattura il segnale CTRL-C e stampa un messaggio di arrivederci.
void handler (int sign){
if (sign==SIGINT) {
  printf("\nUscita in corso...\n");
  sleep (2);
  printf("***Grazie per aver utilizzato il nostro servizio***\n");

  exit(0);
  }
}
//Struct che permette di salvare una data, formata dai campi: giorno, mese ed anno
typedef struct {
    int day;
    int month;
    int year;
} DATE;

//Struct del pacchetto inviato dal centro vaccinale al server vaccinale contentente il numero di tessera sanitaria dell'utente, la data di inizio e fine validità del GP
typedef struct {
    char ID[ID_SIZE];
    DATE start_date;
    DATE expire_date;
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

/*
    Funzione che tratta la comunicazione con il ServerVerifica, ricava il green pass nel file system relativo al numero di tessera
    sanitaria ricevuto e lo invia al ServerVerifica.
*/
void SV_comunication(int connect_fd) {
    char ID[ID_SIZE], report = '1';
    int fd;
    GP_REQUEST gp;

    //Riceve il numero di tessera sanitaria dal ServerVerifica
    if (full_read(connect_fd, ID, ID_SIZE) < 0) {
        perror("full_read() error");
        exit(1);
    }

    if (((fd = open(ID, O_RDONLY, 0777))) < 0) {
        perror("open() error");
        exit(1);
    }
    if (read(fd, &gp, sizeof(GP_REQUEST)) < 0) {
        perror("read() error");
        exit(1);
    }
    close(fd);

    //Mandiamo il green pass richiesto al ServerVerifica che controllerà la sua validità
    if(full_write(connect_fd, &gp, sizeof(GP_REQUEST)) < 0) {
        perror("full_write() error");
        exit(1);
    }
}

//Funzione che tratta la conunicazione con il CentroVaccinale e salva i dati ricevuti da questo in un filesystem.
void CV_comunication(int connect_fd) {
    GP_REQUEST gp;

    //Ricevo il green pass dal CentroVaccinale
    if (full_read(connect_fd, &gp, sizeof(GP_REQUEST)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    int fd;
    GP_REQUEST temp;
    char buffer[MAX_SIZE];

    //Per ogni Tessera Sanitaria crea un file contenente i dati ricevuti.
    if ((fd = open(gp.ID, O_WRONLY| O_CREAT | O_TRUNC, 0777)) < 0) {
        perror("open() error");
        exit(1);
    }
    //Andiamo a scrivere i campi di GP nel file binario con nome il numero di tessera sanitaria del green pass
    if (write(fd, &gp, sizeof(GP_REQUEST)) < 0) {
        perror("write() error");
        exit(1);
    }

    close(fd);
}

int main() {
    int listen_fd, connect_fd, package_size;
    struct sockaddr_in serv_addr;
    pid_t pid;
    char start_bit;
    signal(SIGINT,handler); //Cattura il segnale CTRL-C
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
            if (start_bit == '1') CV_comunication(connect_fd);
            else if (start_bit == '0') SV_comunication(connect_fd);
            else printf("Client non riconosciuto\n");

            close(connect_fd);
            exit(0);
        } else close(connect_fd); //Porzione di codice eseguita dal padre
    }
    exit(0);
}
