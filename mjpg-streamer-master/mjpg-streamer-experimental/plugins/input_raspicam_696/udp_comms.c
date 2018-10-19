/*
 * Copyright (C) 2017 by Daniel Clouse.
 *
 * This file is dual licensed: you can use it either under the terms of
 * the GPL, or the BSD license, at your option.
 *
 *  a) This library is free software; you can redistribute it and/or
 *     modify it under the terms of the GNU General Public License as
 *     published by the Free Software Foundation; either version 2 of the
 *     License, or (at your option) any later version.
 *
 *     This library is distributed in the hope that it will be useful, 
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 *
 *     You should have received a copy of the GNU General Public
 *     License along with this library; if not, write to the Free
 *     Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 *     MA 02110-1301 USA
 *
 * Alternatively,
 *
 *  b) Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *     1. Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *     2. Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 *     THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *     CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *     INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *     MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *     DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 *     CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *     SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *     NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 *     HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *     CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *     OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 *     EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <dirent.h>
#include <syslog.h>
#include "udp_comms.h"
#include "get_ip_addr_str.h"
#include "log_error.h"
#include "get_usecs.h"

#define PICAM_PORT 9696

#define MSECS_PER_SECOND 1000


static inline int64_t timeval_diff_usecs(struct timeval a,
                                         struct timeval b) {
    int64_t diff_sec = a.tv_sec - b.tv_sec;
    int64_t diff_usec = a.tv_usec - b.tv_usec;
    return diff_sec * 1000000 + diff_usec;
}


#define MAX_HOSTNAME 256

/**
 * All the arguments to the comms message loop thread (loop_start()).
 */
typedef struct {
    Udp_Comms* comms_ptr;
    const char* hostname;
    int port_number;
    int max_samples;
    int64_t max_round_trip_usecs;
} Udp_Comms_Thread_Args;


/**
 * Return true if the socket addresses and ports are eqaul.
 */
static bool sockaddr_equal(const struct sockaddr* sa,
                           const struct sockaddr* sb) {
    if (sa->sa_family != sb->sa_family) return false;
    if (sa->sa_family == AF_INET) {
        const struct sockaddr_in* sap = (const struct sockaddr_in*)sa;
        const struct sockaddr_in* sbp = (const struct sockaddr_in*)sb;
        return (memcmp((char*)&sap->sin_addr, (char*)&sap->sin_addr,
                       sizeof(sap->sin_addr)) == 0 &&
                sap->sin_port == sbp->sin_port);
    } else if (sa->sa_family == AF_INET6) {
        const struct sockaddr_in6* sap = (const struct sockaddr_in6*)sa;
        const struct sockaddr_in6* sbp = (const struct sockaddr_in6*)sb;
        return (memcmp((char*)&sap->sin6_addr, (char*)&sap->sin6_addr,
                       sizeof(sap->sin6_addr)) == 0 &&
                sap->sin6_port == sbp->sin6_port);
    } else {
        return false;
    }
}


/**
 * Return the index into the comms_ptr->client array for the socket address
 * and port which match *addr_ptr.
 *
 * If there is no matching client, create a new entry to represent it.
 *
 * A return value of -1 indicates out of memory.
 *
 */
static int lookup_client_info(Udp_Comms* comms_ptr,
                              const struct sockaddr* addr_ptr,
                              socklen_t addr_len) {
    int i;
    for (i = 0; i < comms_ptr->client_count; ++i) {
        if (sockaddr_equal(addr_ptr,
            (const struct sockaddr*)&comms_ptr->client[i].saddr)) {
            return i;
        }
    }
    char ip_addr_str[INET6_ADDRSTRLEN+20];
    get_ip_addr_str(addr_ptr, ip_addr_str, INET6_ADDRSTRLEN+20);
    int64_t cam_host_usec = get_usecs();

    if (comms_ptr->client_count >= MAX_CLIENTS) {
        LOG_ERROR("at %.3f, too many udp clients; can't add %s\n",
                  cam_host_usec / (float)USECS_PER_SECOND, ip_addr_str);
        return -1;
    }

    /* Add a new client. */

    LOG_STATUS("at %.3f, adding upd client_no %d: %s\n",
               cam_host_usec / (float)USECS_PER_SECOND,
               comms_ptr->client_count, ip_addr_str);

    Client_Info* p = &comms_ptr->client[comms_ptr->client_count];
    memcpy(&p->saddr, addr_ptr, addr_len);
    p->saddr_len = addr_len;
    p->client_start_time_msec = INT64_MIN;
    p->min_offset_usec = INT64_MAX;
    p->sum_half_round_trip_usec = 0;
    p->mean_half_round_trip_usec = 0;
    p->last_ping_usec = 0;
    p->sample_count = 0;
    p->failed_sends = 0;
    p->failed_pings = 0;
    p->log_next_at = 1;
    p->is_disconnected = false;
    pthread_mutex_lock(&comms_ptr->lock_mutex);
    ++comms_ptr->client_count;
    pthread_mutex_unlock(&comms_ptr->lock_mutex);
    return comms_ptr->client_count - 1;
}


