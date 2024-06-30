
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <linux/videodev2.h>
#include <linux/v4l2-subdev.h>
#include <linux/media.h>

#include "cam.h"
#include "config.h"

static int fd = -1;
static int g_buf_count = 3;
static int buf_idx = 0;
static buffer_t *buffers = NULL;

// List of V4L2 controls that will be set upon device init
#define NUM_CTRLS 2
struct v4l2_control ctrls[NUM_CTRLS] = {
	{V4L2_CID_HFLIP, 1},
	{V4L2_CID_VFLIP, 1}
};

int cam_open(void) {
	fd = open("/dev/video0", O_RDWR, 0);
	return fd;
}

// Initialize media bus settings
// This is needed for DVP cameras that uses the V4L2 subdev API
// Probably won't work for USB webcams and such
// For more info, see
// https://www.kernel.org/doc/html/latest/userspace-api/media/mediactl/media-controller.html
//
static int cam_media_init(void) {
	int ret = 0;
	struct media_v2_entity *mve = NULL;
	struct media_v2_pad *mvp = NULL;

	// Open media / subdev file descriptors
	int mfd = open("/dev/media0", O_RDWR, 0);
	int sfd = open("/dev/v4l-subdev0", O_RDWR);

	// Query media API topology
	struct media_v2_topology mvt;
	memset(&mvt, 0, sizeof(mvt));
	ioctl(mfd, MEDIA_IOC_G_TOPOLOGY, &mvt);

	fprintf(stderr, "Debug: %d media entities detected\n", mvt.num_entities);

	mve = calloc(mvt.num_entities, sizeof(*mve));
	mvp = calloc(mvt.num_pads, sizeof(*mvp));
	if (!mve || !mvp) {
		fprintf(stderr, "Error: mve/mvp calloc() failed\n");
		ret = -1;
		goto cleanup;
	}
	mvt.ptr_entities = (unsigned long) mve;
	mvt.ptr_pads = (unsigned long) mvp;
	ioctl(mfd, MEDIA_IOC_G_TOPOLOGY, &mvt);

	// Find entity id and subdev pad of device
	int entity_id = -1, subdev_pad = -1;

	for (int i=0; i<mvt.num_entities; i++) {
		if (strcmp(camera_config.sensor, mve[i].name) == 0) {
			entity_id = mve[i].id;
			fprintf(stderr, "Debug: %s: subdev entity id = %d\n", camera_config.sensor,
				 entity_id);
		}
	}

	if (entity_id == -1) {
		fprintf(stderr, "Error: media entity %s not found\n", camera_config.sensor);
		ret = -1;
		goto cleanup;
	}

	for (int i=0; i<mvt.num_pads; i++) {
		if (mvp[i].entity_id == entity_id) {
			subdev_pad = mvp[i].index;
			fprintf(stderr, "Debug: %s: subdev pad = %d\n", camera_config.sensor, subdev_pad);
		}
	}

	if (subdev_pad == -1) {
		fprintf(stderr, "Error: no subdev pad found for %s\n", camera_config.sensor);
		ret = -1;
		goto cleanup;
	}

	// Reset frame rate to 30FPS (default for most media bus formats)
	struct v4l2_fract fract = { .numerator = 1, .denominator = 30 };
	struct v4l2_subdev_frame_interval ival = {
		.interval = fract,
		.pad = subdev_pad,
	};
	ioctl(sfd, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &ival);

	// Set subdev media bus format
	struct v4l2_subdev_format sfmt = {
		.pad = subdev_pad,
		.which = V4L2_SUBDEV_FORMAT_ACTIVE,
	};

	ioctl(sfd, VIDIOC_SUBDEV_G_FMT, &sfmt);

	sfmt.format.width = camera_config.width;
	sfmt.format.height = camera_config.height;
	sfmt.format.code = MEDIA_BUS_FMT_UYVY8_2X8;
	sfmt.format.field = V4L2_FIELD_NONE;

	ioctl(sfd, VIDIOC_SUBDEV_S_FMT, &sfmt);

	fprintf(stderr, "Info: %s: subdev format set to: %dx%d, media bus format code = 0x%x\n",
		camera_config.sensor, sfmt.format.width, sfmt.format.height, sfmt.format.code);

	// Set frame rate
	fract.denominator = camera_config.fps;
	ival.interval = fract;
	ioctl(sfd, VIDIOC_SUBDEV_S_FRAME_INTERVAL, &ival);
	
	fprintf(stderr, "Info: %s: frame rate: requested for %d; image sensor accepted %d\n",
		camera_config.sensor, camera_config.fps, ival.interval.denominator);

cleanup:
	if (sfd >= 0) {
		close(sfd);
		sfd = -1;
	}
	if (mve)
		free(mve);
	if (mvp)
		free(mvp);
	if (mfd >= 0) {
		close(mfd);
		mfd = -1;
	}
	return ret;
}

