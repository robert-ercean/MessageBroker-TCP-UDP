#ifndef __STRUCT_H__
#define __STRUCT_H__

#define MAX_ID 4
/* Add 1 in case the topic string equals to 50, we'll need to manually
 * append the null-terminator */
#define MAX_TOPIC 51
#define MAX_IP 16

typedef struct __attribute__((__packed__)) {
    /* TCP Socket FD associated with this subscriber */
    int fd;
    /* Subscriber's unique ID */
    char *id;
    /* Client's subscribed topics */
    char **topics;
    /* Indicates the number of topics this client is subscribed to */
    int topics_count;
    /* Flag indicating whether the client is online / offline */
    bool online;
} subscriber;

typedef struct __attribute__((__packed__)) {
    char action; /* Flag indicating the type of the payload structure */
    int len; /* Size of the payload, ignoring the header */
} tcp_hdr;

typedef struct __attribute__((__packed__)) {
    char id[MAX_ID];
} connect_payload;

typedef struct __attribute__((__packed__)) {
    char topic[MAX_TOPIC];
} subscribe_payload;

typedef struct __attribute__((__packed__)) {
    char topic[MAX_TOPIC];
    unsigned int type;
    char payload[1501];
} udp_packet;

typedef struct __attribute__((__packed__)) {
    char topic[MAX_TOPIC];
    char type;
    unsigned int port;
    char ip[MAX_IP];
    char payload[1500];
} notification;

#endif