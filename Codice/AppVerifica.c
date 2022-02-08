#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>      // libreria standard del C che contiene definizioni di macro per la gestione delle situazioni di errore.
#include <string.h>
#include <netdb.h>      // contiene le definizioni per le operazioni del database di rete.
#include <sys/types.h>
#include <sys/socket.h> //contiene le definizioni dei socket.
#include <arpa/inet.h>  // contiene le definizioni per le operazioni Internet.
#define MAX_SIZE 1024
#define ACK_SIZE 64
#define WELCOME_SIZE 108
#define ASL_ACK 39
#define ID_SIZE 11 //10 byte per la tessera sanitaria più un byte per il terminatore

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


int main() {
    int socket_fd;
    struct sockaddr_in server_addr;
    char start_bit, report, buffer[MAX_SIZE], ID[ID_SIZE];

    start_bit = '0'; //Inizializziamo il bit a 0 per inviarlo al ServerVerifica

    //Creazione del descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }

    //Valorizzazione struttura
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1026);

    //Conversione dell’indirizzo IP, preso in input come stringa in formato dotted, in un indirizzo di rete in network order.
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("inet_pton() error");
        exit(1);
    }

    //Effettua connessione con il server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() error");
        exit(1);
    }

    //Invia un bit di valore 0 al ServerVerifica per informarlo che la comunicazione deve avvenire con l'AppVerifica
    if (full_write(socket_fd, &start_bit, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Riceve il benvenuto dal ServerVerifica
    if (full_read(socket_fd, buffer, WELCOME_SIZE) < 0) {
        perror("full_read() error");
        exit(1);
    }
    printf("%s\n\n", buffer);

    //Inserimento codice tessera sanitaria
    while (1) {
        printf("Inserisci codice tessera sanitaria [Massimo 10 caratteri]: ");
        if (fgets(buffer, MAX_SIZE, stdin) == NULL) {
            perror("fgets() error");
            exit(1);
        }
        if (strlen(buffer) != ID_SIZE) printf("Numero caratteri tessera sanitaria non corretto, devono essere esattamente 10! Riprovare\n\n");
        else {
            strcpy(ID, buffer);
            //Andiamo a inserire il terminatore al posto dell'invio inserito dalla fgets, poichè questo veniva contato ed inserito come carattere nella stringa
            ID[ID_SIZE - 1] = 0;
            break;
        }
    }

    //Invio del numero di tessera sanitaria da convalidare al server verifica
    if (full_write(socket_fd, ID, ID_SIZE)) {
        perror("full_write() error");
        exit(1);
    }

    //Ricezione dell'ack
    if (full_read(socket_fd, buffer, ACK_SIZE) < 0) {
        perror("full_read() error");
        exit(1);
    }
    printf("\n%s\n\n", buffer);

    //Facciamo attendere 3 secondi per completare l'operazione di verifica
    printf("Convalida in corso, attendere...\n\n");
    sleep(3);
    
    //Riceve esito scansione Green Pass dal ServerVerifica
    if (full_read(socket_fd, buffer, ASL_ACK) < 0) {
        perror("full_read() error");
        exit(1);
    }
    printf("%s\n", buffer);

    close(socket_fd);

    exit(0);
}
