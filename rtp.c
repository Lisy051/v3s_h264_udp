#include "rtp.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <endian.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define RTP_HEADER_LEN  12

struct RTPMsg {
	uint8_t version;
	uint8_t payload_type;
	uint16_t sequence;
	uint32_t timestamp;
	uint32_t ssrc_id;
    uint8_t tx_buffer[65535];
};

static struct RTPMsg rtp_pack;

/**
 * @brief 发送rtp数据包
 * 
 * @param pack_data         h264数据
 * @param pack_size         数据大小
 * @param socket_handle     udp句柄
 * @param dst_address       接收地址
 * @param max_size          单包最大长度
 */
void send_rtp_pack(uint8_t* pack_data, uint32_t pack_size, int socket_handle,
    struct sockaddr* dst_address, int addr_len, uint32_t max_size, uint32_t tick)
{
    static uint16_t rtp_sequence = 0;
    static uint32_t rtp_timestamp = 0;

    rtp_timestamp += tick;

    memset(&rtp_pack, 0 , sizeof(rtp_pack));
	rtp_pack.version = 0x80;
	rtp_pack.payload_type = 0x60;
	rtp_pack.timestamp = htobe32(rtp_timestamp);
	rtp_pack.ssrc_id = 0xDEADBEEF;

    uint8_t prefix = 4;
    if (pack_data[2] == 0x01)
        prefix = 3;
    pack_data += prefix;
    pack_size -= prefix;

    if (pack_size > max_size + prefix)
    {
        uint8_t nal_type_avc = pack_data[0] & 0x1F;
        uint8_t nal_type_hevc = (pack_data[0] >> 1) & 0x3F;
        uint8_t nal_bits_avc = pack_data[0] & 0xE0;
        uint8_t nal_bits_hevc = pack_data[0] & 0x81;

        bool start_bit = true;
        uint8_t tx_size = 2;

        while (pack_size)
        {
            uint32_t chunk_size = pack_size > max_size ? max_size : pack_size;
            if (nal_type_avc == 1 || nal_type_avc == 5)
            {
                rtp_pack.tx_buffer[0] = nal_bits_avc | 28;
                rtp_pack.tx_buffer[1] = nal_type_avc;

                if (start_bit) 
                {
                    pack_data++;
                    pack_size--;
                    rtp_pack.tx_buffer[1] = 0x80 | nal_type_avc;
                    start_bit = false;
                }
                else if (chunk_size == pack_size)
                {
                    rtp_pack.tx_buffer[1] |= 0x40;
                }
                else
                {
                    rtp_pack.tx_buffer[1] &= 0x3F;
                }
            }
            
            if (nal_type_hevc == 1 || nal_type_hevc == 19)
            {
                rtp_pack.tx_buffer[0] = nal_bits_hevc | 49 << 1;
                rtp_pack.tx_buffer[1] = 1;
                rtp_pack.tx_buffer[2] = nal_type_hevc;
                tx_size = 3;

                if (start_bit)
                {
                    pack_data += 2;
                    pack_size -= 2;
                    rtp_pack.tx_buffer[2] = 0x80 | nal_type_hevc;
                    start_bit = false;
                }
                else if (chunk_size == pack_size)
                {
                    rtp_pack.tx_buffer[2] |= 0x40;
                }
                else
                {
                    rtp_pack.tx_buffer[2] &= 0x3F;
                }
            }

            memcpy(rtp_pack.tx_buffer + tx_size, pack_data, chunk_size);
	        rtp_pack.sequence = htobe16(rtp_sequence++);
            pack_data += chunk_size;
            pack_size -= chunk_size;
            if (pack_size == 0)
                rtp_pack.payload_type = 0xe0;
            int ret = sendto(socket_handle, &rtp_pack.version, chunk_size + tx_size + RTP_HEADER_LEN,
                             0, dst_address, addr_len);
            //fprintf(stderr, "rtp send len %d, %d\n",data_len, ret);
        }
    }
    else
    {
        memcpy(rtp_pack.tx_buffer, pack_data, pack_size);
        rtp_pack.sequence = htobe16(rtp_sequence++);
        int ret = sendto(socket_handle, &rtp_pack.version, pack_size + RTP_HEADER_LEN,
                         0, dst_address, addr_len);
        //fprintf(stderr, "rtp send len %d, %d\n",pack_size + RTP_HEADER_LEN, ret);
    }
}
