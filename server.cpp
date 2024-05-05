#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "helpers.h"

/* For disabling Nagle's algorithm */
#include <netinet/tcp.h>

int sv_port;

/* Receives an address to an IPv4 sockaddr structure and sets its
 * fields accordingly, port will be the one hardcoded in the checker,
 * i.e. (argv[1]), ip address will be 0.0.0.0 so the internal kernel
 * networking implementation will take care of it, could've more meticulously
 * set it to 127.0.0.1 as it's hardcoded in the checker but this works too
 * @param sv_addr address to the stack allocated sockaddr struc used in
 *                either the tcp/udp socket creation funcs
 */
void get_basic_sock_addr_in_template(struct sockaddr_in *sv_addr) {
    /* Good practice to firstly memset everything to 0, as i've seen in lab 7 */
    memset(&sv_addr, 0, sizeof(sv_addr));
    
    sv_addr->sin_family = AF_INET;
    sv_addr->sin_port = htons(sv_port);
    sv_addr->sin_addr.s_addr = INADDR_ANY;
}

/* Creates a TCP socket where the server will be listening for
 * new connections from subscribers
 * @return TCP socket fd
 */
int create_sv_connections_sock() {
    int sv_tcp_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sv_tcp_sock_fd < 0, "RIP sv_tcp_sock_fd creation\n");

    /* Disable Nagle's algorithm */
    int disable = 1;
    int ret = setsockopt(sv_tcp_sock_fd, IPPROTO_TCP, TCP_NODELAY, &disable, sizeof(int));
    DIE(ret < 0, "setsockopt - nagle\n");

    struct sockaddr_in sv_addr;
    get_basic_sock_addr_in_template(&sv_addr);
    
    /* Bind socket fd to the port and server ip address as given
     * in the homework */
    ret = bind(sv_tcp_sock_fd, (const struct sockaddr *)&sv_addr, sizeof(struct sockaddr_in));
    DIE(ret < 0, "bind in tcp socket\n");

    return sv_tcp_sock_fd;
}

/* Creates an UDP socket where the UDP clients will send new topics
 * @return UDP socket fd
 */
int create_sv_udp_sock() {
    int sv_udp_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(sv_udp_sock_fd < 0, "RIP sv_udp_sock_fd creation\n");

    struct sockaddr_in sv_addr;
    get_basic_sock_addr_in_template(&sv_addr);

    /* Bind socket fd to the port and server ip address as given
     * in the homework */
    int ret = bind(sv_udp_sock_fd, (const struct sockaddr *)&sv_addr, sizeof(struct sockaddr_in));
    DIE(ret < 0, "bind in udp socket");

    return sv_udp_sock_fd;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    
    sv_port = atoi(argv[1]);

    int sv_tcp_sock_fd = create_sv_connections_sock();
    int sv_udp_sock_fd = create_sv_udp_sock();

    return 0;
}