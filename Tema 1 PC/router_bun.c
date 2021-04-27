#include <queue.h>
#include "skel.h"

#define BUFF 100

// structure for arp table
typedef struct arp_table_entry {
	uint32_t ip_address;
	uint8_t mac_address[ETH_ALEN];
} arp_entry;

// structure for trie
typedef struct trie {
	char value;
	uint32_t next_hop;
	int interface;
	struct trie *left;
	struct trie *right;
} trieNode;

// structure for a queue entry
// for the vector of queues
typedef struct queue_entry {
	uint32_t address;
	queue q;
} q_entry;

// function to add a simple tree node
trieNode *new_trie_node(int flag) {
	trieNode *new_node;

	new_node = malloc(sizeof(trieNode));
	new_node->value = flag;
	new_node->interface = -1;
	new_node->left = NULL;
	new_node->right = NULL;
	return new_node;
}

// function to add a terminal tree node (here a prefix ends)
trieNode *new_trie_node_terminal(int flag, uint32_t next_hop, int interface) {
	trieNode *new_node;

	new_node = malloc(sizeof(trieNode));
	new_node->value = flag;
	new_node->interface = interface;
	new_node->next_hop = next_hop;
	new_node->left = NULL;
	new_node->right = NULL;
	return new_node;
}

// function to add a node to a tree
void addNodeTrie(trieNode **root, uint32_t prefix, uint32_t mask,
	uint32_t next_hop, int interface) {
	trieNode *node = *root;
	uint32_t aux_prefix, aux = 1;
	int i = 0, j, flag;

	uint32_t prefix_auxiliary = 0, mask_auxiliary = 0;
	prefix_auxiliary = prefix_auxiliary | (prefix << 24);
	prefix_auxiliary = prefix_auxiliary | (((prefix << 8) >> 24) << 8);
	prefix_auxiliary = prefix_auxiliary | (((prefix >> 8) << 24) >> 8);
	prefix_auxiliary = prefix_auxiliary | (prefix >> 24);

	mask_auxiliary = mask_auxiliary | (mask << 24);
	mask_auxiliary = mask_auxiliary | (((mask << 8) >> 24) << 8);
	mask_auxiliary = mask_auxiliary | (((mask >> 8) << 24) >> 8);
	mask_auxiliary = mask_auxiliary | (mask >> 24);

	aux_prefix = prefix_auxiliary & mask_auxiliary;
	while (((aux << i) & mask_auxiliary) == 0) {
		i++;
	}
	for (j = 0; j < (32 - i); j++) {
		if ((aux_prefix & (1 << (31 - j))) == 0) {
			flag = 0;
		}
		else {
			flag = 1;
		}
		if (j != (32 - i - 1)) {
			if (flag == 0) {
				if (node->left != NULL) {
					node = node->left;
				} else {
					node->left = new_trie_node(flag);
					node = node->left;
				}
			} else {
				if (node->right != NULL) {
					node = node->right;
				} else {
					node->right = new_trie_node(flag);
					node = node->right;
				}
			}
		} else {
			if (flag == 0) {
				if (node->left != NULL) {
					node->left->next_hop = next_hop;
					node->left->interface = interface;
				} else {
					node->left = new_trie_node_terminal(flag, next_hop, interface);
				}
			} else {
				if (node->right != NULL) {
					node->right->next_hop = next_hop;
					node->right->interface = interface;
				} else {
					node->right = new_trie_node_terminal(flag, next_hop, interface);
				}
			}
		}
	}
}