/**
 * Send a message to client_no.
 */
ssize_t udp_comms_send(Udp_Comms* comms_ptr,
                       int client_no,
                       const void* buf,
                       size_t len) {
    pthread_mutex_lock(&comms_ptr->lock_mutex);
    Client_Info* client_ptr = &comms_ptr->client[client_no];
    ssize_t retval = sendto(
                    comms_ptr->fd, buf, len, 0,
                    (const struct sockaddr*)&client_ptr->saddr,
                    client_ptr->saddr_len);
    pthread_mutex_unlock(&comms_ptr->lock_mutex);
    if (retval < 0) {
        client_ptr->is_disconnected = true;
        ++client_ptr->failed_sends;
        if (client_ptr->failed_sends == client_ptr->log_next_at) {
            char ip_addr_str[INET6_ADDRSTRLEN+20];
            get_ip_addr_str((const struct sockaddr*)&client_ptr->saddr,
                            ip_addr_str, INET6_ADDRSTRLEN+20);
            int64_t cam_host_usec = get_usecs();
            printf("failed_sends=%d log_next_at=%d\n", client_ptr->failed_sends, client_ptr->log_next_at);
            LOG_ERROR(
            "at %.3f, can't sendto udp client_no %d (%s) %d times; errno= %d\n",
                     cam_host_usec / (float)USECS_PER_SECOND,
                     client_no, client_ptr->log_next_at,
                     ip_addr_str, errno)
            client_ptr->log_next_at *= 10;
        }
    } else {
        client_ptr->is_disconnected = false;
        client_ptr->log_next_at = 1;
    }
    return retval;
}


/**
 * Send a Request_Time message to client_no.
 */
static int send_request_time(Udp_Comms* comms_ptr, int client_no) {
    Request_Time msg_out;
    int64_t usec = get_usecs();

    msg_out.msg_id = ID_REQUEST_TIME;
    msg_out.cam_host_usec = usec;
    //printf("send REQUEST_TIME %lld to %d\n", usec, client_no);
    return udp_comms_send(comms_ptr, client_no, &msg_out, sizeof(msg_out));
}


/**
 * Handle an incoming Echo_Time message.
 */
static int handle_echo_time(Udp_Comms* comms_ptr,
                            int client_no,
                            Echo_Time* msg_in_ptr,
                            int64_t arrive_usec,
                            int max_samples,
                            int64_t max_round_trip_usecs) {
    /*
    printf("recv ECHO_TIME %lld %lld %lld from %d\n",
           msg_in_ptr->cam_host_usec, msg_in_ptr->client_msec, arrive_usec,
           client_no);
    */
    Client_Info* client_ptr = &comms_ptr->client[client_no];
    int64_t round_trip_usecs = arrive_usec - msg_in_ptr->cam_host_usec;
    if (round_trip_usecs < 0) return -1;
    client_ptr->last_ping_usec = arrive_usec;
    if (client_ptr->client_start_time_msec == INT64_MIN) {
        client_ptr->client_start_time_msec = msg_in_ptr->client_msec;
    }
    if (round_trip_usecs < max_round_trip_usecs) {
        // Accumulate statistics.
        int64_t client_usecs = 1000 * (msg_in_ptr->client_msec -
                                       client_ptr->client_start_time_msec);
        int64_t offset = msg_in_ptr->cam_host_usec + round_trip_usecs / 2 -
                         client_usecs ;
        if (offset < client_ptr->min_offset_usec) {
            client_ptr->min_offset_usec = offset;
        }
        client_ptr->sum_half_round_trip_usec += round_trip_usecs;
        ++client_ptr->sample_count;
        if (client_ptr->sample_count >= max_samples) {
            client_ptr->mean_half_round_trip_usec =
                                        client_ptr->sum_half_round_trip_usec /
                                        (client_ptr->sample_count * 2);
            if (client_ptr->sample_count == max_samples) {
                int64_t cam_host_usec = get_usecs();
                int64_t client_msec = to_client_msecs(client_ptr,
                                                      cam_host_usec);


#define MAX_ERRMSG 128
                char errmsg[MAX_ERRMSG];
                LOG_STATUS("at %.3f, clock synched udp client_no %d %s;\n",
                    cam_host_usec / (float)USECS_PER_SECOND, client_no,
                    get_ip_addr_str((const struct sockaddr*)&client_ptr->saddr,
                                    errmsg, MAX_ERRMSG));
                LOG_STATUS("client_secs= %.3f one-way= %lld usecs\n",
                    client_msec / (float)MSECS_PER_SECOND,
                    client_ptr->mean_half_round_trip_usec);

                /* Tell any waiting task that a connection has been
                   established. */

                pthread_mutex_lock(&comms_ptr->lock_mutex);
                pthread_cond_signal(&comms_ptr->cond_have_connection);
                pthread_mutex_unlock(&comms_ptr->lock_mutex);
            }
        }
    }
    if (client_ptr->sample_count < max_samples) {
        return send_request_time(comms_ptr, client_no);
    }
    return 0;
}


