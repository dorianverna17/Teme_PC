#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "helpers.h"

// functie pentru initializare socket
// am observat ca pt UDP nu merge asa ca o folosesc doar pentru TCP
void initialize_socket(int *sockfd, struct sockaddr_in *serv_addr, int port, int family) {
	int enable = 1, ret;

	memset((char *) serv_addr, 0, sizeof(struct sockaddr_in));
	(*serv_addr).sin_family = AF_INET;
	(*serv_addr).sin_port = htons(port);
	(*serv_addr).sin_addr.s_addr = INADDR_ANY;

	// family este familia protocoalelor folosite
	if (family == AF_INET)
		*sockfd = socket(AF_INET, SOCK_STREAM, 0);
    else
    	*sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(*sockfd < 0, "Error creating socket");

    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1) {
        perror("setsocketopt");
        exit(1);
    }

	DIE(*sockfd < 0, "socket");

	ret = bind(*sockfd, (struct sockaddr *) serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");
}

// functie care imi parseaza un mesaj tcp de conectare
// structura pentru un astfel de mesaj este diferita de cea
// pentru un mesaj tcp pentru a trimite topicuri sau mesaje de
// subscribe / unsubscribe
tcp_connection_msg parse_tcp_connection_message(char *buffer) {
	int id = 0, lenght_id, lenght_connection;
	tcp_connection_msg msg;
	char *p;

	p = strtok(buffer, " ");
	msg.length = atoi(p);
	p = strtok(NULL, " ");
	msg.id = malloc((strlen(p) + 1) * sizeof(char));
	strcpy(msg.id, p);
	p = strtok(NULL, " ");
	msg.connect = atoi(p);
	p = strtok(NULL, " ");
	msg.connection = malloc((strlen(p) + 1) * sizeof(char));
	strcpy(msg.connection, p);
	return msg;
}

// functie care imi parseaza mesajul primit de la udp intr-un buffer
// si imi returneaza un udp_msg - structura definita de mine in helpers.h
udp_msg parse_udp_message(char *buffer) {
	udp_msg m;

	memcpy(&m.topic, buffer, 50);
	memcpy(&m.type, buffer + 50, 1);
	if (m.type == 3) {
		memcpy(&m.value, buffer + 51, 1500);
	} else if (m.type == 0) {
		memcpy(&m.sign, buffer + 51, 1);
		memcpy(&m.numberINT, buffer + 52, 4);
		m.numberINT = ntohl(m.numberINT);
	} else if (m.type == 1) {
		memcpy(&m.numberSR, buffer + 51, 2);
		m.numberSR = htons(m.numberSR);
	} else if (m.type == 2) {
		memcpy(&m.sign, buffer + 51, 1);
		memcpy(&m.numberF, buffer + 52, 4);
		m.numberF = ntohl(m.numberF);
		memcpy(&m.pow, buffer + 56, 1);
	}
	return m;
}

// aici formez un string din mesajul udp
// asa cum mi se pare mie mai usor pentru a fi trimis catre
// un client tcp
char* udp_to_string(udp_msg m) {
	char *buffer = malloc(BUFLEN * sizeof(char));
	if (m.type == 0) {
		if (m.sign == 0)
			sprintf(buffer, "%s - INT - %d", m.topic, m.numberINT);
		else
			sprintf(buffer, "%s - INT - -%d", m.topic, m.numberINT);
	} else if (m.type == 1) {
		char n[20], res[20], aux[20];
		sprintf(aux, "%d", m.numberSR);
		strcpy(n, aux);
		strncpy(res, n, strlen(n) - 2);
		res[strlen(n) - 2] = '\0';
		strcat(res, ".\0");
		strcat(res, n + strlen(n) - 2);
		sprintf(buffer, "%s - SHORT_REAL - %s", m.topic, res);
	} else if (m.type == 2) {
		char n[20], res[20], aux[20];
		sprintf(aux, "%d", m.numberF);
		strcpy(n, aux);
		strcpy(res, "");
		if (m.pow >= strlen(n)) {
			if (m.pow > strlen(n)) {
				strcat(res, "0.");
			}
			while (m.pow > strlen(n)) {
				strcat(res, "0");
				m.pow--;
			}
			strncat(res, n, strlen(n));
		} else {
			strncpy(res, n, strlen(n) - m.pow);
			res[strlen(n) - m.pow] = '\0';
			if (strlen(n) - m.pow > 0)
				strcat(res, ".\0");
			strcat(res, n + strlen(n) - m.pow);
		}
		if (m.sign == 0)
			sprintf(buffer, "%s - FLOAT - %s", m.topic, res);
		else
			sprintf(buffer, "%s - FLOAT - -%s", m.topic, res);
	} else if (m.type == 3) {
		sprintf(buffer, "%s - STRING - %s", m.topic, m.value);
	}
	return buffer;
}

