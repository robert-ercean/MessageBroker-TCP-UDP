#include "common.h"
#include <regex>

#define EXPECTED_ARGC 2
#define BACKLOG_MAX 64

using namespace std;

int sv_port;
/* sv_fds[0] = STDIN 
 * sv_fds[1] = TCP CONNECTIONS SOCK
 * sv_fds[2] = UDP DGRAMS SOCK */
pollfd *sv_fds;
int sv_fds_count;
int sv_fds_capacity;
char buff[MAX_LEN] = {0};
vector<subscriber> subs;

/* Adds a new pollfd struc to the global server's file descriptors array */
void add_new_pollfd_struc(int fd) {
    if (sv_fds_count == sv_fds_capacity) {
        pollfd *tmp = (pollfd *)realloc(sv_fds, sv_fds_capacity * 2);
        DIE (!tmp, "resizing global fd array failed!\n");
        sv_fds_capacity *= 2;
        sv_fds = tmp;
    }
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
    sv_fds_capacity = BACKLOG_MAX;
    sv_fds = (pollfd *)calloc(BACKLOG_MAX, sizeof(pollfd));
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
    DIE(ret < 0, "setsockopt - nagle failed\n");

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

    int make_sock_reusable = 1;
    int ret = setsockopt(sv_udp_sock_fd, SOL_SOCKET, SO_REUSEADDR, &make_sock_reusable, sizeof(int));
	DIE (ret < 0, "making addr reusable failed!\n");

    struct sockaddr_in sv_addr;
    get_basic_sock_addr_in_template(&sv_addr, port, ip);

    /* Bind socket fd to the port and server ip address as given
     * in the homework */
    ret = bind(sv_udp_sock_fd, (const struct sockaddr *)&sv_addr, sizeof(struct sockaddr_in));
    DIE(ret < 0, "bind in udp socket");

    return sv_udp_sock_fd;
}

/* Checks if the received ID is taken, if not create a new subscriber
 * structure, if it's taken, print the error message and handle closing
 * the client
 * @return -1 in case of error
 *          the new user ID if succesfull */
char *check_and_add_new_sub(int sockfd) {
    tcp_hdr *packet_hdr = (tcp_hdr *)buff;
    connect_payload *payload = (connect_payload *)(buff + sizeof(tcp_hdr));
    char *recv_id = payload->id;
    int id_len = strlen(payload->id) + 1;
    for (subscriber& sub : subs) {
        /* Subscriber exists in the server's database, but is offline so
         * just adjust his status to "online" and update the new socket FD */
        if (strcmp(sub.id, recv_id) == 0 && !sub.online) {
            sub.fd = sockfd;
            sub.online = true;
            return sub.id;
        /* There is already an online subscriber with this ID in the 
         * server's database, so send the shutdown & close signal to
         * the (intruder) user */
        } else if (strcmp(sub.id, recv_id) == 0 && sub.online) {
            fprintf(stdout, "Client %s already connected.\n", recv_id);
            return NULL;
        }
    }

    subscriber new_sub;
    new_sub.fd = sockfd;
    memcpy(new_sub.id, recv_id, id_len);
    new_sub.online = true;
    subs.push_back(new_sub);

    return recv_id;
}

void send_shutdown_to_intruder(int sockfd) {
    CLEAR_BUFFER(buff, MAX_LEN);
    tcp_hdr *hdr = (tcp_hdr *)buff;
    hdr->action = SHUTDOWN_INTRUDER;
    /* We need just the action type, no payload */
    hdr->len = 0;
    send_tcp_packet(sockfd, buff);
}

/* Regex parser handling only one-level and multi-level wildcard options
 * /////// BREAKS when the delimitator isn't a slash (/) *///////