/**
 * Read and handle all incoming messages until a fatal error occurs.
 */
static int message_loop(Udp_Comms* comms_ptr,
                        const char* default_hostname,
                        int default_port_number,
                        int max_samples,
                        int64_t max_round_trip_usecs) {
    int ret_val = -1;

    // Initialize remaining fields of *comms_ptr.

    comms_ptr->client_count = 0;

    // Open and bind the socket.

    comms_ptr->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (comms_ptr->fd == -1) {
        LOG_ERROR("can't socket; errno=%d; %s\n", strerror(errno));
        goto quit;
    }
    int optval = 1;
    (void)setsockopt(comms_ptr->fd, SOL_SOCKET, SO_REUSEPORT, &optval,
                     sizeof(optval));

    struct sockaddr_in si_me;
    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PICAM_PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(comms_ptr->fd, (const struct sockaddr *)&si_me,
             sizeof(si_me)) == -1) {
        close(comms_ptr->fd);
        LOG_ERROR("can't bind; errno=%d; %s\n", errno, strerror(errno));
        goto quit;
    }

    // Set up default client from default_hostname.

    if (default_hostname != NULL) {
        char port_str[32];
        struct addrinfo* client_info;
        struct addrinfo hints;
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
        hints.ai_socktype = SOCK_DGRAM;
        sprintf(port_str, "%d", default_port_number);
        int status = getaddrinfo(default_hostname, port_str, &hints,
                                 &client_info);
        if (status != 0) {
            LOG_ERROR("can't getaddrinfo(%s) %s\n",
                      default_hostname, gai_strerror(status));
        } else {
            struct addrinfo* p = client_info;
            while (p != NULL) {
                (void)lookup_client_info(comms_ptr, p->ai_addr, p->ai_addrlen);
                p = p->ai_next;
            }
            freeaddrinfo(client_info);
        }
    }

    while (1) {
#define MAX_PACKET_BYTES 128
        char msg_in_buf[MAX_PACKET_BYTES];
        char ip_addr_str[INET6_ADDRSTRLEN+20];

        /* Wait until a new message arrives.  Timeout after one second. */

        fd_set set;
        FD_ZERO(&set);
        FD_SET(comms_ptr->fd, &set);
        struct timeval timeout = { 1, 0 };
        int status = select(comms_ptr->fd+1, &set, NULL, NULL, &timeout);
        struct timeval arrival_time;
        int64_t arrive_usec = get_usecs();
        float timestamp = arrive_usec / (float)USECS_PER_SECOND;
        if (status == -1) {
            LOG_ERROR(
                 "at %.3f, can't select; errno= %d; quitting message_loop\n",
                      timestamp, errno);
            break;
        } else if (status == 0) {
            /* Timeout.  No one's talking.  Send Request_Time to all clients. */

            int j;
            for (j = 0; j < comms_ptr->client_count; ++j) {
                if (comms_ptr->client[j].sample_count < max_samples) {
                    get_ip_addr_str(
                        (const struct sockaddr*)&comms_ptr->client[j].saddr,
                                    ip_addr_str, INET6_ADDRSTRLEN+20);
                    LOG_ERROR(
         "at %.3f, clock sync with udp client_no %d %s timeout; resending\n",
                              timestamp, j, ip_addr_str);
                }
                (void)send_request_time(comms_ptr, j);
            }
        } else if (status == 1) {
            /* A message is ready to read. */

            struct sockaddr_storage tmp_addr;
            socklen_t tmp_addr_len = sizeof(struct sockaddr_storage);
            ssize_t bytes = recvfrom(comms_ptr->fd, &msg_in_buf,
                                     MAX_PACKET_BYTES, 0,
                                     (struct sockaddr*)&tmp_addr,
                                     &tmp_addr_len);
            if (bytes < 1) {
                LOG_ERROR("at %.3f, can't recvfrom; errno= %d\n",
                          timestamp, errno);
            } else {
                int client_no = lookup_client_info(
                                            comms_ptr,
                                            (const struct sockaddr*)&tmp_addr,
                                            tmp_addr_len);
                if (client_no >= 0) {
                    switch (msg_in_buf[0]) {
                    case ID_ECHO_TIME:
                        (void)handle_echo_time(comms_ptr, client_no,
                                               (Echo_Time*)msg_in_buf,
                                               arrive_usec, max_samples,
                                               max_round_trip_usecs);
                        break;
                    default:
                        get_ip_addr_str(
                 (const struct sockaddr*)&comms_ptr->client[client_no].saddr,
                                        ip_addr_str, INET6_ADDRSTRLEN+20);
                        LOG_ERROR(
                        "at %.3f, unexpected message %d udp client_no %d %s\n",
                                  timestamp, msg_in_buf[0], client_no,
                                  ip_addr_str);
                    }
                }
            }
        } else {
            LOG_ERROR(
            "at %.3f, select returned %d; errno= %d; quitting message_loop\n",
                      timestamp, status, errno);
            break;
        }
    }
