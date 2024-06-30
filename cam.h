
#ifndef _cam_h_
#define _cam_h_

#include <stdlib.h>
#include <stdint.h>

// custom ioctl to translate virtual address to physical addresses
#define CAM_V2P_IOCTL 12345677

typedef struct {
	void *start;
	void *addrVirY;
	void *addrVirC;
	void *addrPhyY;
	void *addrPhyC;
	size_t length;
} buffer_t;

/*
	All functions return 0 upon success
	and negative upon failure, unless
	explicitly stated otherwise.
*/

int cam_open(void);

int cam_init(void);

int cam_start_capture(void);

// Returns non-negative dequeued buffer index upon success
int cam_dqbuf(void);

// Returns pointer to buffer upon success, NULL otherwise
buffer_t *cam_get_buf(int idx);

int cam_qbuf(void);

int cam_stop_capture(void);

void cam_deinit(void);

void cam_close(void);

#endif
