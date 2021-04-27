#include <queue.h>
#include "skel.h"

#define BUFF 100

// structure for routing table
typedef struct rtable_entry {
	uint32_t prefix;
	uint32_t next_hop;
	uint32_t mask;
	int interface;
} entry;

typedef struct arp_table_entry {
	uint32_t ip_address;
	uint8_t mac_address[ETH_ALEN];
} arp_entry;

// chestie folositoare probabil
// hwaddr_aton(p, e->mask);

//function to parse the routing table
entry* parse_routing_table(char* filename, int *n) {
	char buffer[100];
	FILE* file = fopen(filename, "r");
	char delim[2] = " ";
	int nr = 0, size = 10;
	entry *vector = malloc(size * sizeof(struct rtable_entry)), *e;

	if (file) {
		char *p;
		while (fgets(buffer, sizeof(char) * BUFF, file)) {
			if (nr >= size) {
				size *= 3;
				vector = realloc(vector, size * sizeof(struct rtable_entry));
			}
			p = strtok(buffer, delim);
			e = malloc(sizeof(struct rtable_entry));
			
			// prefixul
			e->prefix = inet_addr(p);
			p = strtok(NULL, delim);
			
			// next_hop
			e->next_hop = inet_addr(p);
			p = strtok(NULL, delim);

			// mask
			e->mask = inet_addr(p);
			p = strtok(NULL, delim);

			// interface
			e->interface = atoi(p);

			// put in the vector of entries
			vector[nr] = *e;
			nr++;
		}
	}
	fclose(file);
	*n = nr;
	return vector;
}

void print_vector(entry* vector, int n) {
	for (int i = 0; i < n; i++) {
		printf("%x %x %x %d\n", vector[i].prefix, vector[i].next_hop,
			vector[i].mask, vector[i].interface);
	}
}


int main(int argc, char *argv[])
{
	packet m;
	int rc;

	init(argc - 2, argv + 2);

	// size of the vector of entries
	int size = 0;
	entry *rtable = parse_routing_table(argv[1], &size);
	print_vector(rtable, size);

	// arp_table;
	int size_arp = 0, arp_capacity = 10;
	arp_entry *arp_table = malloc(sizeof(arp_entry) * arp_capacity);

	// space where I declare variable
	int interface; // the source interface
	uint32_t ip_interface; // the ip of the source
	uint8_t *mac_interface = NULL; // the mac of the source

	while (1) {
		rc = get_packet(&m);
		DIE(rc < 0, "get_message");
		/* Students will write code here */

		// getting details about router
		interface = m.interface;
		ip_interface = inet_addr(get_interface_ip(interface));
		get_interface_mac(interface, mac_interface);

		// extracting the ip header from msg
		struct iphdr *ip_hdr = (struct iphdr*) (m.payload + sizeof(struct ether_header));

		// extracting the ether_header
		struct ether_header *eth_hdr = (struct ether_header *)m.payload;

		// TODO ARP

		// get the ARP header
		struct arp_header* arp_hdr = parse_arp(m.payload);
		if (arp_hdr != NULL) {
			// ARP request
			if (arp_hdr->op == 2) {
				// update ARP table
				if (arp_capacity > size_arp) {
					arp_capacity *= 3;
					arp_table = realloc(arp_table, sizeof(arp_entry) * arp_capacity);
				}
				arp_table[size_arp].ip_address = arp_hdr->spa;
				for (int i = 0; i < ETH_ALEN; i++) {
					arp_table[size_arp].mac_address[i] = arp_hdr->sha[i];
				}
				size_arp++;
				// now I respond
				send_arp(ip_interface, ip_hdr->saddr, eth_hdr, interface, ARPOP_REQUEST);
			} else if (arp_hdr->op == 1) {
				send_arp(ip_interface, ip_hdr->saddr, eth_hdr, interface, ARPOP_REPLY);
			}
		}

		// TODO- continue ICMP

		// // managing the icmp requests
		// if icmp_package is NULL, then it is not an ICMP package
		// struct icmp_header* icmp_package = parse_icmp(m.payload);
		// if (icmp_package != NULL) {
		// 	// check if it is an ICMP ECHO request
		// 	if (icmp_package->type == 0) {
		// 		send_icmp(ip_hdr->daddr, ip_hdr->saddr, );
		// 	}
		// 	continue;
		// }
	}
}
