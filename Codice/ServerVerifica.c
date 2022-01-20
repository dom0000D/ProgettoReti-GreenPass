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
#define WELCOME_SIZE 108
#define ID_SIZE 11
#define ACK_SIZE 64

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

void create_current_date(DATE *start_date) {
    time_t ticks;
    ticks = time(NULL);

    //Dichiarazione strutture per la conversione della data da stringa ad intero
    struct tm *s_date = localtime(&ticks);
    s_date->tm_mon += 1;           //Sommiamo 1 perchè i mesi vanno da 0 ad 11
    s_date->tm_year += 1900;       //Sommiamo 1900 perchè gli anni partono dal 122 (2022 - 1900)

    //Assegnamo i valori ai parametri di ritorno
    start_date->day = s_date->tm_mday ;
    start_date->month = s_date->tm_mon;
    start_date->year = s_date->tm_year;
}

char verify_ID(char ID[]) {
    int socket_fd, welcome_size, package_size;
    struct sockaddr_in server_addr;
    char buffer[MAX_SIZE], report, start_bit;
    GP_REQUEST gp;
    DATE current_date;

    //Inizializziamo il report di convalida ad 1 ovvero il caso in cui un green pass sia valido
    report = '1';

    //Valorizziamo start_bit a 0 per far capire al ServerVaccinale che la comunicazione è con il ServerVerifica
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

    //Invia un bit di valore 0 al ServerVaccinale per informarlo che la comunicazione deve avvenire con il ServerVerifica
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
    if (full_read(socket_fd, &gp, sizeof(GP_REQUEST)) < 0) {
        perror("full_read() error");
        exit(1);
    }

    close(socket_fd);

    //Funzione per ricavare la data corrente
    create_current_date(&current_date);

    /*
        Se l'anno corrente è maggiore dell'anno di scadenza del green pass, questo non è valido e assegnamo a report il valore di 0
        Se l'anno della scadenza del green pass è valido MA il mese corrente è maggiore del mese di scadenza del green pass, 
        questo non è valido e assegnamo a report il valore di 0
        Se l'anno e il mese della scadenza del green pass è valido MA il giorno corrente è maggiore del giorno di scadenza del green 
        pass, questo non è valido e assegnamo a report il valore di 0
    */
    if (current_date.year > gp.expire_date.year) report = '0';
    if (report == '1' && current_date.month > gp.expire_date.month) report = '0';
    if (report == '1' && current_date.day > gp.expire_date.day) report = '0';

    return report;
}

//Funzione per la gestione della comunicazione con l'utente
void receive_ID(int connect_fd) {
    char report, buffer[MAX_SIZE], ID[ID_SIZE];
    int index, welcome_size, package_size;

    //Stampa un messaggo di benvenuto da inviare al clientS quando si collega ServerVerifica.
    snprintf(buffer, WELCOME_SIZE, "***Benvenuto nel server di verifica***\nInserisci il numero di tessera sanitaria per testarne la validità.");
    buffer[WELCOME_SIZE - 1] = 0;
    if(full_write(connect_fd, buffer, WELCOME_SIZE) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Riceve il numero di codice fiscale dal clientS
    if(full_read(connect_fd, ID, ID_SIZE) < 0) {
        perror("full_read error");
        exit(1);
    }
    
    //Notifica all'utente la corretta ricezione dei dati che aveva inviato.
    snprintf(buffer, ACK_SIZE, "Il numero di tessera sanitaria è stato correttamente ricevuto");
    buffer[ACK_SIZE - 1] = 0;
    if(full_write(connect_fd, buffer, ACK_SIZE) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Funzione che invia il numero di tessera sanitaria al ServerVaccinale, riceve l'esito da questo e lo invia al clientS
    report = verify_ID(ID);


    //Invia il report di validità del green pass all'App di verifica
    if (full_write(connect_fd, &report, sizeof(char)) < 0) {
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

        printf("In attesa di nuove richieste di vaccinazione\n");

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
