#ifndef GET_IP_ADDR_STR_H
#define GET_IP_ADDR_STR_H
#include <netinet/in.h>
const char* get_ip_addr_str(const struct sockaddr* saddr,
                            char* buf,
                            size_t buf_bytes);
#endif
