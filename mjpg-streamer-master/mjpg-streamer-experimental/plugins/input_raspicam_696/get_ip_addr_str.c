#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include "get_ip_addr_str.h"

const char* get_ip_addr_str(const struct sockaddr* saddr,
                            char* buf,
                            size_t buf_bytes) {
    int family = 0;
    void* addr_ptr = NULL;
    unsigned short port = 0;
    switch (saddr->sa_family) {
    case AF_INET6:
        family = AF_INET6;
        addr_ptr = &((struct sockaddr_in6*)saddr)->sin6_addr;
        port = ntohs(((struct sockaddr_in6*)saddr)->sin6_port);
        break;
    case AF_INET:
        family = AF_INET;
        addr_ptr = &((struct sockaddr_in*)saddr)->sin_addr;
        port = ntohs(((struct sockaddr_in*)saddr)->sin_port);
        break;
    }

    if (addr_ptr == NULL) {
        strncpy(buf, "<bad family>", buf_bytes);
    } else {
        if (inet_ntop(family, addr_ptr, buf, buf_bytes) == NULL) {
            snprintf(buf, buf_bytes, "<errno %d>", errno);
            buf[buf_bytes-1] = '\0';
        } else {
#define MAX_PORT_STR 20
            char port_str[MAX_PORT_STR];
            snprintf(port_str, MAX_PORT_STR, "/%u", port);
            port_str[MAX_PORT_STR-1] = '\0';
            strncat(buf, port_str, buf_bytes);
        }
    }
    return buf;
}