bool matches_topic(char *topic, string& pattern) {
    /* Replace '+' with a regex pattern that matches one topic level */
    regex plus_wc("\\+");
    /* [^/]+ -> match any character that is not a slash up until we reach a slash */
    string match_one_level_wildcard = regex_replace(pattern, plus_wc, "[^/]+");

    /* Replace '*' with a regex pattern that matches multiple topic levels */
    regex star_wc("\\*");
    /* ".*" -> match zero ore more of any character */
    string match_multi_level_wildcard = regex_replace(match_one_level_wildcard, star_wc, ".*");

    /* Add regex specific string starter and terminator */
    string full_regex_pattern = "^" + match_multi_level_wildcard + "$";
    regex pattern_regex(full_regex_pattern);

    /* Match the built regex with the topic */
    return regex_match(topic, pattern_regex);
}

bool sub_is_interesed_in_topic(subscriber& sub, char *topic) {
    for (string subbed_topic : sub.topics) {
        if (matches_topic(topic, subbed_topic))
            return true;
    }
    return false;
}

/* Returns a vector of subs that should be of new additions to the argument
 * topic */
vector<subscriber> get_subs_by_topic(char *topic) {
    vector<subscriber> interested_subs;
    for (auto& sub : subs) {
        if (sub_is_interesed_in_topic(sub, topic) && sub.online) {
            interested_subs.push_back(sub);
        }
    }
    return interested_subs;
}

/* Sends a packet to each subscriber that is subbed to this packet's
 * payload's topic */
void send_packet_to_subscribers(tcp_hdr* packet_hdr) {
    notification *packet = (notification *)((char *)packet_hdr + sizeof(tcp_hdr));
    char *topic = packet->topic;
    vector<subscriber> interested_subs = get_subs_by_topic(topic);
    for (auto sub: interested_subs) {
        send_tcp_packet(sub.fd, packet_hdr);
    }
}

void parse_udp_packet(struct sockaddr *udp_sender_addr) {
    tcp_hdr *hdr = (tcp_hdr *)malloc(sizeof(notification) + sizeof(tcp_hdr));
    notification *packet = (notification *)((char *)hdr + sizeof(tcp_hdr));

    /* Copy the topic string */
    memcpy(packet->topic, buff, 50);
    /* Make sure the topic inside the received UDP-packet is null-terminated */
    packet->topic[50] = '\0';

    /* Copy the data type */
    memcpy(&packet->type, buff + 50, sizeof(char));
    
    /* Decide the next length to be copied, since it the payload's length
     * varies with the data type */
    int len;
    int type = int(*(buff + 50));
    switch (type) {
        case INT: {
            len = 5; /* Sign byte + sizeof (uint32_t)*/
            break;
        }
        case SHORT_REAL: {
            len = 2; /* sizeof(uint16_t) */
            break;
        }
        case FLOAT: {
            len = 6; /* Sign byte + sizeof (uint32_t) + uint8_t*/
            break;
        }
        case STRING: {
            len = 1501; /* Max str length + 1 in case we need to add \0 */
            break;
        }
    }
    /* Copy the payload */
    memcpy(&packet->payload, buff + 51, len);
    
    /* Find the real length of the payload string in case we are 
     * dealing with a string, it can be null terminated before the
     * maximum length, so avoid sending useless bytes, also be sure
     * to add the null terminator in case we're dealing with a maximum 
     * string of length 1500 */
    if (type == STRING) {
        packet->payload[1500] = '\0';
        int actual_len = strlen(packet->payload);
        len = actual_len + 1;
    }

    /* Get the sender's IP addr and port and stuff it inside the packet */
    inet_ntop(AF_INET, udp_sender_addr, packet->ip, MAX_IP);
    sockaddr_in *tmp = (sockaddr_in *)udp_sender_addr;
    packet->port= ntohs(tmp->sin_port);

    /* Adjust the total payload length, as per the protocol desires */
    int total_packet_len = 51 + 1 + 4 + 16 + len;
    hdr->action = NOTIFICATION_ACTION;
    hdr->len = total_packet_len;

    /* Check subscribers subbed to this topic and send this packet to them */
    send_packet_to_subscribers(hdr);
}

