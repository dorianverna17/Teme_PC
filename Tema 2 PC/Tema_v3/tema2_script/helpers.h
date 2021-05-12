#ifndef __HELPERS_H__
#define __HELPERS_H__

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#define BUFLEN		1700	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	1000	// numarul maxim de clienti in asteptare
#define SIZE 100

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

// Mai jos este structura pentru un mesaj udp

typedef struct udp_message {
	char topic[51];
	char type;
	// completat doar daca type este 0, 1 sau 2
	char sign;
	// daca type este 0 - INT
	uint32_t numberINT;
	// daca type este 1 - SHORT REAL
	uint16_t numberSR;
	// daca type este 2 - FLOAT
	uint32_t numberF;
	uint8_t pow;
	// daca type este 3 - STRING
	char value[1500];
} udp_msg;

// -- strutura pentru tipul mesajului folosit
// -- pentru a comunica cu clientii TCP atunci
// -- cand vine vorba de conectarea la server

typedef struct tcp_connection_msg {
	int length;
	char *id;
	int connect;
	char *connection;
	int port;
} tcp_connection_msg;

// -- structura pentru un mesaj tcp

typedef struct tcp_message {
	char id[11];
	char topic[51];
	int sf;
	int port;
	char ip[INET_ADDRSTRLEN + 1];
	// 0 - subscribe
	// 1 - unsubscribe
	// 2 - topic from server
	// 3 - disconnect
	int type;
	char value[1600];
} tcp_msg;

// -- aici este structura pentru o lista
// -- si metodele care trebuie folosite

typedef struct List {
	tcp_msg m;
	struct List *next;
} *Node;

// adaugare la sfarsitul listei
int add_list(Node *head, tcp_msg m) {
	Node new_node = malloc(sizeof(struct List));
	new_node->next = NULL;
	new_node->m = m;

	Node it;
	if (*head == NULL) {
		*head = new_node;
	} else {
		it = *head;
		while (it->next != NULL) {
			it = it->next;
		}
		it->next = new_node;
	}
	return 1;
}

// -- mai sus sunt functiile pentru o lista generica

// -- acestea sunt functiile pentru
// -- a manipula tabelul de hash-uri pentru socketi

typedef struct socket_entry {
	int sockfd;
	char id[11];
	struct socket_entry *next;
} *table_socket_entry;

typedef struct table_socket {
	table_socket_entry sockets[SIZE];
} *socket_table;

socket_table initialize_socket_table() {
	socket_table table = malloc(sizeof(struct table_socket));
	for (int i = 0; i < SIZE; i++) {
		table->sockets[i] = NULL;
	}
}

int add_socket(int sockfd, char* id, socket_table *table) {
	int pos = sockfd % SIZE;
	table_socket_entry entry = (*table)->sockets[pos];
	if (entry == NULL) {
		(*table)->sockets[pos] = malloc(sizeof(struct socket_entry));
		(*table)->sockets[pos]->next = NULL;
		strcpy((*table)->sockets[pos]->id, id);
		(*table)->sockets[pos]->sockfd = sockfd;
		return 1;
	} else {
		while (entry->next != NULL) {
			if (entry->sockfd == sockfd)
				return 0;
			entry = entry->next;
		}
		table_socket_entry new_node = malloc(sizeof(struct socket_entry));
		new_node->sockfd = sockfd;
		strcpy(new_node->id, id);
		new_node->next = NULL;
		entry->next = new_node;
		return 1;
	}
	return 0;
}

char* find_client_id(int socket, socket_table table) {
	int pos = socket % SIZE;
	table_socket_entry entry = table->sockets[pos];
	if (entry != NULL) {
		while (entry != NULL) {
			if (entry->sockfd == socket)
				return entry->id;
			entry = entry->next;
		}
	}
	return NULL;
}

socket_table delete_client(int sockfd, socket_table table) {
	int pos = sockfd % SIZE;
	table_socket_entry entry = table->sockets[pos];
	if (entry != NULL) {
		if (entry->sockfd == sockfd) {
			table->sockets[pos] = entry->next;
			return table;
		}
		while (entry->next != NULL && entry->next->sockfd != sockfd) {
			entry = entry->next;
		}
		if (entry->next != NULL) {
			entry->next = entry->next->next;
		} else {
			entry->next = NULL;
		}
	}
	return table;
}

