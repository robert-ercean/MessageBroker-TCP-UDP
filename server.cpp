#include "common.h"

#define EXPECTED_ARGC 2
#define BACKLOG_MAX 64

using namespace std;

int sv_port;
/* sv_fds[0] = STDIN 
 * sv_fds[1] = TCP CONNECTIONS SOCK
 * sv_fds[2] = UDP DGRAMS SOCK */
pollfd *sv_fds;
int sv_fds_count;
char buff[MAX_LEN] = {0};
vector<subscriber> subs;

/* Adds a new pollfd struc to the global server's file descriptors array */
void add_new_pollfd_struc(int fd) {
    struct pollfd new_struc;
    new_struc.fd = fd;
    new_struc.events = POLLIN;
    sv_fds[sv_fds_count] = new_struc;
    sv_fds_count++;
}

/* Adds the basic server's file descriptors to the global fd array:
 * STDIN FD for the exit command that closes the server
 * UDP SOCK FD for receving new topics and payloads
 * TCP SOCK FD for establishing new connections and sending appropiate 
 * messages
 */
void push_basic_fds() {
    sv_fds_count = 0;
    sv_fds = (pollfd *)malloc(BACKLOG_MAX * sizeof(pollfd));
    add_new_pollfd_struc(STDIN_FILENO);
    add_new_pollfd_struc(create_sv_tcp_sock(sv_port, INADDR_ANY));
    add_new_pollfd_struc(create_sv_udp_sock(sv_port, INADDR_ANY));
}

/* Creates a TCP socket where the server will be listening for
 * new connections from subscribers
 * @return TCP SOCKET FD
 */
int create_sv_tcp_sock(int port, uint32_t ip) {
    int sv_tcp_sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sv_tcp_sock_fd < 0, "RIP sv_tcp_sock_fd creation\n");

    /* Disable Nagle's algorithm */
    int disable = 1;
    int ret = setsockopt(sv_tcp_sock_fd, IPPROTO_TCP, TCP_NODELAY, &disable, sizeof(int));
    DIE(ret < 0, "setsockopt - nagle\n");

    /* What happens if a program with open sockets on a given IP address + TCP port combo 
     * closes its sockets, and then a brief time later, a program comes 
     * along and wants to listen on that same IP address and TCP port number?
     * (Typical case: A program is killed and is quickly restarted.)
     * The option below allows the new program to re-bind to that IP/port combo.
     * In stacks with BSD sockets interfaces — essentially all Unixes and Unix-like systems,
     * you have to ask for this behavior by setting the SO_REUSEADDR option
     * via setsockopt() before you call bind().
     * Source: https://stackoverflow.com/questions/3229860/what-is-the-meaning-of-so-reuseaddr-setsockopt-option-linux
     * Chose to use this when I was browsing stackoverflow for multiplexing TCP implementations
     * and came accross a thread where a user was getting a failed socket() call after
     * restarting his program in a short time(less than a second).
     */
    int make_sock_reusable = 1;
    ret = setsockopt(sv_tcp_sock_fd, SOL_SOCKET, SO_REUSEADDR, &make_sock_reusable, sizeof(int));
	DIE (ret < 0, "making addr reusable failed!\n");

    struct sockaddr_in sv_addr;
    get_basic_sock_addr_in_template(&sv_addr, port, ip);
    
    /* Bind socket fd to the port and server ip address as given
     * in the homework */
    ret = bind(sv_tcp_sock_fd, (const struct sockaddr *)&sv_addr, sizeof(struct sockaddr_in));
    DIE(ret < 0, "bind in tcp socket\n");

    return sv_tcp_sock_fd;
}

/* Creates an UDP socket where the UDP clients will send new topics
 * @return UDP SOCKET FD
 */
int create_sv_udp_sock(int port, uint32_t ip) {
    int sv_udp_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    DIE(sv_udp_sock_fd < 0, "RIP sv_udp_sock_fd creation\n");

    struct sockaddr_in sv_addr;
    get_basic_sock_addr_in_template(&sv_addr, port, ip);

    /* Bind socket fd to the port and server ip address as given
     * in the homework */
    int ret = bind(sv_udp_sock_fd, (const struct sockaddr *)&sv_addr, sizeof(struct sockaddr_in));
    DIE(ret < 0, "bind in udp socket");

    return sv_udp_sock_fd;
}

