#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

/* Receives an address to an IPv4 sockaddr structure and sets its
 * fields accordingly, port will be the one hardcoded in the checker,
 * i.e. (argv[1]), ip address will be 0.0.0.0 so the internal kernel
 * networking implementation will take care of it, could've more meticulously
 * set it to 127.0.0.1 as it's hardcoded in the checker but this works too
 * @param sv_addr address to the stack allocated sockaddr struc used in
 *                either the tcp/udp socket creation funcs
 */
void get_basic_sock_addr_in_template(struct sockaddr_in *sv_addr, int port, uint32_t ip) {
    /* Good practice to firstly memset everything to 0, as i've seen in lab 7 */
    memset(sv_addr, 0, sizeof(sv_addr));

    sv_addr->sin_family = AF_INET;
    sv_addr->sin_port = htons(port);
    sv_addr->sin_addr.s_addr = ip;
}

int recv_tcp_packet(int sockfd, void *buffer) {
	size_t bytes_received = 0;
	size_t bytes_remaining = sizeof(tcp_hdr);
	char *buff = (char *)buffer;

	while(bytes_remaining) {
		char *ptr_to_be_recv = buff + bytes_received;

		size_t bytes_received_with_one_call = recv(sockfd, ptr_to_be_recv, bytes_remaining, 0);
		DIE (bytes_received_with_one_call < 0, "recv_tcp_packet failed in header!\n");

		bytes_received += bytes_received_with_one_call;
		bytes_remaining -= bytes_received_with_one_call;
	}

	tcp_hdr *hdr = (tcp_hdr *)buffer;
	int payload_len = hdr->len;
	bytes_remaining = payload_len;

	while(bytes_remaining) {
		char *ptr_to_be_recv = buff + bytes_received;

		size_t bytes_received_with_one_call = recv(sockfd, ptr_to_be_recv, bytes_remaining, 0);
		DIE (bytes_received_with_one_call < 0, "recv_tcp_packet failed in payload!\n");

		bytes_received += bytes_received_with_one_call;
		bytes_remaining -= bytes_received_with_one_call;
	}

	return bytes_received;
}

int send_tcp_packet(int sockfd, void *buffer) {
	tcp_hdr *hdr = (tcp_hdr *)buffer;
	int payload_len = hdr->len;
	int len = payload_len + sizeof(tcp_hdr);	

	size_t bytes_sent = 0;
	size_t bytes_remaining = len;
	char *buff = (char *)buffer;
	
	while (bytes_remaining) {
		char *ptr_to_be_sent = (buff + bytes_sent);
		
		size_t bytes_sent_with_one_call = send(sockfd, ptr_to_be_sent, bytes_remaining, 0);
		DIE (bytes_sent_with_one_call < 0, "send_tcp_packet failed!\n");

		bytes_sent += bytes_sent_with_one_call;
		bytes_remaining -= bytes_sent_with_one_call;
	}
	return bytes_sent;
}
