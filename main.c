
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
#include "config.h"
#include "output.h"

/*
	More detail on the v4l2 / media-ctl API:
	https://docs.kernel.org/userspace-api/media/v4l/v4l2.html
	https://www.kernel.org/doc/html/latest/userspace-api/media/mediactl/media-controller.html

	Getting subdevice name from major & minor numbers:
	https://git.linuxtv.org/v4l-utils.git/tree/utils/media-ctl/libmediactl.c
	in function 'media_get_devname_sysfs()'
*/

void stop()
{
	cam_stop_capture();
	cam_deinit();
	cam_close();
	h264_deinit();
	Output_DeInit();
}

int main()
{
	Config_Init();

	if (Output_Init() < 0 ||
		h264_init() < 0 ||
		cam_open() < 0 ||
		cam_init() < 0 ||
		cam_start_capture() < 0)
	{
		stop();
	}

	// Capture frames
	buffer_t *buf;
	while (1)
	{
		// Dequeue buffer and get its index
		buf = cam_get_buf(cam_dqbuf());
		if (buf == NULL)
			break;
		// Encode frame
		h264_encode(buf->addrPhyY, buf->addrPhyC);
		// Queue the recently dequeued buffer back to the device
		cam_qbuf();
	}

	stop();

	return 0;
}
