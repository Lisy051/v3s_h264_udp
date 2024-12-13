#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <stdbool.h>

typedef struct 
{
    int width;
    int height;
    int fps;
    int rotation;
    char sensor[32];
}camera_config_t;

typedef struct
{
    int h264_Maxqp;                 // 0~51
    int h264_Minqp;                 // 0~39
    int h264_Level;                 // 10~51
    int h264_Profile;               // 66:VENC_H264ProfileBaseline 77:VENC_H264ProfileMain 100:VENC_H264ProfileHigh
    int h264_EntropyCodingCABAC;    // 0:CAVLC 1:CABAC
    int h264_CodingMode;            // VENC_FIELD_CODING VENC_FRAME_CODING
    int h264_MaxKeyInterval;
    int h264_Bitrate;
    int h264_BlockNumber;
}encode_config_t;


typedef struct
{
    bool enable_udp;
    bool enable_pipe;
    bool enable_file;

    int pack_len;

    int udp_port;
    char udp_addr[16];
    char file_dir[255];
}output_contig_t;

extern camera_config_t camera_config;
extern encode_config_t encode_config;
extern output_contig_t output_contig;

void Config_Init(void);

/*
#define ENABLE_DLOG
#define ENABLE_SAVE 0

#define G_BUF_COUNT 3
#define G_WIDTH 1920
#define G_HEIGHT 1080
#define G_V4L2_PIX_FMT V4L2_PIX_FMT_NV12
#define G_FPS 30

#define G_FRAMES 450	// 15 seconds at 30fps

#define G_CEDARC_PIX_FMT VENC_PIXEL_YUV420SP
*/

#endif
