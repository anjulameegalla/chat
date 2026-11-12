#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define PORT 57644
#define BUFFER_SIZE 1024
#define NAME_SIZE 32

static void *receive_messages(void *arg);

/* Send the whole buffer, handling partial sends. Returns 0 on success. */
static int send_all(int fd, const char *data, size_t len) {
    while (len > 0) {
        ssize_t n = send(fd, data, len, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        data += n;
        len -= (size_t)n;
    }
    return 0;
}

/* Discard the rest of an overlong input line. */
static void drain_line(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <server_ip>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    int client_socket;
    struct sockaddr_in server_addr;
    pthread_t tid;
    char buffer[BUFFER_SIZE];
    char username[NAME_SIZE];

    signal(SIGPIPE, SIG_IGN);

    printf("Enter your username: ");
    fflush(stdout);
    if (!fgets(username, sizeof(username), stdin)) {
        return 1;
    }
    if (!strchr(username, '\n')) drain_line();
    username[strcspn(username, "\n")] = '\0';
    if (username[0] == '\0') {
        strcpy(username, "anonymous");
    }

    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid address or address not supported\n");
        close(client_socket);
        return 1;
    }

    if (connect(client_socket, (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(client_socket);
        return 1;
    }

    /* The thread gets its own copy of the fd, not a pointer to main's. */
    int *sock_arg = malloc(sizeof(int));
    if (!sock_arg) {
        close(client_socket);
        return 1;
    }
    *sock_arg = client_socket;

    int rc = pthread_create(&tid, NULL, receive_messages, sock_arg);
    if (rc != 0) {
        fprintf(stderr, "pthread_create failed: %s\n", strerror(rc));
        free(sock_arg);
        close(client_socket);
        return 1;
    }
    pthread_detach(tid);

    while (fgets(buffer, sizeof(buffer), stdin)) {
        if (!strchr(buffer, '\n')) drain_line(); /* line too long: truncate */
        buffer[strcspn(buffer, "\n")] = '\0';

        if (buffer[0] == '\0') continue; /* don't send empty messages */

        /* Newline terminates each message so the server can frame them. */
        char message[NAME_SIZE + BUFFER_SIZE + 4];
        int len = snprintf(message, sizeof(message), "%s: %s\n",
                           username, buffer);
        if (len < 0) continue;
        if ((size_t)len >= sizeof(message)) len = sizeof(message) - 1;

        if (send_all(client_socket, message, (size_t)len) < 0) {
            fprintf(stderr, "Connection to server lost.\n");
            break;
        }
    }

    shutdown(client_socket, SHUT_RDWR);
    close(client_socket);
    return 0;
}

static void *receive_messages(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    ssize_t n;

    /* Messages are newline-terminated by the sender, so printing the raw
       stream is correct even if recv() splits or merges messages. */
    while ((n = recv(client_socket, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, (size_t)n, stdout);
        fflush(stdout);
    }

    printf("Disconnected from server.\n");
    exit(0);
    return NULL;
}
