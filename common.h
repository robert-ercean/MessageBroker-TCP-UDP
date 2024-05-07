#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/tcp.h> /* For disabling Nagle's algorithm */
#include <vector>
#include <iostream>
#include <algorithm> 

#include "strucs.h"

#define MAX_LEN 1 << 12
#define MAX_LINE 1 << 8
#define MAX_UDP_PACKET_LEN (51 + 1 + 1500)

#define EXIT "exit"
#define SUBSCRIBE "subscribe"
#define UNSUBSCRIBE "unsubscribe"

#define CONNECT_ACTION 1
#define SHUTDOWN_CLOSE 2  
#define SUBSCRIBE_ACTION 3
#define NOTIFICATION_ACTION 4
#define SHUTDOWN_INTRUDER 5  

#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3

#define CLEAR_BUFFER(buff, len) memset(buff, 0, len)

void get_basic_sock_addr_in_template(struct sockaddr_in *sv_addr, int port, uint32_t ip);
int create_sv_tcp_sock(int port, uint32_t ip);
int create_sv_udp_sock(int port, uint32_t ip);

void build_tcp_hdr(tcp_hdr **msg_hdr, char type);

int send_tcp_packet(int sockfd, void *buff);
int recv_tcp_packet(int sockfd, void *buff);

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
	if (assertion) {                                                           \
	  fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
	  perror(call_description);                                                \
	  exit(EXIT_FAILURE);                                                      \
	}                                                                          \
  } while (0)

#endif