// functie care imi trimite mesajul tcp catre clientii care
// au dat subscribe la acel topic, daca clientul nu este conectat
// si are sf-ul 1, atunci voi salva mesajul intr-o lista pentru
// acel client si voi trimite mesajul cand se va conecta
void send_tcp_messages(udp_msg msg, topic_table topics,
	hashtable table, struct sockaddr_in ip, int port) {
	topic_entry entry = find_topic_entry(topics, msg.topic);
	if (entry == NULL)
		return;
	client_topic *clients = entry->clients;
	int size = entry->size, n;
	char *buffer = malloc(BUFLEN * sizeof(char));
	strcpy(buffer, udp_to_string(msg));
	tcp_msg *m;
	for (int i = 0; i < size; i++) {
		if (!clients[i])
			continue;
		client_entry e = find_client_entry(clients[i]->id, table);
		if (e == NULL)
			continue;
		m = malloc(sizeof(tcp_msg));
		strcpy(m->id, clients[i]->id);
		strcpy(m->topic, msg.topic);
		m->sf = clients[i]->sf;
		m->port = htons(ip.sin_port);
		inet_ntop(AF_INET, &(ip.sin_addr), m->ip, INET_ADDRSTRLEN);
		m->type = 2;
		strcpy(m->value, buffer);
		// Daca clientul este conectat, atunci trimit mesajul catre el
		if (e->connected == 1) {
			int sent = 0;
			n = send(e->sockfd, m, sizeof(tcp_msg), 0);
			DIE(n < 0, "recv");
			sent = sent + n;
			while (sent < sizeof(tcp_msg)) {
				n = send(e->sockfd, m + sent, sizeof(tcp_msg) - sent, 0);
				DIE(n < 0, "recv");
				if (n == 0)
					break;
				sent = sent + n;
			}

		// Daca clientul nu este conectat, dar sf-ul este 1, atunci
		// pun mesajul in lista de mesaje care trebuie trimise clientului
		// cand se va conecta
		} else if (e->connected == 0 && m->sf == 1) {
			add_list(&(e->head), *m);
		}
	}	
}

// functia trimite mesajele pentru clientii cu sf-ul 1 care nu erau conectati
// in acel moment
hashtable send_remaining_messages(int sockfd, char *id, hashtable table) {
	client_entry e = find_client_entry(id, table);
	int n;
	Node it = e->head;

	while (it != NULL) {
		int sent = 0;
		n = send(sockfd, &(it->m), sizeof(tcp_msg), 0);
		DIE(n < 0, "recv");
		sent = sent + n;
		while (sent < sizeof(tcp_msg)) {
			n = send(sockfd, &(it->m) + sent, sizeof(tcp_msg) - sent, 0);
			DIE(n < 0, "recv");
			if (n == 0)
				break;
			sent = sent + n;
		}

		it = it->next;
	}
	e->head = NULL;
	return table;
}