quit:
    return ret_val;
}


/**
 * Start up the message_loop().
 */
static void* loop_start(void* void_args_ptr) {
    Udp_Comms_Thread_Args* args_ptr = (Udp_Comms_Thread_Args*)void_args_ptr;
    (void)message_loop(args_ptr->comms_ptr, args_ptr->hostname,
                       args_ptr->port_number, args_ptr->max_samples,
                       args_ptr->max_round_trip_usecs);
    return NULL;
}


/**
 * Send the same message to all client_nos.
 */


int64_t get_cam_host_usec(Udp_Comms* comms_ptr) {
    return get_usecs();
}

ssize_t udp_comms_send_blobs_to_all(Udp_Comms* comms_ptr,
                                    int64_t cam_host_usec,
                                    Udp_Blob_List* blob_list_ptr) {
    bool some_success = false;
    int client_no;
    pthread_mutex_lock(&comms_ptr->lock_mutex);
    for (client_no = 0; client_no < comms_ptr->client_count; ++client_no) {
        Client_Info* client_ptr = &comms_ptr->client[client_no];
        blob_list_ptr->client_msec = to_client_msecs(client_ptr, cam_host_usec);
        // Send only the used array elements.

        size_t used_size = offsetof(Udp_Blob_List, blob) +
                           blob_list_ptr->blob_count * sizeof(Blob_Stats);
        if (sendto(comms_ptr->fd, blob_list_ptr, used_size, 0,
                   (const struct sockaddr*)&client_ptr->saddr,
                   client_ptr->saddr_len) == 0) {
            some_success = true;
        } else {
            ++client_ptr->failed_sends;
            if (client_ptr->failed_sends == client_ptr->log_next_at) {
                int64_t cam_host_usec = get_usecs();
                char ip_addr_str[INET6_ADDRSTRLEN+20];
                get_ip_addr_str((const struct sockaddr*)&client_ptr->saddr,
                                ip_addr_str, INET6_ADDRSTRLEN+20);
                printf("failed_sends=%d log_next_at=%d\n", client_ptr->failed_sends, client_ptr->log_next_at);
                LOG_ERROR(
          "at %.3f, can't sendto udp client_no %d (%s) %d times; errno= %d\n",
                          cam_host_usec / (float)USECS_PER_SECOND,
                          client_no, ip_addr_str, client_ptr->log_next_at,
                          errno)
                client_ptr->log_next_at *= 10;
            }
        }
    }
    pthread_mutex_unlock(&comms_ptr->lock_mutex);
    if (some_success) return 0;
    return -1;
}

