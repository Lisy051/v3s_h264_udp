
#ifndef _h264_h_
#define _h264_h_

#include <stdio.h>

#include <veInterface.h>
#include <vencoder.h>

#include "config.h"

int h264_init(void);

int h264_encode(unsigned char *addrPhyY, unsigned char *addrPhyC);
void h264_deinit(void);

#endif
