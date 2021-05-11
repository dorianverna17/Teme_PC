#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "helpers.h"

int main(int argc, char *argv[]) {
	// dezactivez buffering-ul
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);

	int sockfd, n, ret, flag = 0;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1) {
        perror("setsocketopt");
        exit(1);
    }

	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	memset(buffer, 0, BUFLEN);
	tcp_connection_msg msg;
	msg.length = strlen(argv[1]);
	msg.id = malloc(msg.length * sizeof(char));
	strcpy(msg.id, argv[1]);
	msg.connect = strlen("connect");
	msg.connection = malloc(msg.connect * sizeof(char));
	strcpy(msg.connection, "connect");
	msg.port = atoi(argv[3]);
	sprintf(buffer, "%d %s %d %s %d", msg.length, msg.id,
		msg.connect, msg.connection, msg.port);
	n = send(sockfd, buffer, strlen(buffer), 0);
	DIE(n < 0, "send");
	// Neagle
	// int Neagle = 1;
	// setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&Neagle, sizeof(int));
	fd_set read_fds, tmp_read_fds;
	FD_SET(0, &read_fds);
	FD_SET(sockfd, &read_fds);

	int max_fd = sockfd;
	// folosite in strtok
	char *p, *topic;
	int sf;

	while (1) {
		tmp_read_fds = read_fds;
    	int rc = select(max_fd + 1, &tmp_read_fds, NULL, NULL, NULL);
		DIE(rc < 0, "select");

		memset(buffer, 0, BUFLEN);
		if (FD_ISSET(0, &tmp_read_fds)) {
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN, stdin);
    		if (strncmp(buffer, "exit", 4) == 0) {
				flag = 1;
			} else if (strncmp(buffer, "subscribe", 9) == 0 && buffer[0] != 'S') {
				// aici fac parsarea mesajului de subscribe
				// compunerea mesajului tcp
				tcp_msg *m = malloc(sizeof(tcp_msg));
				strcpy((*m).id, argv[1]);
				p = strtok(buffer, " ");
				p = strtok(NULL, " ");
				strcpy((*m).topic, p);
				(*m).sf = atoi(strtok(NULL, " "));
				(*m).port = atoi(argv[3]);
				(*m).type = 0;
				// se trimite mesaj la server
				n = send(sockfd, m, sizeof(tcp_msg), 0);
				DIE(n < 0, "send");
				printf("Subscribed to topic.\n");
			} else if (strncmp(buffer, "unsubscribe", 11) == 0 && buffer[0] != 'U') {
				// aici fac parsarea mesajului de unsubscribe
				tcp_msg *m = malloc(sizeof(tcp_msg));
				strcpy((*m).id, argv[1]);
				p = strtok(buffer, " ");
				p = strtok(NULL, " ");
				strcpy((*m).topic, p);
				(*m).sf = -1;
				(*m).port = atoi(argv[3]);
				(*m).type = 1;
				// se trimite mesaj la server
				n = send(sockfd, m, sizeof(tcp_msg), 0);
				DIE(n < 0, "send");
				printf("Unsubscribed from topic.\n");
			}
    	}
    	if (flag == 1)
    		break;
		if (FD_ISSET(sockfd, &tmp_read_fds)) {
			tcp_msg *m = malloc(sizeof(tcp_msg));
			n = recv(sockfd, m, sizeof(tcp_msg), 0);
			DIE(n < 0, "recv");
			if (m->type == 3) {
				break;
			} else if (m->type == 1) {
				printf("1\n");
			} else if (m->type == 2) {
				printf("%s:%d - %s\n", m->ip, m->port, m->value);
			}
		}
	}

	close(sockfd);

	return 0;
}