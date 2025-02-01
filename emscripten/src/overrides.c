#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>

// In emscripten, all C code is being run inside a browser. As such,
// DNS lookups do not work (no UDP, no OS resolver, etc). So, we
// override gethostbyname with our own DNS resolver. The WebPDL2Ork
// server which proxies all outgoing sockets also listens for control
// messages, one of which is a request to resolve a domain. This code
// contacts the WebPdL2Ork server and resolves the domain, then transforms
// the result into a hostent to replace the builtin gethostbyname
struct hostent *__em_gethostbyname(const char *__name) {
	int sockfd;
    struct sockaddr_in server_addr;
    char buffer[256];
    struct hostent *result = NULL;
    
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		fprintf(stderr, "Failed to create DNS socket!\n");
        return NULL;
    }
    
	// 127.0.0.1:3000 signifies to WebPdL2Ork socket intercepter
	// to make a raw socket request to the backend rather than
	// automatically proxying it onwards to its destination
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(3000);
    server_addr.sin_family = AF_INET;
    
    // Connect to the server
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		perror("connect");
        close(sockfd);
        return NULL;
    }
    
    // Send request
    snprintf(buffer, sizeof(buffer), "resolve %s", __name);
    if (send(sockfd, buffer, strlen(buffer), 0) < 0) {
		perror("send");
        close(sockfd);
        return NULL;
    }
    
    // Receive response
    memset(buffer, 0, sizeof(buffer));
	while(recv(sockfd, buffer, sizeof(buffer) - 1, 0) <= 0) {
		if(errno != EAGAIN) {
			perror("recv");
			close(sockfd);
			return NULL;
		}
		usleep(5000);
	}
    close(sockfd);
    
    // Check for failure response
    if (strncmp(buffer, "FAILED", 6) == 0) {
        return NULL;
    }
    
    // Allocate hostent structure
    result = malloc(sizeof(struct hostent));
    if (!result) return NULL;
    memset(result, 0, sizeof(struct hostent));
    
    // Fill hostent
    result->h_name = strdup(__name);
    result->h_aliases = NULL;
    result->h_addrtype = AF_INET;
    result->h_length = sizeof(struct in_addr);
    
    result->h_addr_list = malloc(2 * sizeof(char*));
    if (!result->h_addr_list) {
        free(result->h_name);
        free(result);
        return NULL;
    }
    
    result->h_addr_list[0] = malloc(sizeof(struct in_addr));
    if (!result->h_addr_list[0]) {
        free(result->h_addr_list);
        free(result->h_name);
        free(result);
        return NULL;
    }
    
    inet_pton(AF_INET, buffer, result->h_addr_list[0]);
    result->h_addr_list[1] = NULL;
    
    return result;
}