#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>      // libreria standard del C che contiene definizioni di macro per la gestione delle situazioni di errore.
#include <string.h>
#include <netdb.h>      // contiene le definizioni per le operazioni del database di rete.
#include <sys/types.h>
#include <sys/socket.h> //contiene le definizioni dei socket.
#include <arpa/inet.h>  // contiene le definizioni per le operazioni Internet.
#include <time.h>
#include <signal.h>     //consente l'utilizzo delle funzioni per la gestione dei segnali fra processi.

#define MAX_SIZE 1024  //dim massima del buffer
#define WELCOME_SIZE 108
#define ID_SIZE 11    //dim della tessera sanitaria
#define ACK_SIZE 64
#define ASL_ACK 39

//Struct che permette di salvare una data, formata dai campi: giorno, mese ed anno
typedef struct {
    int day;
    int month;
    int year;
} DATE;

//Struct del pacchetto inviato dal centro vaccinale al server vaccinale contentente il numero di tessera sanitaria dell'utente, la data di inizio e fine validità del GP
typedef struct {
    char ID[ID_SIZE];
    char report;
    DATE start_date;
    DATE expire_date;
} GP_REQUEST;

//Struct del pacchetto inviato dall'ASL contenente il numero di tessera sanitaria di un green pass ed il suo referto di validità
typedef struct  {
    char ID[ID_SIZE];
    char report;
} REPORT;