// function to find best route in a trie
int find_best_route_trie(uint32_t destination, trieNode* root,
	uint32_t* next_hop, int* interface) {
	trieNode *node = root;
	uint32_t destination_aux = 0;
	int i = 31, flag = 0;

	destination_aux = destination_aux | (destination << 24);
	destination_aux = destination_aux | (((destination << 8) >> 24) << 8);
	destination_aux = destination_aux | (((destination >> 8) << 24) >> 8);
	destination_aux = destination_aux | (destination >> 24);
	destination = destination_aux;
	while (i >= 0) {
		if ((destination & (1 << i)) != 0) {
			if (node->right != NULL) {
				if (node->right->interface != -1) {
					*interface = node->right->interface;
					*next_hop = node->right->next_hop;
					flag = 1;
				}
				node = node->right;
			} else {
				break;
			}
		} else {
			if (node->left != NULL) {
				if (node->left->interface != -1) {
					*interface = node->left->interface;
					*next_hop = node->left->next_hop;
					flag = 1;
				}
				node = node->left;
			} else {
				break;
			}
		}
		i--;
	}
	return flag;
}

//function to parse the routing table
void parse_routing_table(char* filename, trieNode **root) {
	char buffer[100];
	FILE* file = fopen(filename, "r");
	char delim[2] = " ";
	uint32_t prefix, mask, next_hop;
	int interface;

	*root = malloc(sizeof(trieNode));
	(*root)->left = NULL;
	(*root)->right = NULL;
	(*root)->interface = -1;

	if (file) {
		char *p;
		while (fgets(buffer, sizeof(char) * BUFF, file)) {
			p = strtok(buffer, delim);
			// prefixul
			prefix = inet_addr(p);
			p = strtok(NULL, delim);
			// next_hop
			next_hop = inet_addr(p);
			p = strtok(NULL, delim);
			// mask
			mask = inet_addr(p);
			p = strtok(NULL, delim);
			// interface
			interface = atoi(p);
			// add to trie
			addNodeTrie(root, prefix, mask, next_hop, interface);
		}
	}
	fclose(file);
}

// function that finds me an entry in the arp table
// returns NULL if if the address is not found
uint8_t* find(arp_entry* arp_table, uint32_t ip_address, int size) {
	for (int i = 0; i < size; i++) {
		if (ip_address == arp_table[i].ip_address)
			return arp_table[i].mac_address;
	}
	return NULL;
}

// function to get the interface of the router
// by checking its interfaces - for ip headers
int get_interface_router_ip(struct iphdr* ip) {
	for (int i = 0; i < ROUTER_NUM_INTERFACES; i++) {
		if (inet_addr(get_interface_ip(i)) == ip->daddr)
			return i;
	}
	return -1;
}

// function to get the interface of the router
// by checking its interfaces - for arp headers
int get_interface_router_arp(struct arp_header* arp) {
	for (int i = 0; i < ROUTER_NUM_INTERFACES; i++) {
		if (inet_addr(get_interface_ip(i)) == arp->tpa)
			return i;
	}
	return -1;
}

// function to find the mac address in an arp table
uint8_t* find_mac_arp(arp_entry* arp_table, uint32_t address, int size) {
	for (int i = 0; i < size; i++) {
		if (arp_table[i].ip_address == address) {
			return arp_table[i].mac_address;
		}
	}
	return NULL;
}

// function that checks if the arp table contains the entry
// corresponding to the ip address in the arp table
int contains_arp_entry(arp_entry* arp_table, uint8_t* mac_address,
	uint32_t ip_address, int size) {
	int i;

	for (i = 0; i < size; i++) {
		if (arp_table[i].ip_address == ip_address)
			return 1;
	}
	return 0;
}

// function to update the arp table
int update_arp_table(struct arp_header* arp_hdr, arp_entry* arp_table, int *size,
	int *arp_capacity) {
	int i;

	if (contains_arp_entry(arp_table, arp_hdr->sha, arp_hdr->spa, *size))
		return 0;
	if (*arp_capacity <= *size) {
		*arp_capacity *= 2;
		arp_table = realloc(arp_table, *arp_capacity * sizeof(arp_entry));
	}
	arp_table[*size].ip_address = arp_hdr->spa;
	for (i = 0; i < ETH_ALEN; i++)
		arp_table[*size].mac_address[i] = arp_hdr->sha[i];
	(*size)++;
	return 1;
}

