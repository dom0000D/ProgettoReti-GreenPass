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

//Definiamo il pacchetto applicazione per l'user da inviare al client centro vaccinale
typedef struct {
    char name[MAX_SIZE];
    char surname[MAX_SIZE];
    char ID[ID_SIZE];
} VAX_REQUEST;

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

//Funzione per la creazione del pacchetto da inviare al centro vaccinale
VAX_REQUEST create_package() {
    char buffer[MAX_SIZE];
    VAX_REQUEST temp;

    //Inserimento nome
    printf("Inserisci nome: ");
    if (fgets(temp.name, MAX_SIZE, stdin) == NULL) {
        perror("fgets() error");
    }
    //Andiamo a inserire il terminatore al posto dell'invio inserito dalla fgets, poichè questo veniva contato ed inserito come carattere nella stringa
    temp.name[strlen(temp.name) - 1] = 0;

    //Inserimento cognome
    printf("Inserisci cognome: ");
    if (fgets(temp.surname, MAX_SIZE, stdin) == NULL) {
        perror("fgets() error");
    }
    //Andiamo a inserire il terminatore al posto dell'invio inserito dalla fgets, poichè questo veniva contato ed inserito come carattere nella stringa
    temp.surname[strlen(temp.surname) - 1] = 0;

    //Inserimento codice tessera sanitaria
    while (1) {
        printf("Inserisci codice tessera sanitaria [Massimo 10 caratteri]: ");
        if (fgets(buffer, MAX_SIZE, stdin) == NULL) {
            perror("fgets() error");
            exit(1);
        }
        if (strlen(buffer) != ID_SIZE) printf("Numero caratteri tessera sanitaria non corretto, devono essere esattamente 10! Riprovare\n\n");
        else {
            strcpy(temp.ID, buffer);
            //Andiamo a inserire il terminatore al posto dell'invio inserito dalla fgets, poichè questo veniva contato ed inserito come carattere nella stringa
            temp.ID[ID_SIZE - 1] = 0;
            break;
        }
    }

    return temp;
}

int main(int argc, char **argv) {
    int socket_fd, welcome_size, package_size;
    struct sockaddr_in server_addr;
    VAX_REQUEST package;
    char buffer[MAX_SIZE];
    char **alias;
    char *addr;
	struct hostent *data;

    if (argc != 2) {
        perror("usage: <host name>"); //perror: Produce un messaggio sullo standard error che descrive l’ultimo errore avvenuto durante una System call o una funzione di libreria.
        exit(1);
    }

    //Creazione del descrittore del socket
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket() error");
        exit(1);
    }

    //Valorizzazione struttura
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1024);

    //Conversione dal nome al dominio a indirizzo IP
    if ((data = gethostbyname(argv[1])) == NULL) {
        herror("gethostbyname() error");
		exit(1);
    }
	alias = data -> h_addr_list;

    //inet_ntop converte un indirizzo in una stringa:
    if ((addr = (char *)inet_ntop(data -> h_addrtype, *alias, buffer, sizeof(buffer))) < 0) {
        perror("inet_ntop() error");
        exit(1);
    }

    //Conversione dell’indirizzo IP, preso in input come stringa in formato dotted, in un indirizzo di rete in network order.
    if (inet_pton(AF_INET, addr, &server_addr.sin_addr) <= 0) {
        perror("inet_pton() error");
        exit(1);
    }

    //Effettua connessione con il server
    if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect() error");
        exit(1);
    }

    //Riceve il benevenuto dal centro vaccinale
    if (full_read(socket_fd, &welcome_size, sizeof(int)) < 0) {
        perror("full_read() error");
        exit(1);
    }
    if (full_read(socket_fd, buffer, welcome_size) < 0) {
        perror("full_read() error");
        exit(1);
    }
    printf("%s\n", buffer);

    //Creazione del pacchetto da inviare al centro vaccinale
    package = create_package();

    //Invio del pacchetto richiesto al centro vaccinale
    if (full_write(socket_fd, &package, sizeof(package)) < 0) {
        perror("full_write() error");
        exit(1);
    }

    //Ricezione dell'ack
    if (full_read(socket_fd, buffer, ACK_SIZE) < 0) {
        perror("full_read() error");
        exit(1);
    }
    printf("%s\n\n", buffer);

    exit(0);
}
