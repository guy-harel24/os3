/*
 * client.c: A very, very primitive HTTP client.
 * 
 * Example usage:
 *      ./client www.example.com 80 /
 *
 * This client sends a single HTTP request to a server and prints the response.
 *
 * HW3: For testing your server, you will likely want to modify this client:
 *  - Add multi-threading support to test concurrency.
 *  - Add ability to send different request types and URIs.
 *  - Consider reading URIs or request data from a file or stdin.
 *
 * NOTE: We will use a modified version of this client to test your server.
 */

#include "segel.h"

// Sends an HTTP request to the server using the given socket
void clientSend(int fd, char *filename, char* method)
{
    char buf[MAXLINE];
    char hostname[MAXLINE];

    Gethostname(hostname, MAXLINE); // Get the client's hostname

    // Form the HTTP request line and headers
    sprintf(buf, "%s %s HTTP/1.1\n", method, filename);
    sprintf(buf, "%shost: %s\n\r\n", buf, hostname);

    printf("Request:\n%s\n", buf); // Display the request for debugging

    // Send the request to the server
    Rio_writen(fd, buf, strlen(buf));
}

// Reads and prints the server's HTTP response
void clientPrint(int fd)
{
    rio_t rio;
    char buf[MAXBUF];
    int length = 0;
    int n;

    // Initialize buffered input
    Rio_readinitb(&rio, fd);

    // --- Read and print HTTP headers ---
    n = Rio_readlineb(&rio, buf, MAXBUF);
    while (strcmp(buf, "\r\n") && (n > 0)) {
        printf("Header: %s", buf);

        // Try to parse Content-Length header
        if (sscanf(buf, "Content-Length: %d ", &length) == 1) {
            printf("Length = %d\n", length);
        }

        n = Rio_readlineb(&rio, buf, MAXBUF);
    }

    // --- Read and print HTTP body ---
    n = Rio_readlineb(&rio, buf, MAXBUF);
    while (n > 0) {
        printf("%s", buf);
        n = Rio_readlineb(&rio, buf, MAXBUF);
    }
}

int main(int argc, char *argv[])
{
    char *host, *filename, *method;
    int port;
    int clientfd;

    // Validate input arguments
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <host> <port> <filename> <method>\n", argv[0]);
        exit(1);
    }

    // Parse command-line arguments
    host = argv[1];
    port = atoi(argv[2]);
    filename = argv[3];
    method = argv[4];

    // Open a connection to the specified server
    clientfd = Open_clientfd(host, port);
    if (clientfd < 0) {
        fprintf(stderr, "Error connecting to %s:%d\n", host, port);
        exit(1);
    }

    printf("Connected to server. clientfd = %d\n", clientfd);

    // Send HTTP request and read response
    clientSend(clientfd, filename, method);
    clientPrint(clientfd);

    // Clean up
    Close(clientfd);
    return 0;
}
