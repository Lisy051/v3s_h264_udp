
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <linux/media.h>

#include "h264.h"
#include "cam.h"
#include "util.h"
#include "conf.h"

/*
	More detail on the v4l2 / media-ctl API:
	https://docs.kernel.org/userspace-api/media/v4l/v4l2.html
	https://www.kernel.org/doc/html/latest/userspace-api/media/mediactl/media-controller.html

	Getting subdevice name from major & minor numbers:
	https://git.linuxtv.org/v4l-utils.git/tree/utils/media-ctl/libmediactl.c
	in function 'media_get_devname_sysfs()'
*/

void usage(char *argv0) {
	dlog(DLOG_WARN
		"Usage: %s [width] [height] [FPS] [ip] [port]\n"
		"Supported formats: 640x480, 1280x720, 1920x1080\n"
		"All formats support 30FPS; 640x480 also supports 60FPS.\n"
		, argv0);
}

int main(int argc, char **argv) {
#ifdef ENABLE_DEBUG
	dlog_set_level(DLOG_DEBUG);
#endif
	int ret = 0;

	if (argc != 6) {
		usage(argv[0]);
		return 0;
	}

	int width = atoi(argv[1]);
	int height = atoi(argv[2]);
	int fps = atoi(argv[3]);

	int socket_descriptor; //套接口描述字
	struct sockaddr_in address;//处理网络通信的地址
	bzero(&address,sizeof(address));
	address.sin_family=AF_INET;
	address.sin_addr.s_addr=inet_addr(argv[4]);	//ip
	address.sin_port=htons(atoi(argv[5]));
	dlog("\nInfo: inet_ntoa ip = %s port = %d\n",
		inet_ntoa(address.sin_addr),atoi(argv[5]));
	//创建一个 UDP socket
	socket_descriptor=socket(AF_INET,SOCK_DGRAM,0);//IPV4  SOCK_DGRAM 数据报套接字（UDP协议）
	if(socket_descriptor == -1){
		dlog(DLOG_CRIT "Error: socket\n");
		return 0;
	}

	if ((width == 640 && height == 480) ||
		(width == 1280 && height == 720) ||
		(width == 1920 && height == 1080)) {

		dlog_cleanup(h264_init(width, height, fps), DLOG_CRIT "Error: h264_init() failed\n");
		dlog_cleanup(cam_open(), DLOG_CRIT "Error: cam_open() failed\n");
		dlog_cleanup(cam_init(width, height, G_V4L2_PIX_FMT, fps),
					DLOG_CRIT "Error: cam_init() failed\n");
	}
	else {
		dlog(DLOG_CRIT "Error: unsupported width/height\n");
		usage(argv[0]);
		return 0;
	}

	dlog_cleanup(cam_start_capture(), DLOG_CRIT "Error: cam_start_capture() failed\n");

	// Capture frames
	for(;;){
		rt_timer_start();
		
		// Dequeue buffer and get its index
		int j = cam_dqbuf();
		dlog_cleanup(j, DLOG_CRIT "Error: cam_dqbuf() failed");
		buffer_t *buf = cam_get_buf(j);

		// Encode frame
		dlog_cleanup(h264_encode(buf->addrPhyY, buf->addrPhyC, socket_descriptor, address), DLOG_CRIT "Error: h264_encode() failed\n");
		
		// Queue the recently dequeued buffer back to the device
		dlog_cleanup(cam_qbuf(), DLOG_CRIT "Error: cam_qbuf() failed\n");
		rt_timer_stop();
		double elapsed = rt_timer_elapsed();
		dlog("\nInfo: captured %d frames in %.2fs; FPS = %.1f\n",
			j, elapsed, 1 / elapsed);
	}

cleanup:
	cam_stop_capture();
	cam_deinit();
	cam_close();
	h264_deinit();
	return ret;
}

