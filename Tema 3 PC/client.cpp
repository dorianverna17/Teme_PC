#include <stdio.h>      /* printf, sprintf */
#include <stdlib.h>     /* exit, atoi, malloc, free */
#include <unistd.h>     /* read, write, close */
#include <string.h>     /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h>      /* struct hostent, gethostbyname */
#include <arpa/inet.h>
#include <iostream>

#include "helpers.h"
#include "requests.h"
// am folosit nlohmann pentru a parsa json
#include "nlohmann/json.hpp"

#define BUFFLEN 100
#define CLEN 300
#define JWTLEN 500
#define MESSAGELEN 1000

using namespace std;
// pentru nlohmann
using json = nlohmann::json;

int main(int argc, char *argv[]) {
	// dezactivez buffering-ul
	setvbuf(stdout, NULL, _IONBF, BUFFLEN);
	// realizarea conexiunii + file descriptor
	char *host = (char *)malloc(15 * sizeof(char));
	strcpy(host, "34.118.48.238");
	int sockfd = open_connection(host, 8080,
		AF_INET, SOCK_STREAM, 0);
	// buffer-ul in care citim comenzile
	char buffer[BUFFLEN - 1], *msg;
	// auxiliaries;
	char *r;
	char **transmit = (char **) malloc(1 * sizeof(char*));
	char session_cookie[CLEN], aux[CLEN], jwt[JWTLEN];
	// mai jos este implementata functionalitatea clientului
	while(1) {
		fgets(buffer, BUFFLEN, stdin);
		if (strcmp(buffer, "exit\n") == 0) {
			break;
		} else if (strcmp(buffer, "logout\n") == 0) {
			char route[30];
			strcpy(route, "/api/v1/tema/auth/logout");

			sockfd = open_connection(host, 8080,
				AF_INET, SOCK_STREAM, 0);

			// sending with the cookie
			char *cookie[1];
			cookie[0] = (char *) malloc(CLEN * sizeof(char));
			strcpy(cookie[0], session_cookie);

			msg = compute_get_request(host, route, NULL, cookie, 1);
			puts(msg);
    		send_to_server(sockfd, msg);
    		r = receive_from_server(sockfd);
    		puts(r);

    		// TODO
    		// clean the tokens
		} else if (strcmp(buffer, "delete_book\n") == 0) {
			char id[20];
			cout << "id=";
			fgets(id, 20, stdin);
			id[strlen(id) - 1] = '\0';

			sockfd = open_connection(host, 8080,
				AF_INET, SOCK_STREAM, 0);

			char route[30];
			strcpy(route, "/api/v1/tema/library/books/");
			strcat(route, id);

			char* msg = (char*) malloc(MESSAGELEN * sizeof(char));
			strcat(msg, "DELETE ");
			strcat(msg, route);
			strcat(msg, " HTTP/1.1\r\n");
			strcat(msg, "Authorization: Bearer ");
			strcat(msg, jwt);
			strcat(msg, "\r\nHost: ");
			strcat(msg, host);
			strcat(msg, "\r\n");
			strcat(msg, "\r\n");

			puts(msg);
			send_to_server(sockfd, msg);
    		r = receive_from_server(sockfd);
    		puts(r);

		} else if (strcmp(buffer, "add_book\n") == 0) {
			char title[50], author[50], genre[20];
			char publisher[50], page_count[10];
			cout << "title=";
			fgets(title, 20, stdin);
			title[strlen(title) - 1] = '\0';
			cout << "author=";
			fgets(author, 20, stdin);
			author[strlen(author) - 1] = '\0';
			cout << "genre=";
			fgets(genre, 20, stdin);
			genre[strlen(genre) - 1] = '\0';
			cout << "page_count=";
			fgets(page_count, 20, stdin);
			page_count[strlen(page_count) - 1] = '\0';
			cout << "publisher=";
			fgets(publisher, 20, stdin);
			publisher[strlen(publisher) - 1] = '\0';

			sockfd = open_connection(host, 8080,
				AF_INET, SOCK_STREAM, 0);

			// sending with the cookie
			char *cookie[1];
			cookie[0] = (char *) malloc(CLEN * sizeof(char));
			strcpy(cookie[0], session_cookie);

			json jmsg;
			jmsg["title"] = title;
			jmsg["author"] = author;
			jmsg["genre"] = genre;
			jmsg["page_count"] = atoi(page_count);
			jmsg["publisher"] = publisher;
			string message = jmsg.dump();

			transmit[0] = (char*) malloc(MESSAGELEN * sizeof(char));
			strcpy(transmit[0], message.c_str());

			char route[30], payload[30];
			strcpy(route, "/api/v1/tema/library/books");
			strcpy(payload, "application/json");

			msg = compute_post_request(host, route, payload,
				transmit, 1, NULL, 0);
			
			// adaugarea header-ului Authorization
			char *p = strstr(msg, "Host");
			int i = 0;
			while (p[i] != '\n') {
				i++;
			}
			char *message_aux = (char*) malloc(MESSAGELEN * sizeof(char));
			strncpy(message_aux, msg, strlen(msg) - strlen(p));
			strcat(message_aux, "Authorization: Bearer ");
			strcat(message_aux, jwt);
			strcat(message_aux, "\r\n");
			strcat(message_aux, p);

    		send_to_server(sockfd, message_aux);
    		r = receive_from_server(sockfd);
    		puts(r);

		} else if (strcmp(buffer, "get_book\n") == 0) {
			char id[20];
			cout << "id=";
			fgets(id, 20, stdin);
			id[strlen(id) - 1] = '\0';

			char route[30];
			strcpy(route, "/api/v1/tema/library/books/");
			strcat(route, id);

			sockfd = open_connection(host, 8080,
				AF_INET, SOCK_STREAM, 0);

			// sending with the cookie
			char *cookie[1];
			cookie[0] = (char *) malloc(CLEN * sizeof(char));
			strcpy(cookie[0], session_cookie);

			msg = compute_get_request(host, route, NULL, cookie, 1);

			msg = compute_get_request(host, route, NULL, cookie, 1);
			puts(msg);
			
			// adaugarea header-ului Authorization
			char *p = strstr(msg, "Host");
			int i = 0;
			while (p[i] != '\n') {
				i++;
			}
			char *message = (char*) malloc(MESSAGELEN * sizeof(char));
			strncpy(message, msg, strlen(msg) - strlen(p));
			strcat(message, "Authorization: Bearer ");
			strcat(message, jwt);
			strcat(message, "\r\n");
			strcat(message, p);

    		send_to_server(sockfd, message);
    		r = receive_from_server(sockfd);
    		puts(r);

    		std::string aux_str;
    		json jmsg = json::parse(strstr(r, "[{\""));
    		cout << jmsg << endl;
		} else if (strcmp(buffer, "get_books\n") == 0) {
			char route[30];
			strcpy(route, "/api/v1/tema/library/books");

			sockfd = open_connection(host, 8080,
				AF_INET, SOCK_STREAM, 0);

			// sending with the cookie
			char *cookie[1];
			cookie[0] = (char *) malloc(CLEN * sizeof(char));
			strcpy(cookie[0], session_cookie);

			msg = compute_get_request(host, route, NULL, cookie, 1);
			
			// adaugarea header-ului Authorization
			char *p = strstr(msg, "Host");
			int i = 0;
			while (p[i] != '\n') {
				i++;
			}
			char *message = (char*) malloc(MESSAGELEN * sizeof(char));
			strncpy(message, msg, strlen(msg) - strlen(p));
			strcat(message, "Authorization: Bearer ");
			strcat(message, jwt);
			strcat(message, "\r\n");
			strcat(message, p);

    		send_to_server(sockfd, message);
    		r = receive_from_server(sockfd);
    		puts(r);
		} else if (strcmp(buffer, "enter_library\n") == 0) {
			char route[30];
			strcpy(route, "/api/v1/tema/library/access");

			sockfd = open_connection(host, 8080,
				AF_INET, SOCK_STREAM, 0);

			// sending with the cookie
			char *cookie[1];
			cookie[0] = (char *) malloc(CLEN * sizeof(char));
			strcpy(cookie[0], session_cookie);

			msg = compute_get_request(host, route, NULL, cookie, 1);
			puts(msg);
    		send_to_server(sockfd, msg);
    		r = receive_from_server(sockfd);
    		puts(r);
    		// extrag mesajul json in loc sa
    		// parsez manual
    		std::string aux_str;
    		json jmsg = json::parse(strstr(r, "{\""));
    		aux_str = jmsg["token"];
    		strcpy(jwt, aux_str.c_str());
    		cout << endl;
		} else if (strcmp(buffer, "login\n") == 0) {
			char username[20], password[20];
			cout << "username=";
			fgets(username, 20, stdin);
			username[strlen(username) - 1] = '\0';
			cout << "password=";
			fgets(password, 20, stdin);
			password[strlen(password) - 1] = '\0';

			sockfd = open_connection(host, 8080,
				AF_INET, SOCK_STREAM, 0);

			json jmsg;
			jmsg["username"] = username;
			jmsg["password"] = password;
			string message = jmsg.dump();
			transmit[0] = (char *) malloc(BUFFLEN * sizeof(char));
			strcpy(transmit[0], message.c_str());

			char route[30], payload[30];
			strcpy(route, "/api/v1/tema/auth/login");
			strcpy(payload, "application/json");

			msg = compute_post_request(host, route, payload,
				transmit, 1, NULL, 0);
			puts(msg);
    		send_to_server(sockfd, msg);
    		r = receive_from_server(sockfd);
    		puts(r);

    		// parsez raspunsul de la server si
    		// preiau cookie-ul de sesiune
    		if (strstr(r, "Set-Cookie") == NULL)
    			continue;
    		strcpy(aux, (strstr(r, "Set-Cookie")));
    		int i = 0;
    		while (aux[i + 12] != ';' || aux[i + 13] != ' ') {
    			session_cookie[i] = aux[i + 12];
    			i++;
    		}
    		session_cookie[i] = '\0';
		} else if (strcmp(buffer, "register\n") == 0) {
			// comanda register - similara cu login
			// doar ca fara cookie
			char username[20], password[20];
			cout << "username=";
			fgets(username, 20, stdin);
			username[strlen(username) - 1] = '\0';
			cout << "password=";
			fgets(password, 20, stdin);
			password[strlen(password) - 1] = '\0';

			sockfd = open_connection(host, 8080,
				AF_INET, SOCK_STREAM, 0);

			json jmsg;
			jmsg["username"] = username;
			jmsg["password"] = password;
			string message = jmsg.dump();
			transmit[0] = (char *) malloc(BUFFLEN * sizeof(char));
			strcpy(transmit[0], message.c_str());

			char route[30], payload[30];
			strcpy(route, "/api/v1/tema/auth/register");
			strcpy(payload, "application/json");
			
			msg = compute_post_request(host, route, payload,
				transmit, 1, NULL, 0);
			puts(msg);
    		send_to_server(sockfd, msg);
    		r = receive_from_server(sockfd);
    		puts(r);
		}
	}
	return 0;
}