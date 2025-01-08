#ifndef _RTP_H_
#define _RTP_H_

#include <stdint.h>
#include <arpa/inet.h>

void send_rtp_pack(uint8_t* pack_data, uint32_t pack_size, int socket_handle,
    struct sockaddr* dst_address, uint32_t max_size);

#endif
