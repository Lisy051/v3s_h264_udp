
#ifndef _h264_h_
#define _h264_h_

#include <stdio.h>

#include <veInterface.h>
#include <vencoder.h>

#include "conf.h"
#include "util.h"

//add
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

int h264_init(int width, int height, int fps);

int h264_encode(unsigned char *addrPhyY, unsigned char *addrPhyC, int socket_descriptor, struct sockaddr_in address);
void h264_deinit();

#endif