/* Fetches the subscriber structure by its TCP SOCKET FD from
 * the global subs array */
subscriber *get_subscriber_by_fd(int tcp_subscriber_fd) {
    for (subscriber& sub : subs) {
        if (sub.fd == tcp_subscriber_fd) {
            return &sub;
        }
    }
    return NULL;
}

/* Fetches the pollfd structure by its TCP SOCKET FD from
 * the global pollfd array */
int get_poll_struc_by_fd(int fd) {
    for (int i = 0; i < sv_fds_count; i++) {
        if (sv_fds[i].fd == fd)
            return i;
    }
    return -1;
}

/* Shutsdown and closes the socket associated with the respecitve subscriber
 * FD and handles the details of this action */
void handle_shutdown_sub(int tcp_subscriber_fd) {
    subscriber *sub = get_subscriber_by_fd(tcp_subscriber_fd);
    DIE(!sub, "get sub failed in handle shutdown!\n");
    shutdown(sub->fd, SHUT_RDWR);
    close(sub->fd);
    /* Mark the user as offline, since he might reconnect later */
    sub->online = false;

    /* Remove the closed FD from the global FD array */
    int idx = get_poll_struc_by_fd(tcp_subscriber_fd);
    DIE(idx == -1, "get poll struc by fd failed in handle shutdown!\n");
    for (int i = idx; i < sv_fds_count - 1; i++) {
        sv_fds[i] = sv_fds[i + 1];
    }
    sv_fds[sv_fds_count - 1] = {0};
    sv_fds_count--;
    fprintf(stdout, "Client %s disconnected.\n", sub->id);
}

/* Manages a subscriber's subscribe action, simply adds the
 * respective topic to that sub's topics array */
void handle_subscribe(int tcp_subscriber_fd, tcp_hdr *hdr) {
    int len = hdr->len;
    subscribe_payload *payload = (subscribe_payload *)((char *)hdr + sizeof(tcp_hdr)); 

    subscriber *sub = get_subscriber_by_fd(tcp_subscriber_fd);
    DIE(sub == NULL, "get_subscriber_by_fd failed in subscribe!\n");

    sub->topics.push_back(payload->topic);
}

/* Manages a subscriber's unsubscribe action, simply removes the
 * respective topic from that sub's topics array */
void handle_unsubscribe(int tcp_subscriber_fd, tcp_hdr *hdr) {
    int len = hdr->len;
    subscribe_payload *payload = (subscribe_payload *)((char *)hdr + sizeof(tcp_hdr)); 

    subscriber *sub = get_subscriber_by_fd(tcp_subscriber_fd);
    DIE(sub == NULL, "get_subscriber_by_fd in unsubscribe failed!\n");

    auto iter = find(sub->topics.begin(), sub->topics.end(), payload->topic);
    DIE(iter == sub->topics.end(), "Error: Subscriber is not subbed to this topic!\n");
    sub->topics.erase(iter);
}

/* Parses the action that was sent through the TCP connection socket 
 * by one of the subscribers */
void parse_tcp_client_message(int tcp_subscriber_fd) {
    int ret = recv_tcp_packet(tcp_subscriber_fd, buff);
    DIE(ret < 0, "recv_tcp_packet failed while parsing tcp client msg!\n");

    tcp_hdr *hdr = (tcp_hdr *)buff;
    switch (hdr->action) {
        case SUBSCRIBE_ACTION: {
            handle_subscribe(tcp_subscriber_fd, hdr);
            break;
        }
        case UNSUBSCRIBE_ACTION: {
            handle_unsubscribe(tcp_subscriber_fd, hdr);
            break;
        }
        case SHUTDOWN_CLOSE: {
            handle_shutdown_sub(tcp_subscriber_fd);
            break;
        }
    }
}