/* Checks if the received ID is taken, if not create a new subscriber
 * structure, if it's taken, print the error message and handle closing
 * the client
 * @return -1 in case of error
 *          the new user ID if succesfull */
char *check_and_add_sub_id(int sockfd) {
    tcp_hdr *packet_hdr = (tcp_hdr *)buff;
    connect_payload *payload = (connect_payload *)(buff + sizeof(tcp_hdr));
    char *recv_id = payload->id;

    for (subscriber sub : subs) {
        /* Subscriber exists in the server's database, but is offline so
         * just adjust his status to "online" and update the new socket FD */
        if (memcmp(sub.id, recv_id, MAX_ID) == 0 && !sub.online) {
            sub.fd = sockfd;
            sub.online = true;
        /* There is already an online subscriber with this ID in the 
         * server's database, so send the shutdown & close signal to
         * the (intruder) user */
        } else if (memcmp(sub.id, recv_id, MAX_ID) == 0 && sub.online) {
            fprintf(stdout, "Client %s already connected.\n", recv_id);
            return NULL;
        }
    }

    subscriber new_sub;
    new_sub.fd = sockfd;
    new_sub.id = recv_id;
    new_sub.online = true;
    new_sub.topics = NULL;
    subs.push_back(new_sub);

    return recv_id;
}

void send_shutdown_to_intruder(int sockfd) {
    CLEAR_BUFFER();
    tcp_hdr *hdr = (tcp_hdr *)buff;
    hdr->action = SHUTDOWN_CLOSE;
    /* We need just the action type, no payload */
    hdr->len = 0;
    send_tcp_packet(sockfd, buff);
}

void start_server() {
    int sv_tcp_sock_fd = sv_fds[1].fd;
    int ret = listen(sv_tcp_sock_fd, BACKLOG_MAX);
    DIE(ret < 0, "listen failed!\n");
    while(true) {
        CLEAR_BUFFER();
        ret = poll(sv_fds, sv_fds_count, -1);
        DIE(ret < 0, "poll failed!\n");
        /* STDIN */
        if ((sv_fds[0].revents & POLLIN) != 0) {
        }
        /* TCP LISTENER SOCK */
        else if ((sv_fds[1].revents & POLLIN) != 0) {
            /* Add new TCP connection socket to the global FD array */
            struct sockaddr_in new_client_addr;
            socklen_t len = sizeof(struct sockaddr_in);
            int new_tcp_con_fd = accept(sv_tcp_sock_fd, (struct sockaddr *)&new_client_addr, &len);
            DIE(new_tcp_con_fd < 0, "accept failed!\n");

            /* Receive the subscriber's ID (TCP CONNECT ACTION TYPE) */
            ret = recv_tcp_packet(new_tcp_con_fd, buff);
            DIE (ret < 0, "recv connect action failed!\n");
            char *ret_id = check_and_add_sub_id(new_tcp_con_fd);
            /* Send the shutdown & close signal to the (intruder)user */
            if (!ret_id) {
                fprintf(stdout, "Sending shutdown signal to intruder!\n");
                send_shutdown_to_intruder(new_tcp_con_fd);
                continue;
            }
            add_new_pollfd_struc(new_tcp_con_fd);
            fprintf(stdout, "New client %s connected from %s:%d.\n",
                ret_id, inet_ntoa(new_client_addr.sin_addr), ntohs(new_client_addr.sin_port));
        }
        /* UDP DATAGRAMS SOCK */
        else if ((sv_fds[2].revents & POLLIN) != 0) {
        }
    }
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    
    DIE(argc != EXPECTED_ARGC, "Wrong argument count!\n");

    CLEAR_BUFFER();

    sv_port = atoi(argv[1]);
    
    push_basic_fds();

    start_server();

    return 0;
}