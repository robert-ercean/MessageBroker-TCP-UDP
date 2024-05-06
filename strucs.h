#ifndef __STRUCT_H__
#define __STRUCT_H__

#define MAX_ID 4

typedef struct __attribute__((__packed__)) {
    /* TCP Socket FD associated with this subscriber */
    int fd;
    /* Subscriber's unique ID */
    char *id;
    /* Client's subscribed topics */
    char **topics;
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


#endif