/* Received the exit command from STDIN, time to close every open socket
 * and send each online subscriber the SHUTDOWN__CLOSE signal to tell them
 * it's time to close their client
 * Don't have to worry about offline subscribers since once a subscriber
 * goes offline, we simply remove the repsective FD from the global fds array,
 * so we'll not reach that FD once we iterate in this func */
void shutdown_server() {
    /* Skip STDIN FD, UDP AND TCP LISTENER SOCKET */
    for (int i = 3; i < sv_fds_count; i++) {
        /* Tell the subscriber to close his client before shutting 
         * down the server */
        CLEAR_BUFFER(buff, MAX_LEN);
        tcp_hdr *hdr = (tcp_hdr *)buff;
        hdr->action = SHUTDOWN_CLOSE;
        /* We need just the action type, no payload */
        hdr->len = 0;
        send_tcp_packet(sv_fds[i].fd, buff);

        shutdown(sv_fds[i].fd, SHUT_RDWR);
        close(sv_fds[i].fd);
    }
    /* Shutdown and close the UDP and TCP LISTENER SOCKET */
    int tcp_sock = sv_fds[1].fd;
    int udp_sock = sv_fds[2].fd;
    shutdown(tcp_sock, SHUT_RDWR);
    close(tcp_sock);

    shutdown(udp_sock, SHUT_RDWR);
    close(udp_sock);

    /* Free dynamically allocated memory */
    free(sv_fds);

    /* Bye, bye cruel world! (still better server than valve's servers smh)*/
    exit(EXIT_SUCCESS);
}

void start_server() {
    int sv_tcp_sock_fd = sv_fds[1].fd;
    int ret = listen(sv_tcp_sock_fd, BACKLOG_MAX);
    DIE(ret < 0, "listen failed!\n");
    while(true) {
        CLEAR_BUFFER(buff, MAX_LEN);
        ret = poll(sv_fds, sv_fds_count, -1);
        DIE(ret < 0, "poll failed!\n");
        /* STDIN */
        if ((sv_fds[0].revents & POLLIN) != 0) {
            /* Only possible case is when we receive the shutdown signal */
            shutdown_server();
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
            char *ret_id = check_and_add_new_sub(new_tcp_con_fd);
            /* Send the shutdown & close signal to the (intruder)user */
            if (!ret_id) {
                send_shutdown_to_intruder(new_tcp_con_fd);
                continue;
            }
            add_new_pollfd_struc(new_tcp_con_fd);
            fprintf(stdout, "New client %s connected from %s:%d.\n",
                ret_id, inet_ntoa(new_client_addr.sin_addr), ntohs(new_client_addr.sin_port));
        }
        /* UDP DATAGRAMS SOCK */
        else if ((sv_fds[2].revents & POLLIN) != 0) {
            /* Received new topics from the UDP clients */
            int udp_con_fd = sv_fds[2].fd;
            struct sockaddr from;
            socklen_t addr_len = sizeof(struct sockaddr);
            int ret = recvfrom(udp_con_fd, buff, MAX_UDP_PACKET_LEN, 0, &from, &addr_len);
            DIE(ret < 0 || ret > MAX_UDP_PACKET_LEN, "recvfrom failed!\n");
            parse_udp_packet(&from);
        /* Received a message through the connection sockets with the TCP clients */
        } else if (sv_fds_count > 3) {
            for (int i = 3; i < sv_fds_count; i++) {
                if ((sv_fds[i].revents & POLLIN) != 0) {
                    parse_tcp_client_message(sv_fds[i].fd);
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    
    DIE(argc != EXPECTED_ARGC, "Wrong argument count!\n");

    /* Both the subscriber and the server utilise a global buffer for receving,
     * sending, parsing data */
    CLEAR_BUFFER(buff, MAX_LEN);

    sv_port = atoi(argv[1]);
    
    push_basic_fds();

    start_server();

    return 0;
}