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

int main(int argc, char *argv[])
{
	packet m;
	int rc;

	init(argc - 2, argv + 2);

	// size of the vector of entries
	int size = 0;
	entry *rtable = parse_routing_table(argv[1], &size);

	// arp_table;
	int size_arp = 0, arp_capacity = 10;
	arp_entry *arp_table = malloc(sizeof(arp_entry) * arp_capacity);

	// populate arp_table
	arp_table[0].ip_address = inet_addr("192.168.0.2");
	hwaddr_aton("de:ad:be:ef:00:00", arp_table[0].mac_address);
	arp_table[1].ip_address = inet_addr("192.168.1.2");
	hwaddr_aton("de:ad:be:ef:00:01", arp_table[1].mac_address);
	arp_table[2].ip_address = inet_addr("192.168.2.2");
	hwaddr_aton("de:ad:be:ef:00:02", arp_table[2].mac_address);
	arp_table[3].ip_address = inet_addr("192.168.3.2");
	hwaddr_aton("de:ad:be:ef:00:03", arp_table[3].mac_address);
	arp_table[4].ip_address = inet_addr("192.0.1.1");
	hwaddr_aton("ca:fe:ba:be:00:01", arp_table[4].mac_address);
	arp_table[5].ip_address = inet_addr("192.0.1.2");
	hwaddr_aton("ca:fe:ba:be:01:00", arp_table[5].mac_address);

	// space where I declare variable
	int interface; // the source interface
	uint32_t ip_interface; // the ip of the source
	uint8_t *mac_interface = NULL; // the mac of the source

	while (1) {
		rc = get_packet(&m);
		DIE(rc < 0, "get_message");
		/* Students will write code here */
		// TODO- continue ICMP

		// managing the icmp requests
		if icmp_package is NULL, then it is not an ICMP package
		struct icmp_header* icmp_package = parse_icmp(m.payload);
		if (icmp_package != NULL) {
			// check if it is an ICMP ECHO request
			if (icmp_package->type == 0) {
				send_icmp(ip_hdr->daddr, ip_hdr->saddr, );
			}
			continue;
		}
	}
}
