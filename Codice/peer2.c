#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <sys/wait.h> //da eliminare
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
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

VAX_REQUEST answer_user(int connect_fd) {
    char *hub_name[] = {"Milano", "Napoli", "Roma", "Torino", "Firenze", "Palermo", "Bari", "Catanzaro", "Bologna", "Udine"};
    char buffer[MAX_SIZE];
    int index, welcome_size, package_size;
    VAX_REQUEST package;

    //Scegliamo un centro vaccinale casuale
    srand(time(NULL));
    index = rand() % 10;

    snprintf(buffer, MAX_SIZE, "***Benvenuto nel centro vaccinale di %s***\nInserisci nome, cognome e numero di tessera sanitaria per inserirli sulla piattaforma.\n", hub_name[index]);
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

    printf("\nNome: %s\n", package.name);
    printf("Cognome: %s\n", package.surname);
    printf("Numero Tessera Sanitaria: %s\n", package.ID);

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
    exit(0);
}

//Funzione per calcolare la data di scadenza e la data di inizio validità del green pass
void create_expire(EXPIRE_DATE *expire_date, START_DATE *start_date) {
    time_t ticks;
    ticks = time(NULL);

    //Dichiarazione strutture per la conversione della data da stringa ad intero
    struct tm *e_date = localtime(&ticks);
    struct tm *s_date = localtime(&ticks);
    e_date->tm_mon += 4;           //Sommiamo 4 perchè i mesi vanno da 0 ad 11
    e_date->tm_year += 1900;       //Sommiamo 1900 perchè gli anni partono dal 122 (2022 - 1900)

    //Effettuiamo il controllo nel caso in cui il vaccino sia stato fatto nel mese di ottobre, novembre o dicembre, comportando un aumento dell'anno
    if (e_date->tm_mon == 13) {
        e_date->tm_mon = 1;
        e_date->tm_year++;
    }
    if (e_date->tm_mon == 14) {
        e_date->tm_mon = 2;
        e_date->tm_year++;
    }
    if (e_date->tm_mon == 15) {
        e_date->tm_mon = 3;
        e_date->tm_year++;
    }
    printf("La data di scadenza del green pass e': %02d:%02d:%02d\n", e_date->tm_mday, e_date->tm_mon, e_date->tm_year);

    //Assegnamo i valori ai parametri di ritorno
    e_date->tm_mday = expire_date->day;
    e_date->tm_mon = expire_date->month;
    e_date->tm_year= expire_date->year;
    s_date->tm_mday = start_date->day;
    s_date->tm_mon = start_date->month;
    s_date->tm_year= start_date->year;
}

void send_GP() {
    int socket_fd;
    struct sockaddr_in server_addr;
    char **alias;
    char *addr;
	struct hostent *data;
    char buffer[MAX_SIZE];

    //Creazione del descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        exit(1);
    }

    //Valorizzazione struttura
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1025);

    //Conversione dell'indirizzo IP dal formato dotted decimal a stringa di bit
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("inet_pton error");
        exit(1);
    }

    //Effettua connessione con il server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect error");
        exit(1);
    }
}

void receiving(int listen_fd) {
    int connect_fd, buffer_size;
    char buffer[MAX_SIZE];
    struct sockaddr_in client_addr;
    pid_t pid;
    socklen_t len;
    VAX_REQUEST package;
    EXPIRE_DATE expire_date;
    START_DATE start_date;

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

            package = answer_user(connect_fd);

            //Crea la data di scadenza (3 mesi)
            create_expire(&expire_date, &start_date);

            close(connect_fd);
            exit(0);
        } else {
            close(connect_fd);
        }
    }
    exit(0);
}


void sending() {
    send_GP();
}

int main(int argc, char const *argv[]) {
    int listen_fd, connect_fd, buffer_size;
    struct sockaddr_in server_address;
    pid_t pid;
    char buffer[MAX_SIZE];

    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket error");
        exit(1);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(1024);
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listen_fd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("bind error");
        exit(1);
    }

    if (listen(listen_fd, 5) < 0) {
        perror("listen error");
        exit(1);
    }

    //Creazione del figlio;
    if ((pid = fork()) < 0) {
        perror("fork() error");
        exit(1);
    }

    if (pid == 0) receiving(listen_fd);
    else {
        sending();
    }

    exit(0);
}