// function to add in the vector of queues
void queues_add(packet *m, uint32_t address, q_entry *vector, int *size, int *capacity) {
	int flag = 0;
	for (int i = 0; i < *size; i++) {
		if (vector[i].address == address) {
			queue_enq(vector[i].q, m);
			flag = 1;
		}
	}
	if (flag == 0) {
		q_entry *entry = malloc(sizeof(struct queue_entry));
		if (*size >= *capacity) {
			*capacity *= 2;
			vector = realloc(vector, *capacity * sizeof(struct queue_entry));
		}
		entry->address = address;
		entry->q = queue_create();
		queue_enq(entry->q, m);
		vector[*size] = *entry;
		(*size)++;
	}
}

// function to find the desired queue by comparing the ip
queue find_queue(q_entry *vector, int size, uint32_t address) {
	for (int i = 0; i < size; i++) {
		if (address == vector[i].address) {
			return vector[i].q;
		}
	}
	return NULL;
}

int main(int argc, char *argv[]) {
	packet m;
	int rc;

	init(argc - 2, argv + 2);

	// size of the vector of entries
	trieNode *root = NULL;
	parse_routing_table(argv[1], &root);
	// arp_table;
	int size_arp = 0, arp_capacity = 10;
	arp_entry *arp_table = malloc(sizeof(arp_entry) * arp_capacity);

	// space where I declare variable
	int interface; // the source interface
	uint8_t *mac_interface = malloc(6 * sizeof(uint8_t)); // the mac of the source

	uint8_t *mac_address_destination;
	uint32_t next_hop;
	int interface_next_hop;

	// the queue needed for packets
	int queues_size = 0, queues_capacity = 10;
	struct queue_entry *vector_queue = malloc(queues_capacity * sizeof(struct queue_entry));
	while (1) {
		setvbuf(stdout, NULL, _IONBF, 0);
		rc = get_packet(&m);
		DIE(rc < 0, "get_message");
		/* Students will write code here */

		// extracting the ether_header
		struct ether_header *eth_hdr = (struct ether_header*) m.payload;

		if (ntohs(eth_hdr->ether_type) == ETHERTYPE_IP) {
			// extracting the ip header from msg
			struct iphdr *ip_hdr = (struct iphdr*) (m.payload + sizeof(struct ether_header));
			// getting details about router
			interface = get_interface_router_ip(ip_hdr);
			get_interface_mac(interface, mac_interface);

			if (ip_hdr->ttl <= 1) {
				send_icmp_error(ip_hdr->saddr, ip_hdr->daddr, eth_hdr->ether_dhost,
				eth_hdr->ether_shost, ICMP_TIME_EXCEEDED, 0, m.interface);
				continue;
			}
			// managing the icmp requests
			// if icmp_package is NULL, then it is not an ICMP package
			struct icmphdr* icmp_package = parse_icmp(m.payload);
			if (interface != -1) {
				if (icmp_package != NULL) {
					// check if it is an ICMP ECHO request
					// if it is, then send an ICMP ECHO REPLY
					if (icmp_package->type == 8 && ip_hdr->ttl > 1) {
						send_icmp(ip_hdr->saddr, ip_hdr->daddr, eth_hdr->ether_dhost,
						eth_hdr->ether_shost, 0, 0, m.interface,
						icmp_package->un.echo.id, icmp_package->un.echo.sequence);
						continue;
					}
				}
			}
			// now I verify the checksum - if it is 0, then I
			// drop the package and continue receiving packages
			if(ip_checksum(ip_hdr, sizeof(struct iphdr)) != 0) {
				continue;
			}
			// decrementing the ttl
			ip_hdr->ttl--;
			// now I update the checksum
			ip_hdr->check = 0;
			// now to find the route to the desired host
			if(find_best_route_trie(ip_hdr->daddr, root, &next_hop, &interface_next_hop) == 0) {
				send_icmp_error(ip_hdr->saddr, inet_addr(get_interface_ip(m.interface)),
				eth_hdr->ether_dhost, mac_interface, ICMP_DEST_UNREACH, 0, m.interface);
				continue;	
			}

			// changing the source and destination MACs
			mac_address_destination = find_mac_arp(arp_table, next_hop, size_arp);
			if (mac_address_destination != NULL) {
				build_ethhdr(eth_hdr, mac_interface, mac_address_destination, htons(ETHERTYPE_IP));
				ip_hdr->check = ip_checksum(ip_hdr, sizeof(struct iphdr));
			} else {
				// I put the packet in the queue
				// I make a copy of it
				packet *aux_packet = malloc(sizeof(packet));
				memcpy(aux_packet, &m, sizeof(packet));
				queues_add(aux_packet, next_hop, vector_queue, &queues_size, &queues_capacity);
				get_interface_mac(interface_next_hop, mac_interface);
				for (int j = 0; j < ETH_ALEN; j++) {
					eth_hdr->ether_dhost[j] = 255;
					eth_hdr->ether_shost[j] = mac_interface[j];
				}
				eth_hdr->ether_type = htons(ETHERTYPE_ARP);
				// sending the arp
				send_arp(next_hop, inet_addr(get_interface_ip(interface_next_hop)),
					eth_hdr, interface_next_hop, htons(ARPOP_REQUEST));
				continue;
			}
			// forwarding the packet
			send_packet(interface_next_hop, &m);
		} else if (ntohs(eth_hdr->ether_type) == ETHERTYPE_ARP) {
			// extracting the arp header
			struct arp_header* arp_hdr = parse_arp(m.payload);
			interface = get_interface_router_arp(arp_hdr);
			get_interface_mac(m.interface, mac_interface);
			// checking if it is an ARP Requests
			if (arp_hdr != NULL) {
				if (ntohs(arp_hdr->op) == ARPOP_REQUEST && interface != -1) {
					memcpy(eth_hdr->ether_dhost, arp_hdr->sha, ETH_ALEN);
					for (int j = 0; j < ETH_ALEN; j++) {
						eth_hdr->ether_shost[j] = mac_interface[j];
					}
					send_arp(arp_hdr->spa, inet_addr(get_interface_ip(m.interface)), eth_hdr,
						m.interface, htons(ARPOP_REPLY));
					continue;
				}
			}

			// checking if it is an ARP Reply
			if (arp_hdr != NULL) {
				if (htons(arp_hdr->op) == ARPOP_REPLY) {
					// updating the arp table
					update_arp_table(arp_hdr, arp_table, &size_arp, &arp_capacity);
					packet *p;
					// finding the queue that contains the packages that need to be
					// sent now
					queue aux_queue = find_queue(vector_queue, queues_size, arp_hdr->spa);
					if (aux_queue == NULL)
						continue;
					// taking all the packages in the queue to be transmitted now
					while (queue_empty(aux_queue) == 0) {
						// taking the packet, finding the best route to its
						// destination, modifying the ethernet header and
						// then transmittiong the packet
						p = (packet*) queue_deq(aux_queue);
						struct ether_header *ethernet = (struct ether_header*) p->payload;
						struct iphdr *ip_hdr = (struct iphdr*) (p->payload + sizeof(struct ether_header));
						find_best_route_trie(ip_hdr->daddr, root, &next_hop, &interface_next_hop);
						for (int j = 0; j < ETH_ALEN; j++) {
							ethernet->ether_dhost[j] = arp_hdr->sha[j];
						}
						get_interface_mac(interface_next_hop, ethernet->ether_shost);
						ip_hdr->check = ip_checksum(ip_hdr, sizeof(struct iphdr));
						send_packet(interface_next_hop, p);
					}
				}
			}
		}
	}
}