int main(int argc, char *argv[]) {
	// dezactivarea buffering-ului
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	// voi folosi un hashtable pentru a lucra cu clientii tcp
	hashtable table = initialize_table();

	// variabile auxiliare
	struct sockaddr_in client_address;
	socklen_t addr_size;
	int port, n, et, sockfd;
	// flag este pentru a determina daca inchid server-ul
	int i, flag = 0, ret, rd;
	// buffer-ul pentru mesaje
	char buffer[BUFLEN];

	fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
	int fdmax;			// valoare maxima fd din multimea read_fds

	// Se goleste multimea de descriptori de citire (read_fds)
	// si multimea temporara (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// preluarea datelor serverului
	struct sockaddr_in serv_addr_tcp, serv_addr_udp;

	// preluarea portului
	port = atoi(argv[1]);
	DIE(port == 0, "atoi");

	// socketurile pentru TCP si UDP
	int sockfdTCP, sockfdUDP;

	// crearea socket-urilor si realizarea bind-urilor

	// crearea socketului pentru UDP
	int enable = 1;

	memset((char*) &serv_addr_udp, 0, sizeof(struct sockaddr_in));
	serv_addr_udp.sin_family = AF_INET;
	serv_addr_udp.sin_port = htons(port);
	serv_addr_udp.sin_addr.s_addr = INADDR_ANY;

	// family este familia protocoalelor folosite
    sockfdUDP = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(sockfdUDP < 0, "Error creating socket");

    if (setsockopt(sockfdUDP, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) == -1) {
        perror("setsocketopt");
        exit(1);
    }

	DIE(sockfdUDP < 0, "socket");

	ret = bind(sockfdUDP, (struct sockaddr *) &serv_addr_udp, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	// crearea socket-ului pentru TCP
	initialize_socket(&sockfdTCP, &serv_addr_tcp, port, AF_INET);

	// 0 este pentru stdin - pentru comanda exit
	FD_SET(0, &read_fds);
	// adaugarea descriptorilor in multimea set
	FD_SET(sockfdUDP, &read_fds);
	FD_SET(sockfdTCP, &read_fds);
	fdmax = sockfdTCP;

	// socketul inactiv
	ret = listen(sockfdTCP, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	
	// initializez tabela de hash pentru topicuri
	topic_table topics = initialize_topics();
	// tabela pentru socketi
	socket_table table_sock = initialize_socket_table();

	// aici este implementata funtionalitatea serverului
	while (1) {
		tmp_fds = read_fds;
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");
		flag = 0;

		for (i = 0; i <= fdmax; i++) {
			memset(buffer, 0, BUFLEN);
			if (FD_ISSET(i, &tmp_fds)) {
				// aici primesc un mesaj de exit
				if (i == 0) {
					fgets(buffer, BUFLEN, stdin);
					if (strcmp(buffer, "exit\n") == 0)
						flag = 1;
				// aici primesc un mesaj de la clientul UDP
				} else if (i == sockfdUDP) {
					socklen_t len = sizeof(serv_addr_udp);
					memset(buffer, 0, BUFLEN);

					rd = recvfrom(sockfdUDP, (char*) buffer, BUFLEN, 0,
						(struct sockaddr *) &serv_addr_udp, &len);
					if (rd >= 0) {
						udp_msg msg = parse_udp_message(buffer);
						send_tcp_messages(msg, topics, table,
							serv_addr_udp, len);
					}
				// aici primesc un mesaj de pe socketul TCP
				} else if (i == sockfdTCP) {
					// cerere pe socketul cu listen
					addr_size = sizeof(client_address);
					sockfd = accept(sockfdTCP, (struct sockaddr *)
						&client_address, &addr_size);
					DIE(sockfd < 0, "accept");
					// adaug noul socket in multimea descriptorilor
					FD_SET(sockfd, &read_fds);
					if (sockfd > fdmax) { 
						fdmax = sockfd;
					}
					// Dezactivare Neagle
					int Neagle = 1;
					setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&Neagle, sizeof(int));
					// Nu stiu id-ul - il voi afla printr-un mesaj dat
					// imediat dupa subscribe
					n = recv(sockfd, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");
					// am primit mesajul, iar primul caracter din mesaj este id-ul
					// clientului conectat
					tcp_connection_msg msg = parse_tcp_connection_message(buffer);
					if (strcmp(msg.connection, "connect") == 0) {
						int result;
						// caut id-ul in tabela de socketi
						// daca exista inseamna ca clientul este conectat
						result = find_id_socket_table(msg.id, table_sock);
						if (result) {
							printf("Client %s already connected.\n", msg.id);
							tcp_msg *message = malloc(sizeof(message));
							(*message).type = 3;
							n = send(sockfd, message, sizeof(tcp_msg), 0);
							DIE(n < 0, "send");
							close(sockfd);
							FD_CLR(sockfd, &read_fds);
						} else {
							// Daca nu atunci am de a face cu un client nou
							printf("New client %s connected from %s:%d.\n",
								msg.id, inet_ntoa(client_address.sin_addr),
								ntohs(client_address.sin_port));
							// adaugarea clientului in hashtable-ul cu clienti
							result = add_socket(sockfd, msg.id, &table_sock);
							result = add_client(&table, msg, sockfd);
							// daca clientul nu a mers sa fie adaugat in tabela de clienti
							// inseamna ca el s-a mai conectat inainte, caz in care doar
							// trebuie sa setez connected = 1 si sa trimit mesajele care
							// sunt de trimis daca sf == 1
							if (result == 0) {
								table = connect_client(table, msg.id, sockfd);
								table = send_remaining_messages(sockfd, msg.id, table);
							}
						}
					}
				} else {
					// aici primesc mesaje de la clientii TCP
					memset(buffer, 0, BUFLEN);
					tcp_msg *msg = malloc(sizeof(tcp_msg));

					int size_received = sizeof(tcp_msg), received = 0, n;
					n = recv(sockfd, msg, size_received, 0);
					DIE(n < 0, "recv");
					received = received + n;
					if (n != 0) {
						while (received < sizeof(tcp_msg)) {
							n = recv(sockfd, msg + received, sizeof(tcp_msg) - received, 0);
							DIE(n < 0, "recv");
							if (n == 0)
								break;
							received = received + n;
						}
					}
					// In cazul in care n este 0, atunci inseamna ca clientul
					// a fost inchis si deci trebuie sa fac schimbarile necesare
					// in tabele si sa inchid socketul lui
					if (n == 0) {
						close(i);
						FD_CLR(i, &read_fds);
						char id[10];
						strcpy(id, find_client_id(i, table_sock)); 
						printf("Client %s disconnected.\n", id);
						// stergerea clientului din tabelul de socketi
						table_sock = delete_client(i, table_sock);
						table = disconnect_client(table, id);
						if (i == fdmax) {
							int j, max = -1;
							for (j = 0; j < fdmax; j++) {
								if (FD_ISSET(j, &tmp_fds) && j > max) {
									max = j;
								}
							}
							fdmax = j;
						}
						// daca este un mesaj de subscribe, atunci adaug topic-ul daca acesta
						// nu exista inainte in tabela, apoi sa modific intrarea in tabela,
						// adaugand clientul in lista celor care au dat subscribe pe acel topic
					} else if (n > 0 && (*msg).type == 0) {
						// Aici clientul da un mesaj cu subscribe
						if (!isTopic(topics, (*msg).topic)) {
							topics = add_topic(topics, (*msg).topic);
						}
						tcp_msg *message = malloc(sizeof(tcp_msg));
						memcpy(message, msg, sizeof(tcp_msg));
						subscribe_client(&topics, (*message).topic, (*message).id, (*message).sf);
					} else if (n > 0 && (*msg).type == 1) {
						// Aici clientul da un mesaj cu unsubscribe
						// modific tabela topics stergand din lista de abonati topicului acel client
						topics = unsubscribe_client(topics, (*msg).topic, (*msg).id);
					}
				}
			}
		}
		// daca opresc serverul, atunci trebuie sa opresc si toti clientii conectati
		// in acel moment la server
		if (flag == 1) {
			for (int j = 0; j <= fdmax; j++) {
				if (FD_ISSET(j, &read_fds)) {
					if (j != 0 && j != sockfdTCP && j != sockfdUDP) {
						tcp_msg m;
						m.type = 3;
						// le trimit un mesaj de deconectare
						int sent = 0;
						n = send(j, &m, sizeof(tcp_msg), 0);
						DIE(n < 0, "recv");
						sent = sent + n;
						while (sent < sizeof(tcp_msg)) {
							n = send(j, &m + sent, sizeof(tcp_msg) - sent, 0);
							DIE(n < 0, "recv");
							if (n == 0)
								break;
							sent = sent + n;
						}
					}
					close(j);
					FD_CLR(j, &read_fds);
				}
			}
			break;
		}
	}
	// la final inchid cei doi socketi
	close(sockfdTCP);
	close(sockfdUDP);

	return 0;
}