int find_id_socket_table(char* id, socket_table table) {
	for (int i = 0; i < SIZE; i++) {
		table_socket_entry entry = table->sockets[i];
		if (table->sockets[i] != NULL)
			entry = table->sockets[i];
		while (entry != NULL) {
			if (strcmp(id, entry->id) == 0)
				return 1;
			entry = entry->next;
		}
	}
	return 0;
}

void print_sockets(socket_table table) {
	for (int i = 0; i < SIZE; i++) {
		table_socket_entry entry = table->sockets[i];
		printf("%d ", i);
		if (entry == NULL)
			printf("NULL\n");
		else {
			while(entry != NULL) {
				printf("%d %s", entry->sockfd, entry->id);
				entry = entry->next;
			}
			printf("\n");
		}
	}
}

// -- mai sus sunt functiile pentru tabela de hash-uri
// -- pentru socketi

// Mai jos este implementarea Hashtable-ului pentru id
// Aici tin clientii in functie de id si stochez mesajele
// care trebuie trimise catre ei daca vine vorba de store and forward

int hash(char *str) {
	int hash = 5381, c, i = 0;

	c = *str;
	while (c != '\0') {
		i++;
		hash = ((hash << 5) + hash) + c;
		c = str[i];
	}
	if (hash < 0)
		hash = -hash;
	return hash;
}

// aceste structuri le folosesc pentru o mai buna functionare
// a tabelei de hash-uri
typedef struct client_entry {
	char id[11];
	int port;
	int connected;
	int sockfd;
	Node head;
} *client_entry;

typedef struct client_entry_node {
	client_entry client;
	struct client_entry_node *next;
} *client_entry_node;

typedef struct hash_table {
	client_entry_node vector[SIZE];
} *hashtable;

Node new_node() {
	Node node = malloc(sizeof(struct List));
	node->next = NULL;
	return node;
}

hashtable initialize_table() {
	hashtable table = malloc(sizeof(struct hash_table));
	for (int i = 0; i < SIZE; i++) {
		table->vector[i] = NULL;
	}
	return table;
}

int add_client_node(client_entry_node *node, tcp_connection_msg m, int sockfd) {
	if (*node == NULL) {
		*node = malloc(sizeof(struct client_entry_node));
		(*node)->next = NULL;
		(*node)->client = malloc(sizeof(struct client_entry));
		strcpy((*node)->client->id, m.id);
		(*node)->client->port = m.port;
		(*node)->client->connected = 1;
		(*node)->client->head = NULL;
		(*node)->client->sockfd = sockfd;
		return 1;
	} else {
		client_entry_node it = *node;
		if (strcmp(it->client->id, m.id) == 0)
			return 0;
		while (it->next != NULL) {
			it = it->next;
			if (strcmp(it->client->id, m.id) == 0)
				return 0;
		}
		client_entry_node new_node = malloc(sizeof(struct client_entry_node));
		new_node->next = NULL;
		new_node->client = malloc(sizeof(struct client_entry));
		strcpy(new_node->client->id, m.id);
		new_node->client->port = m.port;
		new_node->client->connected = 1;
		new_node->client->head = NULL;
		new_node->client->sockfd = sockfd;
		if (it->next != NULL)
			new_node->next = it->next->next;
		else
			new_node->next = NULL;
		it->next = new_node;
	}
	return 1;
}

int add_client(hashtable *table, tcp_connection_msg m, int sockfd) {
	int entry = hash(m.id) % SIZE;
	int result = add_client_node(&(*table)->vector[entry], m, sockfd);
	return result;
}

int find_id_hashtable(char* id, hashtable table) {
	for (int i = 0; i < SIZE; i++) {
		client_entry_node it = NULL;
		if (table->vector[i] != NULL)
			it = table->vector[i];
		while (it != NULL) {
			if (strcmp(id, it->client->id) == 0)
				return 1;
			it = it->next;
		}
	}
	return 0;
}

client_entry find_client_entry(char* id, hashtable table) {
	int pos = hash(id) % SIZE;
	client_entry_node node = table->vector[pos];
	if (node != NULL) {
		client_entry_node it = node;
		while (it != NULL) {
			if (strcmp(it->client->id, id) == 0) {
				return it->client;
			}
			it = it->next;
		}
	}
	return NULL;
}

