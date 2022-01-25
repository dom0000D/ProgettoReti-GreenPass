#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define MAX_SIZE 1024
#define ID_SIZE 11
#define ACK_SIZE 61

//Struct del pacchetto dell'ASL contenente il numero di tessera sanitaria di un green pass ed il suo referto di validità
typedef struct  {
    char ID[ID_SIZE];
    char report;
} REPORT;

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

int main(int argc, char **argv) {
    int socket_fd;
    struct sockaddr_in server_addr;
    REPORT package;
    char start_bit, buffer[MAX_SIZE];

    start_bit = '1';

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

    //Invia un bit di valore 1 al ServerVerifica per informarlo che la comunicazione deve avvenire con l'ASL
    if (full_write(socket_fd, &start_bit, sizeof(char)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    printf("*** ASL ***\n");
    printf("Immettere un numero di tessera sanitaria ed il referto di un tampone per invalidare o ripristinare un Green Pass\n");
    //Inserimento codice tessera sanitaria
    while (1) {
        printf("Inserisci codice tessera sanitaria [Massimo 10 caratteri]: ");
        if (fgets(buffer, MAX_SIZE, stdin) == NULL) {
            perror("fgets() error");
            exit(1);
        }
        if (strlen(buffer) != ID_SIZE) printf("Numero caratteri tessera sanitaria non corretto, devono essere esattamente 10! Riprovare\n\n");
        else {
            strcpy(package.ID, buffer);
            //Andiamo a inserire il terminatore al posto dell'invio inserito dalla fgets, poichè questo veniva contato ed inserito come carattere nella stringa
            package.ID[ID_SIZE - 1] = 0;
            break;
        }
    }

    while (1) {
        printf("Inserire 0 [Green Pass Valido]\n Inserire 1 [Green Pass Non Valido]: ");
        scanf("%c", &package.report);
        if (package.report == '1' || package.report == '0') break;
        printf("Errore: input errato, riprovare...\n\n");
    }

    //Invia pacchetto report al ServerVerifica
    if (full_write(socket_fd, &package, sizeof(REPORT)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    exit(0);
}