int udp_comms_connection_count_no_mutex(Udp_Comms* comms_ptr) {
    int connection_count = 0;
    int ii;
    for (ii = 0; ii < comms_ptr->client_count; ++ii) {
        if (!comms_ptr->client[ii].is_disconnected &&
            comms_ptr->client[ii].sample_count > 0) {
            ++connection_count;
        }
    }
    return connection_count;
}

/**
 * Return the number of clients we are connected to.
 */
int udp_comms_connection_count(Udp_Comms* comms_ptr) {
    int connection_count;
    pthread_mutex_lock(&comms_ptr->lock_mutex);
    connection_count = udp_comms_connection_count_no_mutex(comms_ptr);
    pthread_mutex_unlock(&comms_ptr->lock_mutex);
    return connection_count;
}


/**
 * Return the age (in usecs) of the oldest ping response of all the connected
 * clients.
 */
int64_t udp_comms_age_of_oldest_ping_response(Udp_Comms* comms_ptr) {
    pthread_mutex_lock(&comms_ptr->lock_mutex);
    int64_t oldest = INT64_MAX;
    int ii;
    for (ii = 0; ii < comms_ptr->client_count; ++ii) {
        if (!comms_ptr->client[ii].is_disconnected &&
            comms_ptr->client[ii].last_ping_usec < oldest) {
            oldest = comms_ptr->client[ii].last_ping_usec;
        }
    }
    pthread_mutex_unlock(&comms_ptr->lock_mutex);
    int64_t now = get_usecs();
    int64_t age = now - oldest;
    if (age < 0) age = 0;
    return age;
}


int udp_comms_construct(Udp_Comms* comms_ptr,
                        const char* default_hostname,
                        int port_number,
                        int max_samples,
                        int64_t max_round_trip_usecs,
                        bool block_till_connection) {
    int ret_val = -1;

    // Fill in arguments to loop_start().

    Udp_Comms_Thread_Args args;
    args.comms_ptr = comms_ptr;
    args.hostname = default_hostname;
    args.port_number = port_number;
    args.max_samples = max_samples;
    args.max_round_trip_usecs = max_round_trip_usecs;

    // Initialize shared fields of *comms_ptr.

    int status = pthread_mutex_init(&comms_ptr->lock_mutex, NULL);
    if (status != 0) {
        LOG_ERROR("can't pthread_mutex_init (%d)\n", status);
        goto quit;
    }
    status = pthread_cond_init(&comms_ptr->cond_have_connection, NULL);
    if (status != 0) {
        LOG_ERROR("can't pthread_cond_init (%d)\n", status);
        goto quit;
    }

    // Start message_loop thread.

    status = pthread_create(&comms_ptr->comms_loop_thread, NULL,
                            loop_start, &args);
    if (status != 0) {
        LOG_ERROR("can't pthread_create (%d)\n", status);
        goto quit;
    }

    ret_val = 0;
    if (block_till_connection) {
        // Wait for someone to make a connection.

        status = pthread_mutex_lock(&comms_ptr->lock_mutex);
        if (status != 0) {
            LOG_ERROR("can't pthread_mutex_lock (%d)\n", status);
            goto quit;
        }
        while (udp_comms_connection_count_no_mutex(comms_ptr) <= 0) {
            status = pthread_cond_wait(&comms_ptr->cond_have_connection,
                                       &comms_ptr->lock_mutex);
            if (status != 0) {
                LOG_ERROR("can't pthread_cond_wait (%d)\n", status);
                ret_val = -1;
            }
        }
        status = pthread_mutex_unlock(&comms_ptr->lock_mutex);
        if (status != 0) {
            LOG_ERROR("can't pthread_mutex_unlock (%d)\n", status);
            goto quit;
        }
    }
quit:
    return ret_val;
}


#if 0
//#define DEFAULT_REMOTE_HOSTNAME "127.0.0.1"
#define DEFAULT_REMOTE_HOSTNAME NULL
#define REMOTE_PORT 10696
int main(void)
{
    Udp_Comms udp_comms;
    udp_comms_construct(&udp_comms, DEFAULT_REMOTE_HOSTNAME, REMOTE_PORT, 500,
                        USECS_PER_SECOND / 8, true);
quit:
    return 0;
}
#endif