hashtable disconnect_client(hashtable table, char* id) {
	int pos = hash(id) % SIZE;
	client_entry_node node = table->vector[pos];
	if (node != NULL) {
		client_entry_node it = node;
		while (it != NULL) {
			if (strcmp(it->client->id, id) == 0) {
				it->client->connected = 0;
			}
			it = it->next;
		}
	}
	return table;
}

hashtable connect_client(hashtable table, char* id, int sockfd) {
	int pos = hash(id) % SIZE;
	client_entry_node node = table->vector[pos];
	if (node != NULL) {
		client_entry_node it = node;
		while (it != NULL) {
			if (strcmp(it->client->id, id) == 0) {
				it->client->connected = 1;
				it->client->sockfd = sockfd;
			}
			it = it->next;
		}
	}
	return table;
}

// Mai jos am un tabel de hash-uri pentru fiecare topic in care
// retin userii care au dat subscribe acolo

typedef struct client_topic_entry {
	char id[11];
	int sf;
} *client_topic;

typedef struct topic_entry_struct {
	char topic[51];
	client_topic clients[MAX_CLIENTS];
	int size;
	struct topic_entry_struct *next;
} *topic_entry;

typedef struct table_topic {
	topic_entry topics[SIZE];
} *topic_table;

topic_table initialize_topics() {
	topic_table table = malloc(sizeof(struct table_topic));
	for (int i = 0; i < SIZE; i++) {
		table->topics[i] = NULL;
	}
	return table;
}

topic_table add_topic(topic_table table, char *topic) {
	int pos = hash(topic) % SIZE;
	topic_entry new_topic = malloc(sizeof(struct topic_entry_struct));
	strcpy(new_topic->topic, topic);
	new_topic->next = NULL;
	new_topic->size = 0;
	if (table->topics[pos] == NULL) {
		table->topics[pos] = new_topic;
	} else {
		topic_entry it = table->topics[pos];
		while (it->next != NULL) {
			it = it->next;
		}
		it->next = new_topic;
	}
	return table;
}

void subscribe_client(topic_table *table, char *topic, char *id, int sf) {
	int pos = hash(topic) % SIZE;
	if (!((*table)->topics[pos] == NULL)) {
		topic_entry it = (*table)->topics[pos];
		while (it != NULL) {
			if (strcmp(it->topic, topic) == 0) {
				client_topic c = malloc(sizeof(struct client_topic_entry));
				strcpy(c->id, id);
				c->sf = sf;
				it->clients[it->size] = c;
				it->size++;
				return;
			}
			it = it->next;
		}
	}
}

topic_table unsubscribe_client(topic_table table, char *topic, char *id) {
	int pos = hash(topic) % SIZE, aux;
	if (table->topics[pos] == NULL) {
		return table;
	} else {
		topic_entry it = table->topics[pos];
		while (it != NULL) {
			if (strcmp(it->topic, topic) == 0) {
				for (int i = 0; i < MAX_CLIENTS; i++) {
					if (strcmp(it->clients[i]->id, id) == 0) {
						aux = i;
						break;
					}
				}
				for (int i = aux; i < MAX_CLIENTS - 1; i++) {
					it->clients[i] = it->clients[i + 1];
				}
				it->size--;
			}
			it = it->next;
		}
	}
	return table;	
}

int isTopic(topic_table table, char *topic) {
	int pos = hash(topic) % SIZE;
	if (table->topics[pos] != NULL) {
		topic_entry it = table->topics[pos];
		while (it != NULL) {
			if (strcmp(it->topic, topic) == 0) {
				return 1;
			}
			it = it->next;
		}
	}
	return 0;
}

topic_entry find_topic_entry(topic_table table, char *topic) {
	int pos = hash(topic) % SIZE;
	if (table->topics[pos] != NULL) {
		topic_entry it = table->topics[pos];
		while (it != NULL) {
			if (strcmp(it->topic, topic) == 0) {
				return it;
			}
			it = it->next;
		}
	}
	return NULL;
}

void print_topics(topic_table table) {
	for (int i = 0; i < SIZE; i++) {
		topic_entry it = table->topics[i];
		while (it != NULL) {
			printf("%s ", it->topic);
			for (int j = 0; j < it->size; j++) {
				printf("(%s, %d) ", it->clients[j]->id, it->clients[j]->sf);
			}
			it = it->next;
		}
		printf("\n");
	}
}

#endif /* __HELPERS_H__ */
