#ifndef __STRUCT_H__
#define __STRUCT_H__

#include <string>

#define MAX_ID 4
/* Add 1 in case the topic string equals to 50, we'll need to manually
 * append the null-terminator */
#define MAX_TOPIC 51
#define MAX_IP 16

typedef struct {
    /* TCP Socket FD associated with this subscriber */
    int fd;
    /* Subscriber's unique ID */
    char id[MAX_ID];
    /* Client's subscribed topics */
    std::vector<std::string> topics;
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
    /* max string length + 1 in case the string received isn't null-terminated,
     * we'll need an aditional byte to manually add the null-terminator  */
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