//Legge esattamente count byte s iterando opportunamente le letture. Legge anche se viene interrotta da una System Call.
ssize_t full_read(int fd, void *buffer, size_t count) {
    size_t n_left;
    ssize_t n_read;
    n_left = count;
    while (n_left > 0) {  // repeat finchè non ci sono left
        if ((n_read = read(fd, buffer, n_left)) < 0) {
            if (errno == EINTR) continue; // Se si verifica una System Call che interrompe ripeti il ciclo
            else exit(n_read);
        } else if (n_read == 0) break; // Se sono finiti, esci
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
    while (n_left > 0) {          //repeat finchè non ci sono left
        if ((n_written = write(fd, buffer, n_left)) < 0) {
            if (errno == EINTR) continue; //Se si verifica una System Call che interrompe ripeti il ciclo
            else exit(n_written); //Se non è una System Call, esci con un errore
        }
        n_left -= n_written;
        buffer += n_written;
    }
    buffer = 0;
    return n_left;
}

//Handler che cattura il segnale CTRL-C e stampa un messaggio di arrivederci.
void handler (int sign){
    if (sign == SIGINT) {
        printf("\nUscita in corso...\n");
        sleep(2);
        printf("***Grazie per aver utilizzato il nostro servizio di verifica GP***\n");

        exit(0);
    }
}

//Funzione per estrapolare la data corrente del sistema, verrà usata per fare le operazioni di verifica GP
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

 /* Funzione usata per la scansione del certificato verde. Riceve un numero di tessera sanitaria
  dall'App Verifica, chiede al ServerVaccinale il report e dopo aver
   fatto delle procedure di verifica, comunica l'esito all'App Verifica*/
char verify_ID(char ID[]) {
    int socket_fd, welcome_size, package_size;
    struct sockaddr_in server_addr;
    char buffer[MAX_SIZE], report, start_bit;
    GP_REQUEST gp;
    DATE current_date;

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

    start_bit = '1';

    //Invia un bit di valore 1 al ServerVaccinale per informarlo che deve verificare il green pass
    if (full_write(socket_fd, &start_bit, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Invia il numero di tessera sanitaria ricevuto dall'AppVerifica al SeverVaccinale
    if (full_write(socket_fd, ID, ID_SIZE)) {
        perror("full_write() error");
        exit(1);
    }

    //Riceve report dal ServerVaccinale
    if (full_read(socket_fd, &report, sizeof(char)) < 0) {
        perror("full_read() error");
        exit(1);
    }

    if (report == '1') {
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
        if (report == '1' && gp.report == '0') report = '0'; //Se il Green Pass è valido temporalmente MA il report (esito del tampone) è negativo, allora il GP non è valido
    }

    return report;
}

//Funzione per la gestione della comunicazione con l'utente
void receive_ID(int connect_fd) {
    char report, buffer[MAX_SIZE], ID[ID_SIZE];
    int index, welcome_size, package_size;

    //Stampa un messaggo di benvenuto da inviare all'AppVerifica quando si collega ServerVerifica.
    snprintf(buffer, WELCOME_SIZE, "***Benvenuto nel server di verifica***\nInserisci il numero di tessera sanitaria per testarne la validità.");
    buffer[WELCOME_SIZE - 1] = 0;
    if(full_write(connect_fd, buffer, WELCOME_SIZE) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Riceve il numero di codice fiscale dall'AppVerica
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
    if (report == '1') {
        strcpy(buffer, "Green Pass valido, operazione conclusa");
        if(full_write(connect_fd, buffer, ASL_ACK) < 0) {
            perror("full_write() error");
            exit(1);
        }
    } else if (report == '0') {
        strcpy(buffer, "Green Pass non valido, uscita in corso");
        if(full_write(connect_fd, buffer, ASL_ACK) < 0) {
            perror("full_write() error");
            exit(1);
        }
    } else {
        strcpy(buffer, "Numero tessera sanitaria non esistente");
        if(full_write(connect_fd, buffer, ASL_ACK) < 0) {
            perror("full_write() error");
            exit(1);
        }
    }

    close(connect_fd);
}

char send_report(REPORT package) {
    int socket_fd;
    struct sockaddr_in server_addr;
    char start_bit, buffer[MAX_SIZE], report;

    start_bit = '0';

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

    //Invia un bit di valore 0 al ServerVaccinale per informarlo che deve comunicare con il ServerVaccinale
    if (full_write(socket_fd, &start_bit, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Invia un bit di valore 0 al ServerVaccinale per informarlo che deve modificare il report del green pass
    if (full_write(socket_fd, &start_bit, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Invia il pacchetto appena ricevuto dall'ASL al ServerVaccinale
    if (full_write(socket_fd, &package, sizeof(REPORT)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Riceve il report dal ServerVerifica
    if (full_read(socket_fd, &report, sizeof(report)) < 0) {
        perror("full_read() error");
        exit(1);
    }

    close(socket_fd);

    return report;
}

void receive_report(int connect_fd) {
    REPORT package;
    char report, buffer[MAX_SIZE];


    //Legge i dati del pacchetto REPORT inviato dall'ASL
    if (full_read(connect_fd, &package, sizeof(REPORT)) < 0) {
        perror("full_read() error");
        exit(1);
    }

    report = send_report(package);

    if (report == '1') {
        strcpy(buffer, "Numero tessera sanitaria non esistente");
        if(full_write(connect_fd, buffer, ASL_ACK) < 0) {
            perror("full_write() error");
            exit(1);
        }
    } else {
        strcpy(buffer, "***Operazione avvenuta con successo***");
        if(full_write(connect_fd, buffer, ASL_ACK) < 0) {
            perror("full_write() error");
            exit(1);
        }
    }
}

int main() {
    int listen_fd, connect_fd;
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
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(1026);

    //Assegnazione della porta al server
    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind() error");
        exit(1);
    }

    //Mette il socket in ascolto in attesa di nuove connessioni
    if (listen(listen_fd, 1024) < 0) {
        perror("listen() error");
        exit(1);
    }

    for (;;) {
    printf("In attesa di Green Pass da scansionare\n");


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

            /*
                Il ServerVerifica riceve un bit come primo messaggio, che può avere valore 0 o 1, siccome abbiamo due connessioni differenti.
                Quando riceve come bit 1 allora il figlio gestirà la connessione con l'ASL.
                Quando riceve come bit 0 allora il figlio gestirà la connessione con l'AppVerifica.
            */
            if (full_read(connect_fd, &start_bit, sizeof(char)) < 0) {
                perror("full_read() error");
                exit(1);
            }
            if (start_bit == '1') receive_report(connect_fd);   //Riceve informazioni dall'ASL
            else if (start_bit == '0') receive_ID(connect_fd);  //Riceve informazioni dall'AppVerifica
            else printf("Client non riconosciuto\n");

            close(connect_fd);
            exit(0);
        } else close(connect_fd);
    }
    exit(0);
}
