#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>      // libreria standard del C che contiene definizioni di macro per la gestione delle situazioni di errore.
#include <string.h>
#include <fcntl.h>      // contiene opzioni di controllo dei file
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/socket.h> //contiene le definizioni dei socket.
#include <arpa/inet.h>  // contiene le definizioni per le operazioni Internet.
#include <time.h>
#include <signal.h>     //consente l'utilizzo delle funzioni per la gestione dei segnali fra processi.
#define MAX_SIZE 2048   //dim massima del buffer
#define ID_SIZE 11        //dim del codice della tessera sanitaria (10 byte + 1 byte del terminatore)

//Struct del pacchetto dell'ASL contenente il numero di tessera sanitaria di un green pass ed il suo referto di validità
typedef struct  {
    char ID[ID_SIZE];
    char report;
} REPORT;

//Struct che permette di salvare una data, formata dai campi: giorno, mese ed anno
typedef struct {
    int day;
    int month;
    int year;
} DATE;

//Struct del pacchetto inviato dal centro vaccinale al server vaccinale contentente il numero di tessera sanitaria dell'utente, la data di inizio e fine validità del GP
typedef struct {
    char ID[ID_SIZE];
    char report; //0 green pass non valido, 1 green pass valido
    DATE start_date;
    DATE expire_date;
} GP_REQUEST;

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
        sleep(2); //attende 2 secondi prima della prossima operazione
        printf("***Grazie per aver utilizzato il nostro servizio***\n");
        exit(0);
    }
}

