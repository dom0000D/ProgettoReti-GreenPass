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
#define ACK_SIZE 61

//Struct del pacchetto che il centro vaccinale deve ricevere dall'utente contentente nome, cognome e numero di tessera sanitaria dell'utente
typedef struct {
    char name[MAX_SIZE];
    char surname[MAX_SIZE];
    char ID[ID_SIZE];
} VAX_REQUEST;

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

//Funzione per calcolare la data di scadenza e la data di inizio validità del green pass
void create_expire_date(DATE *expire_date) {
    time_t ticks;
    ticks = time(NULL);
    
    //Dichiarazione strutture per la conversione della data da stringa ad intero
    struct tm *e_date = localtime(&ticks);
    e_date->tm_mon += 4;           //Sommiamo 4 perchè i mesi vanno da 0 ad 11
    e_date->tm_year += 1900;       //Sommiamo 1900 perchè gli anni partono dal 122 (2022 - 1900)
    
    //Effettuiamo il controllo nel caso in cui il vaccino sia stato fatto nel mese di ottobre, novembre o dicembre, comportando un aumento dell'anno
    if (e_date->tm_mon == 13) { //if 13 è ottobre quindi si incrementano 3 mesi, arrivando a gennaio dell'anno successivo
        e_date->tm_mon = 1;
        e_date->tm_year++;
    }
    if (e_date->tm_mon == 14) { //if 14 è novembre quindi si incrementano 3 mesi, arrivando a febbraio dell'anno successivo
        e_date->tm_mon = 2;
        e_date->tm_year++;
    }
    if (e_date->tm_mon == 15) { //if 15 è dicembre quindi si incrementano 3 mesi, arrivando a marzo dell'anno successivo
        e_date->tm_mon = 3;
        e_date->tm_year++;
    }

    printf("La data di scadenza del green pass e': %02d:%02d:%02d\n", e_date->tm_mday, e_date->tm_mon, e_date->tm_year);
    
    //Assegnamo i valori ai parametri di ritorno
    expire_date->day = e_date->tm_mday ;
    expire_date->month = e_date->tm_mon;
    expire_date->year = e_date->tm_year;
}

void create_start_date(DATE *start_date) {
    time_t ticks;
    ticks = time(NULL);

    //Dichiarazione strutture per la conversione della data da stringa ad intero
    struct tm *s_date = localtime(&ticks);
    s_date->tm_mon += 1;           //Sommiamo 1 perchè i mesi vanno da 0 ad 11
    s_date->tm_year += 1900;       //Sommiamo 1900 perchè gli anni partono dal 122 (2022 - 1900)

    printf("La data di inizio validità del green pass e': %02d:%02d:%02d\n", s_date->tm_mday, s_date->tm_mon, s_date->tm_year);

    //Assegnamo i valori ai parametri di ritorno
    start_date->day = s_date->tm_mday ;
    start_date->month = s_date->tm_mon;
    start_date->year = s_date->tm_year;
}

void send_GP(GP_REQUEST gp) {
    int socket_fd;
    struct sockaddr_in server_addr;
    char start_bit, buffer[MAX_SIZE];

    start_bit = '1';

    //Creazione del descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }

    //Valorizzazione struttura
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1025);

    //Conversione dell'indirizzo IP dal formato dotted decimal a stringa di bit
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("inet_pton() error");
        exit(1);
    }

    //Effettua connessione con il server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() error");
        exit(1);
    }

    //Invia un bit di valore 1 al ServerVaccinale per informarlo che la comunicazione deve avvenire con il CentroVaccinale
    if (full_write(socket_fd, &start_bit, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Inviamo il green pass al ServerVaccinale
    if (full_write(socket_fd, &gp, sizeof(gp)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    close(socket_fd);
}

//Funzione per la gestione della comunicazione con l'utente
void answer_user(int connect_fd) {
    char *hub_name[] = {"Milano", "Napoli", "Roma", "Torino", "Firenze", "Palermo", "Bari", "Catanzaro", "Bologna", "Udine"}; //Centri Vaccinali
    char buffer[MAX_SIZE];
    int index, welcome_size, package_size;
    VAX_REQUEST package;
    GP_REQUEST gp;

    //Scegliamo un centro vaccinale casuale
    srand(time(NULL));
    index = rand() % 10;
    
    //Stampa un messaggo di benvenuto da inviare all'utente quando si collega al centro vaccinale.
    snprintf(buffer, MAX_SIZE, "***Benvenuto nel centro vaccinale di %s***\nInserisci nome, cognome e numero di tessera sanitaria per inserirli sulla piattaforma.\n", hub_name[index]);
    welcome_size = sizeof(buffer);
    if(full_write(connect_fd, &welcome_size, sizeof(int)) < 0) {
        perror("full_write() error");
        exit(1);
    }
    if(full_write(connect_fd, buffer, welcome_size) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Riceviamo le informazioni per il GreenPass dall'Utente
    if(full_read(connect_fd, &package, sizeof(VAX_REQUEST)) < 0) {
        perror("full_read() error");
        exit(1);
    }

    printf("\nDati ricevuti\n");
    printf("Nome: %s\n", package.name);
    printf("Cognome: %s\n", package.surname);
    printf("Numero Tessera Sanitaria: %s\n\n", package.ID);

    //Notifica all'utente la corretta ricezione dei dati che aveva inviato.
    snprintf(buffer, ACK_SIZE, "I tuoi dati sono stati correttamente inseriti in piattaforma");
    if(full_write(connect_fd, buffer, ACK_SIZE) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Copio il numero di tessera sanitaria inviatoci dall'utente nel green pass da inviare al ServerVaccinale
    strcpy(gp.ID, package.ID);
    //Si ottiene la data di inizo validità del Green Pass
    create_start_date(&gp.start_date);
    //Crea la data di scadenza (3 mesi)
    create_expire_date(&gp.expire_date); 
    
    close(connect_fd);

    //Funzione per mandare il nuovo Green Pass al CentroVaccinale
    send_GP(gp);
}

int main(int argc, char const *argv[]) {
    int listen_fd, connect_fd;
    VAX_REQUEST package;
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
            answer_user(connect_fd);
          
            close(connect_fd);
            exit(0);
        } else close(connect_fd);
    }
    exit(0);
}
