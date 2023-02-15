
#include <stdio.h>
#include <unistd.h>

#include <vencoder.h>
#include <veInterface.h>
#include <memoryAdapter.h>

#include "cam.h"
#include "util.h"
#include "conf.h"

//add
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static VideoEncoder *gVideoEnc = NULL;
static VencBaseConfig baseConfig;
static int g_width = G_WIDTH;
static int g_height = G_HEIGHT;
static int g_pix_fmt = G_CEDARC_PIX_FMT;
static int g_fps = G_FPS;

//FILE *fpH264 = NULL;

int h264_init(int width, int height, int fps) {

	g_width = width;
	g_height = height;
	g_fps = fps;
/*
	fpH264 = fopen("/mnt/out.h264", "wb");
	if (fpH264 == NULL) {
		dlog(DLOG_CRIT "Error: failed to open /mnt/out.h264 for writing\n");
		return -1;
	}
*/
	VencH264Param h264Param = {
		.bEntropyCodingCABAC = 1,
		.nBitrate = 1 * 1024 * 1024,
		.nFramerate = g_fps,
		.nCodingMode = VENC_FRAME_CODING,
		.nMaxKeyInterval = 30,
		.sProfileLevel.nProfile = VENC_H264ProfileHigh,
		.sProfileLevel.nLevel = VENC_H264Level42,
		.sQPRange.nMinqp = 1,
		.sQPRange.nMaxqp = 30,
	};

	CLEAR(baseConfig);
	baseConfig.memops = MemAdapterGetOpsS();
	if (baseConfig.memops == NULL) {
		dlog(DLOG_CRIT "Error: MemAdapterGetOpsS() failed\n");
		return -1;
	}
	CdcMemOpen(baseConfig.memops);

	baseConfig.nInputWidth = g_width;
	baseConfig.nInputHeight = g_height;
	baseConfig.nStride = g_width;
	baseConfig.nDstWidth = g_width;
	baseConfig.nDstHeight = g_height;
	baseConfig.eInputFormat = g_pix_fmt;

	gVideoEnc = VideoEncCreate(VENC_CODEC_H264);
	if (gVideoEnc == NULL) {
		dlog(DLOG_CRIT "Error: VideoEncCreate() failed\n");
		return -1;
	}

	{
		VideoEncSetParameter(gVideoEnc, VENC_IndexParamH264Param, &h264Param);
		int value = 0;
		VideoEncSetParameter(gVideoEnc, VENC_IndexParamIfilter, &value);
		value = 0;
		VideoEncSetParameter(gVideoEnc, VENC_IndexParamRotation, &value);
		//value = 1;
		//VideoEncSetParameter(gVideoEnc, VENC_IndexParamFastEnc, &value);
		value = 0;
		VideoEncSetParameter(gVideoEnc, VENC_IndexParamSetPSkip, &value);
		VencCyclicIntraRefresh sIntraRefresh;
		sIntraRefresh.bEnable = 1;
    	sIntraRefresh.nBlockNumber = 10;
		//VideoEncSetParameter(gVideoEnc, VENC_IndexParamH264CyclicIntraRefresh, &sIntraRefresh);
	}
	VideoEncInit(gVideoEnc, &baseConfig);

	// Write SPS, PPS NAL units
	//VencHeaderData sps_pps_data;
	//VideoEncGetParameter(gVideoEnc, VENC_IndexParamH264SPSPPS, &sps_pps_data);
	//fwrite(sps_pps_data.pBuffer, 1, sps_pps_data.nLength, fpH264);

	dlog(DLOG_INFO "Info: h264 encocder init OK\n");

	return 0;
}

int h264_encode(unsigned char *addrPhyY, unsigned char *addrPhyC, int socket_descriptor, struct sockaddr_in address) {
	// Prepare buffers
	VencInputBuffer inputBuffer;
	VencOutputBuffer outputBuffer;
	CLEAR(inputBuffer);
	CLEAR(outputBuffer);
	// Pass pre-allocated buffer (from V4L2) to CedarVE
	// No need to use AllocInputBuffer() and copy data unnecessarily
	inputBuffer.pAddrPhyY = addrPhyY;
	inputBuffer.pAddrPhyC = addrPhyC;
	AddOneInputBuffer(gVideoEnc, &inputBuffer);

	if (VideoEncodeOneFrame(gVideoEnc) != VENC_RESULT_OK) {
		dlog("Error: VideoEncodeOneFrame() failed\n");
		return -1;
	}

	// Mark buffer as used, and get output H.264 bitstream
	AlreadyUsedInputBuffer(gVideoEnc, &inputBuffer);
	GetOneBitstreamFrame(gVideoEnc, &outputBuffer);
		/*int PASCAL FAR sendto( SOCKET s, const char FAR* buf, int len, int flags,const struct sockaddr FAR* to, int tolen);　　
		* s：一个标识套接口的描述字。　
		* buf：包含待发送数据的缓冲区。　　
		* len：buf缓冲区中数据的长度。　
		* flags：调用方式标志位。　　
		* to：（可选）指针，指向目的套接口的地址。　
		* tolen：to所指地址的长度。  
		*/
	if(outputBuffer.nFlag == 1)
	{
		// Write SPS, PPS NAL units
		VencHeaderData sps_pps_data;
		VideoEncGetParameter(gVideoEnc, VENC_IndexParamH264SPSPPS, &sps_pps_data);
		sendto(socket_descriptor, sps_pps_data.pBuffer, sps_pps_data.nLength, 0, (struct sockaddr *)&address, sizeof(address));
	}
	if (outputBuffer.nSize0 > 0){
		//fwrite(outputBuffer.pData0, 1, outputBuffer.nSize0, fpH264);
		sendto(socket_descriptor, outputBuffer.pData0, outputBuffer.nSize0, 0, (struct sockaddr *)&address, sizeof(address));
	}
		
	if (outputBuffer.nSize1 > 0){
		//fwrite(outputBuffer.pData1, 1, outputBuffer.nSize1, fpH264);
		sendto(socket_descriptor, outputBuffer.pData1, outputBuffer.nSize1, 0, (struct sockaddr *)&address, sizeof(address));
	}

	FreeOneBitStreamFrame(gVideoEnc, &outputBuffer);
	return 0;
}

void h264_deinit() {
	if (baseConfig.memops) {
		CdcMemClose(baseConfig.memops);
		baseConfig.memops = NULL;
	}
	if (gVideoEnc) {
		ReleaseAllocInputBuffer(gVideoEnc);
		VideoEncDestroy(gVideoEnc);
		gVideoEnc = NULL;
	}
	/*
	if (fpH264) {
		fclose(fpH264);
		fpH264 = NULL;
	}
	*/
}