//Funzione che invia un GP richiesto dal ServerVerifica
void send_gp(int connect_fd) {
    char report, ID[ID_SIZE];
    int fd;
    GP_REQUEST gp;

    //Riceve il numero di tessera sanitaria dal ServerVerifica
    if (full_read(connect_fd, ID, ID_SIZE) < 0) {
        perror("full_read() error");
        exit(1);
    }
    //Apre il file rinominato "ID", cioè il codice ricevuto dal ServerVerifica
    fd = open(ID, O_RDONLY, 0777);
    /*
        Se il numero di tessera sanitaria inviato dall'AppVerifca non esiste la variabile globale errno cattura l'evento ed in quel caso
        invia un report uguale ad 1 al ServerVerifica, che a sua volta aggiornerà l'AppVerifica dell'inesistenza del codice. In caso
        contrario invierà un report uguale a 0 per indicare che l'operazione è avvenuta correttamente.
    */
    if (errno == 2) {
        printf("Numero tessera sanitaria non esistente, riprovare...\n");
        report = '2';
        //Invia il report al ServerVerifica
        if (full_write(connect_fd, &report, sizeof(char)) < 0) {
            perror("full_write() error");
            exit(1);
        }
    } else {
        //Accediamo in maniera mutuamente esclusiva al file in lettura
        if (flock(fd, LOCK_EX) < 0) {
            perror("flock() error");
            exit(1);
        }
        //Lettura del GP dal file aperto in precedenza
        if (read(fd, &gp, sizeof(GP_REQUEST)) < 0) {
            perror("read() error");
            exit(1);
        }
        //Sblocchiamo il lock
        if(flock(fd, LOCK_UN) < 0) {
            perror("flock() error");
            exit(1);
        }

        close(fd);
        report = '1';
        //Invia il report al ServerVerifica
        if (full_write(connect_fd, &report, sizeof(char)) < 0) {
            perror("full_write() error");
            exit(1);
        }

        //Mandiamo il green pass richiesto al ServerVerifica che controllerà la sua validità
        if(full_write(connect_fd, &gp, sizeof(GP_REQUEST)) < 0) {
            perror("full_write() error");
            exit(1);
        }
    }
}
//Funzione che modifica il report di un GP, sotto richiesta dell'ASL
void modify_report(int connect_fd) {
    REPORT package;
    GP_REQUEST gp;
    int fd;
    char report;

    //Riceve il pacchetto dal ServerVerifica proveniente dall'ASL contenente numero di tessera sanitaria ed il referto del tampone
    if (full_read(connect_fd, &package, sizeof(REPORT)) < 0) {
        perror("full_read() error");
        exit(1);
    }

    //Apre il file contenente il green pass relativo al numero di tessera ricevuto dall'ASL
    fd = open(package.ID, O_RDWR , 0777);

    /*
        Se il numero di tessera sanitaria inviato dall'ASL non esiste la variabile globale errno cattura l'evento ed in quel caso 
        invia un report uguale ad 1 al ServerVerifica, che a sua volta aggiornerà l'ASL dell'inesistenza del codice. In caso contrario
        invierà un report uguale a 0 per indicare che l'operazione è avvenuta correttamente.
    */
    if (errno == 2) {
        printf("Numero tessera sanitaria non esistente, riprovare...\n");
        report = '1';
    } else {
        //Accediamo in maniera mutuamente esclusiva al file in lettura
        if(flock(fd, LOCK_EX | LOCK_NB) < 0) {
            perror("flock() error");
            exit(1);
        }
        //Legge il file aperto contenente il green pass relativo al numero di tessera ricevuto dall'ASL
        if (read(fd, &gp, sizeof(GP_REQUEST)) < 0) {
            perror("read() error");
            exit(1);
        }

        //Assegna il referto ricevuto dall'ASL al green pass
        gp.report = package.report;

        //Ci posizioniamo all'inizio dello stream del file
        lseek(fd, 0, SEEK_SET);

        //Andiamo a sovrascrivere i campi di GP nel file binario con nome il numero di tessera sanitaria del green pass
        if (write(fd, &gp, sizeof(GP_REQUEST)) < 0) {
            perror("write() error");
            exit(1);
        }
        //Sblocchiamo il lock
        if(flock(fd, LOCK_UN) < 0) {
            perror("flock() error");
            exit(1);
        }
        report = '0';
    }
    //Invia il report al ServerVerifica
    if (full_write(connect_fd, &report, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }
}


  /*
    Funzione che tratta la comunicazione con il ServerVerifica, ricava il green pass dal file system relativo al numero di tessera
    sanitaria ricevuto e lo invia al ServerVerifica.
  */
void SV_comunication(int connect_fd) {
    char start_bit;

    /*
        Il ServerVaccinale riceve un bit dal ServerVerifica, che può avere valore 0 o 1, siccome sono due funzioni differenti.
        Quando riceve come bit 0 allora il ServerVaccinale gestirà la funzione per modificare il report di un green pass.
        Quando riceve come bit 1 allora il ServerVaccinale gestirà la funzione per inviare un green pass al ServerVerifica.
    */
    if (full_read(connect_fd, &start_bit, sizeof(char)) < 0) {
        perror("full_read() error");
        exit(1);
    }
    if (start_bit == '0') modify_report(connect_fd);
    else if (start_bit == '1') send_gp(connect_fd);
    else printf("Dato non valido\n\n");
}

//Funzione che tratta la comunicazione con il CentroVaccinale e salva i dati ricevuti da questo in un filesystem.
void CV_comunication(int connect_fd) {
    int fd;
    GP_REQUEST gp;

    //Ricevo il green pass dal CentroVaccinale
    if (full_read(connect_fd, &gp, sizeof(GP_REQUEST)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Quando viene generato un nuovo green pass è valido di defualt
    gp.report = '1';

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

    //Mette il socket in ascolto in attesa di nuove connessioni
    if (listen(listen_fd, 1024) < 0) {
        perror("listen() error");
        exit(1);
    }

    for (;;) {

    printf("In attesa di nuovi dati\n\n");

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
                Il ServerVaccinale riceve un bit come primo messaggio, che può avere valore 0 o 1, siccome ci sono due connessioni differenti.
                Quando riceve come bit 1 allora il figlio gestirà la connessione con il CentroVaccinale.
                Quando riceve come bit 0 allora il figlio gestirà la connessione con il ServerVerifica.
            */
            if (full_read(connect_fd, &start_bit, sizeof(char)) < 0) {
                perror("full_read() error");
                exit(1);
            }
            if (start_bit == '1') CV_comunication(connect_fd);
            else if (start_bit == '0') SV_comunication(connect_fd);
            else printf("Client non riconosciuto\n\n");

            close(connect_fd);
            exit(0);
        } else close(connect_fd); //Porzione di codice eseguita dal padre
    }
    exit(0);
}
