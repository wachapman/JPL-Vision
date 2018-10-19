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
#ifndef UDP_COMMS_H
#define UDP_COMMS_H

#include <stdint.h>
#include <pthread.h>
#include <sys/socket.h>
#include "detect_color_blobs.h"

#define ID_REQUEST_TIME 1
#define ID_ECHO_TIME 2
#define ID_UDP_BLOB_LIST 2
typedef struct {
    signed char msg_id;
    signed char filler[7];
    int64_t cam_host_usec;
} Request_Time;

typedef struct {
    signed char msg_id;
    signed char filler[7];
    int64_t cam_host_usec;
    int64_t client_msec;
} Echo_Time;

#define MAX_UDP_BLOBS 20
typedef struct {
    signed char msg_id;
    signed char filler[3];
    int blob_count;
    int64_t client_msec;
    Blob_Stats blob[MAX_UDP_BLOBS];
} Udp_Blob_List;


/**
 * A description of a particular remote host that we are talking to.
 *
 * How to communicate with this remote host, and how to predict what the
 * remote host clock will say by looking at our clock.
 */
typedef struct {
    struct sockaddr_storage saddr;     /// remote host internet address
    socklen_t saddr_len;               /// bytes of used portion of saddr
    int64_t client_start_time_msec;    /// start time on remote host clock
    /* estimate of offset between our clock and theirs */
    int64_t min_offset_usec;
    int64_t sum_half_round_trip_usec;  /// sum of message one way trip times
    int64_t mean_half_round_trip_usec; /// mean message one way trip time
    int64_t last_ping_usec;            /// time of most recent arrived msg
    int sample_count;                  /// number of time samples collected
    int failed_sends;                  /// number of send failures
    int failed_pings;                  /// number of send failures
    int log_next_at;                   /// number of failed sends between logs
    bool is_disconnected;
} Client_Info;

/**
 * The maximum number of remote hosts we can talk to.
 *
 * Any remote host that tries to communicate with us above this number will
 * simply be ignored.
 */
#define MAX_CLIENTS 10

/**
 * A description of all the remote hosts.
 *
 * This is the main object used by this module.  It includes variables common
 * to all remote hosts (mutex) and specific info about each remote
 * host.
 */
typedef struct {
    struct timeval start_time;            /// time on our clock when we started
    Client_Info client[MAX_CLIENTS];      /// info about each remote host
    int client_count;                     /// used size of client array
    int fd;                               /// socket file descriptor
    pthread_t comms_loop_thread;          /// thread id of the comms loop thread
    pthread_mutex_t lock_mutex;           /// mutual exclusion lock
    /// signals when each remote host connects
    pthread_cond_t cond_have_connection;
} Udp_Comms;


/**
 * Construct a new Udp_Comms object.
 *
 * @param comms_ptr [out]            Points to space for the constructed
 *                                   object.
 * @param default_hostname [in]      A string containing the IP address or
 *                                   hostname for the default host.  A message
 *                                   is sent to this host once per second
 *                                   trying to establish a connection.  If
 *                                   NULL, there is no default host.
 * @param port_number [in]           The port number to use in UDP
 *                                   communications.
 * @param max_samples [in]           The number of time samples to collect
 *                                   to establish clock synchronization.
 * @param max_round_trip_usecs [in]  The maximum acceptable round trip message
 *                                   time in usecs during clock synchronization.
 *                                   Messages that take longer than this will
 *                                   be dropped.
 * @param block_till_connection [in] If true, wait till a remote host
 *                                   successfully connects before returning.
 */
int udp_comms_construct(Udp_Comms* comms_ptr,
                        const char* default_hostname,
                        int port_number,
                        int max_samples,
                        int64_t max_round_trip_usecs,
                        bool block_till_connection);

/**
 * Send a message to client_no.
 */
ssize_t udp_comms_send(Udp_Comms* comms_ptr,
                       int client_no,
                       const void* buf,
                       size_t len);

/**
 * Send the same message to all client_nos.
 */
/*
ssize_t udp_comms_send_to_all(Udp_Comms* comms_ptr,
                              const void* buf,
                              size_t len);
*/

/**
 * Send a Udp_Blob_List message to all client_nos.
 */
ssize_t udp_comms_send_blobs_to_all(Udp_Comms* comms_ptr,
                                    int64_t cam_host_usec,
                                    Udp_Blob_List* blob_list_ptr);

/**
 * Return the number of clients we are connected to.
 */
int udp_comms_connection_count(Udp_Comms* comms_ptr);

/**
 * Return the age (in usecs) of the oldest ping response of all the connected
 * clients.
 */
int64_t udp_comms_age_of_oldest_ping_response(Udp_Comms* comms_ptr);

/**
 * Convert camera host usecs to client msecs.
 *
 * Start_time must be subtracted off of cam_host_usec before this routine is
 * called.
 */
static inline int64_t to_client_msecs(Client_Info* client_ptr,
                                      int64_t cam_host_usec) {
    return (cam_host_usec + client_ptr->mean_half_round_trip_usec -
            client_ptr->min_offset_usec) / 1000 +
            client_ptr->client_start_time_msec;
}


/**
 * Get the current cam_host_usec time.
 */
int64_t get_cam_host_usec(Udp_Comms* comms_ptr);

#endif
