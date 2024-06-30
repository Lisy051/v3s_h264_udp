
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <linux/videodev2.h>
#include <vencoder.h>
#include <veInterface.h>
#include <memoryAdapter.h>
#include "config.h"
#include "output.h"

static VideoEncoder *gVideoEnc = NULL;
static VencBaseConfig baseConfig;

int h264_init()
{
	VencH264Param h264Param = {
		.bEntropyCodingCABAC = encode_config.h264_EntropyCodingCABAC,
		.nBitrate = encode_config.h264_Bitrate,
		.nFramerate = camera_config.fps,
		.nCodingMode = encode_config.h264_CodingMode,
		.nMaxKeyInterval = encode_config.h264_MaxKeyInterval,
		.sProfileLevel.nProfile = encode_config.h264_Profile,
		.sProfileLevel.nLevel = encode_config.h264_Level,
		.sQPRange.nMinqp = encode_config.h264_Minqp,
		.sQPRange.nMaxqp = encode_config.h264_Maxqp,
	};

	memset(&baseConfig, 0, sizeof(baseConfig));
	baseConfig.memops = MemAdapterGetOpsS();
	if (baseConfig.memops == NULL) {
		fprintf(stderr, "Error: MemAdapterGetOpsS() failed\n");
		return -1;
	}
	CdcMemOpen(baseConfig.memops);

	baseConfig.nInputWidth = camera_config.width;
	baseConfig.nInputHeight = camera_config.height;
	baseConfig.nStride = camera_config.width;
	baseConfig.nDstWidth = camera_config.width;
	baseConfig.nDstHeight = camera_config.height;
	baseConfig.eInputFormat = VENC_PIXEL_YUV420SP;

	gVideoEnc = VideoEncCreate(VENC_CODEC_H264);
	if (gVideoEnc == NULL) {
		fprintf(stderr, "Error: VideoEncCreate() failed\n");
		return -1;
	}

	VideoEncSetParameter(gVideoEnc, VENC_IndexParamH264Param, &h264Param);
	int value = 1;
	VideoEncSetParameter(gVideoEnc, VENC_IndexParamIfilter, &value);
	VideoEncSetParameter(gVideoEnc, VENC_IndexParamFastEnc, &value);
	VideoEncSetParameter(gVideoEnc, VENC_IndexParamRotation, &camera_config.rotation);
	value = 0;
	VideoEncSetParameter(gVideoEnc, VENC_IndexParamSetPSkip, &value);

	if (encode_config.h264_BlockNumber > 0)
	{
		VencCyclicIntraRefresh sIntraRefresh;
		sIntraRefresh.bEnable = 1;
		sIntraRefresh.nBlockNumber = encode_config.h264_BlockNumber;
		VideoEncSetParameter(gVideoEnc, VENC_IndexParamH264CyclicIntraRefresh, &sIntraRefresh);
	}

	VideoEncInit(gVideoEnc, &baseConfig);
	return 0;
}

int h264_encode(unsigned char *addrPhyY, unsigned char *addrPhyC) {
	// Prepare buffers
	VencInputBuffer inputBuffer;
	VencOutputBuffer outputBuffer;
	char AUD[6] = {0x00, 0x00, 0x00, 0x01, 0x09, 0xF0};
	int ret = 0;
	memset(&inputBuffer, 0, sizeof(inputBuffer));
	memset(&outputBuffer, 0, sizeof(outputBuffer));
	// Pass pre-allocated buffer (from V4L2) to CedarVE
	// No need to use AllocInputBuffer() and copy data unnecessarily
	inputBuffer.pAddrPhyY = addrPhyY;
	inputBuffer.pAddrPhyC = addrPhyC;

	AddOneInputBuffer(gVideoEnc, &inputBuffer);
	ret = VideoEncodeOneFrame(gVideoEnc);
	if (ret != VENC_RESULT_OK) {
		fprintf(stderr, "Error: VideoEncodeOneFrame() failed %d\n",ret);
		return -1;
	}
	// Mark buffer as used, and get output H.264 bitstream
	AlreadyUsedInputBuffer(gVideoEnc, &inputBuffer);
	GetOneBitstreamFrame(gVideoEnc, &outputBuffer);

	if (outputBuffer.nSize0 > 0){
		if(outputBuffer.nFlag == 1)
		{
			VencHeaderData sps_pps_data;
			// Write SPS, PPS NAL units
			VideoEncGetParameter(gVideoEnc, VENC_IndexParamH264SPSPPS, &sps_pps_data);
			Output(sps_pps_data.pBuffer, sps_pps_data.nLength);
		}
		Output(outputBuffer.pData0, outputBuffer.nSize0);
	}
		
	if (outputBuffer.nSize1 > 0){
		Output(outputBuffer.pData1, outputBuffer.nSize1);
	}
	Output(AUD, 6);
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
}
