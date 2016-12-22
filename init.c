#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NODES 3
#define BUFLEN 10

typedef struct {
	char buf[BUFLEN];
	int nodes[MAX_NODES];
	int len;
} InitMP;

int main() {
	int sock;
	struct sockaddr_in other;
	InitMP myinit;

	sock = socket(AF_INET, SOCK_DGRAM, 0);
	other.sin_family = AF_INET;
	other.sin_addr.s_addr = INADDR_ANY;
	other.sin_port = htons(9070);

	strcpy(myinit.buf, "init");
	myinit.len = 0;

	if (sendto(sock, &myinit, sizeof(InitMP), 0, (struct sockaddr *) &other, sizeof(struct sockaddr)) < 0) {
		printf("cannot send to router 1\n");
	}
	printf("init router 1 on port %d\n", other.sin_port);
	close(sock);
}