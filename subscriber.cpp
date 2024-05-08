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
 * sub_fds[1] -> TCP SUBSCRIBER-SERVER SOCKET */
struct pollfd sub_fds[SUBSCRIBER_MAX_CONS];
char buff[MAX_LEN];
char topic[MAX_TOPIC];

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
 * telling the server that this subscriber wants to establish a connection
 * */
void connect_to_server() {
    connection_fd = create_sub_tcp_sock();

    int len = sizeof(connect_payload) + sizeof(tcp_hdr);

    tcp_hdr *hdr = (tcp_hdr *)buff;
    hdr->action = CONNECT_ACTION;
    hdr->len = strlen(id) + 1;
    
    connect_payload *payload = (connect_payload *)(buff + sizeof(tcp_hdr));
    memcpy(payload->id, id, hdr->len);
    /* Send to the server the CONNECT ACTION TCP type packet that contains
     * the TCP header and the unique ID of the subscriber*/
    send_tcp_packet(connection_fd, buff);
}

/* Shutdown the socket, close it and then exit the program */
void shutdown_and_close_conn_fd() {
    int ret = shutdown(connection_fd, SHUT_RDWR);
    DIE(ret == -1, "shutdown error!\n");
    close(connection_fd);
    exit(0);
}

/* Received the exit command through stdin, so signal the server
 * we're closing the socket so it can do it too on the other end */
void handle_shutdown() {
    CLEAR_BUFFER(buff, MAX_LEN);

    tcp_hdr *hdr = (tcp_hdr *)buff;
    hdr->len = 0;
    hdr->action = SHUTDOWN_CLOSE;
    int ret = send_tcp_packet(connection_fd, buff);
    DIE(ret < 0, "send tcp packet failed in handle shutdown!\n");

    shutdown_and_close_conn_fd();
}

void subscribe() {
    /* Ignore string "subscribe" */
    char *tmp = buff;
    tmp += sizeof(SUBSCRIBE);
    sscanf(tmp, "%s", topic);

    /* Build TCP header with payload of type SUBSCRIBE ACTION */
    CLEAR_BUFFER(buff, MAX_LEN);
    tcp_hdr *hdr = (tcp_hdr *)buff;
    hdr->action = SUBSCRIBE_ACTION;
    hdr->len = strlen(topic) + 1; /* Include null terminator */
    subscribe_payload *payload = (subscribe_payload *)(buff + sizeof(tcp_hdr));
    memcpy(payload->topic, topic, strlen(topic) + 1);
    
    /* Send packet on its way, bye-bye! */
    int ret = send_tcp_packet(connection_fd, buff);
    DIE(ret != (sizeof(tcp_hdr) + strlen(topic) + 1),
    "wrong number of bytes send in subscribe action!\n");
    fprintf(stdout, "Subscribed to topic %s\n", payload->topic);
}

void unsubscribe() {
    /* Ignore string "unsubscribe" */
    char *tmp = buff;
    tmp += sizeof(UNSUBSCRIBE);
    sscanf(tmp, "%s", topic);

    /* Build TCP header with payload of type SUBSCRIBE ACTION */
    CLEAR_BUFFER(buff, MAX_LEN);
    tcp_hdr *hdr = (tcp_hdr *)buff;
    hdr->action = UNSUBSCRIBE_ACTION;
    hdr->len = strlen(topic) + 1; /* Include null terminator */
    subscribe_payload *payload = (subscribe_payload *)(buff + sizeof(tcp_hdr));
    memcpy(payload->topic, topic, strlen(topic) + 1);
    
    /* Send packet on its way, bye-bye! */
    int ret = send_tcp_packet(connection_fd, buff);
    DIE(ret != (sizeof(tcp_hdr) + strlen(topic) + 1),
    "wrong number of bytes send in subscribe action!\n");
    fprintf(stdout, "Unsubscribed to topic %s\n", payload->topic);
}

void parse_stdin_command() {
    fgets(buff, MAX_LINE, stdin);
    buff[strlen(buff) - 1] = '\0'; /* Remove newline */
    
    if (strncmp(EXIT, buff, 4) == 0) {
        handle_shutdown();
    } else if (strncmp(SUBSCRIBE, buff, 9) == 0) {
        subscribe();
    } else if (strncmp(UNSUBSCRIBE, buff, 11) == 0) {
        unsubscribe();
    } else {
        fprintf(stderr, "Wrong command format!\n");
    }
}

void parse_notification() {
    tcp_hdr *packet_hdr = (tcp_hdr *)buff;
    notification *packet = (notification *)((char *)buff + sizeof(tcp_hdr));

    fprintf(stdout, "%s:%d - %s - ", packet->ip, packet->port, packet->topic);

    switch(packet->type) {
        case INT: {
            char sgn = packet->payload[0];
            uint32_t num = ntohl(*((uint32_t *)(packet->payload + 1)));
            num = (sgn == 1) ? -num : num;
            fprintf(stdout, "INT - %d\n", num);
            break;
        }
        case SHORT_REAL: {
            uint16_t num = ntohs(*((uint16_t *)packet->payload));
            float tmp = (float) num / 100;
            fprintf(stdout, "SHORT_REAL - %.2f\n", tmp);
            break;
        }
        case FLOAT: {
            char sgn = packet->payload[0];
            uint32_t num = ntohl(*((uint32_t *)(packet->payload + 1)));
            uint8_t exp = *((uint8_t *)(packet->payload + 5));
            double float_value = (float) num;
            double offset = pow(10, exp);
            double result = (float_value / offset);
            result = (sgn == 1) ? -result : result;
            fprintf(stdout, "FLOAT - %.*f\n", exp, result);
            break;
        }
        case STRING: {
            fprintf(stdout, "STRING - %s\n", packet->payload);
            break;
        }
    }
}

void run_subscriber() {
    while (true) {
        CLEAR_BUFFER(buff, MAX_LEN);
        CLEAR_BUFFER(topic, MAX_TOPIC);
        poll(sub_fds, SUBSCRIBER_MAX_CONS, -1);
        /* STDIN */
        if ((sub_fds[0].revents & POLLIN) != 0) {
            parse_stdin_command();
        }
        /* TCP SV CONN SOCK */
        else if ((sub_fds[1].revents & POLLIN) != 0) {
            int ret = recv_tcp_packet(connection_fd, buff);
            tcp_hdr *packet = (tcp_hdr *)buff;
            switch (packet->action) {
                /* This signal indicates that we tried to connect to the server
                 * with an already existing ID, so we need to shutdown & close
                 * the socket and this (intruder) subscriber's program */
                case SHUTDOWN_INTRUDER: {
                    shutdown_and_close_conn_fd();
                    break;
                }
                case NOTIFICATION_ACTION: {
                    parse_notification();
                    break;
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    DIE(argc != EXPECTED_ARGC, "Wrong argument count!\n");

    CLEAR_BUFFER(buff, MAX_LEN);

    parse_args(argv);

    connect_to_server();

    push_basic_fds();

    run_subscriber();

    return 0;
}