int cam_init() {
	if (cam_media_init() < 0) {
		fprintf(stderr, "Error: cam_media_init() failed\n");
		return -1;
	}

	// Query device capabilities
	struct v4l2_capability cap;
	memset(&cap, 0, sizeof(cap));
	ioctl(fd, VIDIOC_QUERYCAP, &cap);

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
		!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "Error: V4L2_CAP_VIDEO_CAPTURE or V4L2_CAP_STREAMING not supported\n");
		return -1;
	}
	
	// Set V4L2 format
	struct v4l2_format fmt = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.fmt.pix.width = camera_config.width,
		.fmt.pix.height = camera_config.height,
		.fmt.pix.pixelformat = V4L2_PIX_FMT_NV12,
	};
	ioctl(fd, VIDIOC_S_FMT, &fmt);
	
	// Set V4L2 controls specified in ctrls[]
	for (int i=0; i<NUM_CTRLS; i++)
		ioctl(fd, VIDIOC_S_CTRL, &ctrls[i]);

	// Request buffers from device (for storing frames later)
	// using the V4L2_MEMORY_MMAP mechanism
	struct v4l2_requestbuffers req = {
		.count = g_buf_count,
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.memory = V4L2_MEMORY_MMAP,
	};
	ioctl(fd, VIDIOC_REQBUFS, &req);

	g_buf_count = req.count;	// Update the actual number of buffers expected by device

	buffers = calloc(g_buf_count, sizeof(*buffers));

	if (buffers == NULL) {
		fprintf(stderr, "Error: buffer calloc() failed\n");
		return -1;
	}

	// MMAP buffers from device
	for (int i=0; i<g_buf_count; i++) {
		struct v4l2_buffer buf = {
			.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
			.memory = V4L2_MEMORY_MMAP,
			.index = i,
		};
		ioctl(fd, VIDIOC_QUERYBUF, &buf);

		buffers[i].start = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
								MAP_SHARED, fd, buf.m.offset);
		buffers[i].length = buf.length;
		buffers[i].addrVirY = buffers[i].start;
	
		// This ALIGN_16B thing is wrong?
		// At 1920x1080, this will screw up the data alignment and create a green band in video

		//buffers[i].addrVirC = buffers[i].start + ALIGN_16B(g_width) * ALIGN_16B(g_height);
		buffers[i].addrVirC = buffers[i].start + camera_config.width * camera_config.height;
		
		int addr = buf.m.offset;

		// dirty hack to get physical address of buffers
		// see github repo README for details
		ioctl(fd, CAM_V2P_IOCTL, &addr);

		buffers[i].addrPhyY = (void *) addr;
		//buffers[i].addrPhyC = addr + ALIGN_16B(g_width) * ALIGN_16B(g_height);
		buffers[i].addrPhyC = (void *) (addr + camera_config.width * camera_config.height);
	}
	return 0;
}

int cam_start_capture() {
	for (int i=0; i<g_buf_count; i++) {
		struct v4l2_buffer buf = {
			.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
			.memory = V4L2_MEMORY_MMAP,
			.index = i,
		};
		ioctl(fd, VIDIOC_QBUF, &buf);
	}
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(fd, VIDIOC_STREAMON, &type);
	return 0;
}

// Returns non-negative dequeued buffer index upon success
int cam_dqbuf() {
	struct v4l2_buffer buf = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.memory = V4L2_MEMORY_MMAP,
		.index = buf_idx,
	};
	ioctl(fd, VIDIOC_DQBUF, &buf);
	return buf_idx;
}

buffer_t *cam_get_buf(int idx) {
	if (0 <= idx && idx < g_buf_count)
		return buffers + idx;
	return NULL;
}

// Enqueue the last dequeued buffer back to the camera device
int cam_qbuf() {
	struct v4l2_buffer buf = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.memory = V4L2_MEMORY_MMAP,
		.index = buf_idx,
	};
	ioctl(fd, VIDIOC_QBUF, &buf);
	buf_idx = (buf_idx + 1) % g_buf_count;
	return 0;
}

int cam_stop_capture() {
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ioctl(fd, VIDIOC_STREAMOFF, &type);
	return 0;
}

void cam_deinit() {
	if (buffers) {
		for (int i=0; i<g_buf_count; i++)
			munmap(buffers[i].start, buffers[i].length);
		free(buffers);
		buffers = NULL;
	}
}

void cam_close() {
	if (fd != -1) {
		close(fd);
		fd = -1;
	}
}

