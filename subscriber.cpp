#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "helpers.h"

int sv_port;

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    return 0;
}