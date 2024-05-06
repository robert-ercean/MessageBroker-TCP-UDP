#include "common.h"

#define EXPECTED_ARGC 4
/* One for STDIN, another for the TCP socket */
#define SUBSCRIBER_MAX_CONS 2

using namespace std;

int sv_port;
char *sv_ip;
char *id;
int connection_fd;
/* sub_fds[0] -> STDIN
 * sub_fds[1] -> TCP SV CONN SOCKET */
struct pollfd sub_fds[SUBSCRIBER_MAX_CONS];
char buff[MAX_LEN];

/* Parses the server port, ip and unique subscriber ID */
void parse_args(char **argv) {
    id = argv[1];
    sv_ip = argv[2];
    sv_port = atoi(argv[3]);
}

/* Adds the STDIN and TCP connection socket FD to the global FD array */
void push_basic_fds() {
    sub_fds[0].fd = STDIN_FILENO;
    sub_fds[0].events = POLLIN;
    
    sub_fds[1].fd = connection_fd;
    sub_fds[1].events = POLLIN;
}

int create_sub_tcp_sock() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(fd < 0, "sub tcp socket creation failed!\n");

    struct sockaddr_in addr;
    get_basic_sock_addr_in_template(&addr, sv_port, inet_addr(sv_ip));

    socklen_t len = sizeof(struct sockaddr_in);
    int ret = connect(fd, (const sockaddr *)&addr, len);
    DIE(ret < 0, "connect subscriber failed!\n");


    return fd;
}

/* Creates the TCP socket through which the subscriber and the server
 * will communicate and sends a TCP packet to the server of type CONNECT that is
 * telling the server that this subscriber wants to establish a subscription
 * */
void connect_to_server() {
    connection_fd = create_sub_tcp_sock();

    int len = sizeof(connect_payload) + sizeof(tcp_hdr);
    char buffer[len] = {0};

    tcp_hdr *hdr = (tcp_hdr *)buffer;
    hdr->action = CONNECT_ACTION;
    hdr->len = sizeof(connect_payload);
    
    connect_payload *payload = (connect_payload *)(buffer + sizeof(tcp_hdr));
    memcpy(payload->id, id, MAX_ID);
    /* Send to the server the CONNECT ACTION TCP type packet that contains
     * the TCP header and the unique ID of the subscriber*/
    send_tcp_packet(connection_fd, buffer);
}

void shutdown_and_close_conn_fd() {
    int ret = shutdown(connection_fd, SHUT_RDWR);
    DIE(ret == -1, "shutdown error!\n");
    close(connection_fd);
    exit(0);
}

void run_subscriber() {
    while (true) {
        poll(sub_fds, SUBSCRIBER_MAX_CONS, -1);
        /* STDIN */
        if ((sub_fds[0].revents & POLLIN) != 0) {
            
        }
        /* TCP SV CONN SOCK */
        else if ((sub_fds[1].revents & POLLIN) != 0) {
            int ret = recv_tcp_packet(connection_fd, buff);
            tcp_hdr *packet = (tcp_hdr *)buff;
            switch (packet->action) {
                case SHUTDOWN_CLOSE: {
                    shutdown_and_close_conn_fd();
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc != EXPECTED_ARGC, "Wrong argument count!\n");

    CLEAR_BUFFER();

    parse_args(argv);

    connect_to_server();

    push_basic_fds();

    run_subscriber();

    return 0;
}