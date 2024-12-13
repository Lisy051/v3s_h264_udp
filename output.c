#include "output.h"
#include "config.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

static int udp_out;
static FILE *file_out;
static struct sockaddr_in address;  //接收端的地址

int Output_Init(void)
{
    if (output_contig.enable_file)
    {
        char file_name[255];
        memset(file_name, 0, sizeof(file_name));
        sprintf(file_name, "%sts_index", output_contig.file_dir);  // 视频文件索引
        FILE *file_index_fd = fopen(file_name, "rb+");
        if (file_index_fd < 0)
        {
            fprintf(stderr, "open %s error\n",file_name);
        }
        else
        {
            int file_index = getw(file_index_fd);
            if (file_index < 0)
            {
                file_index = 0;
            }

            memset(file_name, 0 , sizeof(file_name));
            sprintf(file_name, "%svideo_%d.ts", output_contig.file_dir, file_index);
            file_out = fopen(file_name, "wb");      // 视频文件

            file_index++;
            fseek(file_index_fd, 0, SEEK_SET);
            putw(file_index, file_index_fd);
            putw(0, file_index_fd);
            fclose(file_index_fd);
        }
    }

    if (output_contig.enable_udp)
    {
	    bzero(&address,sizeof(address));
	    address.sin_family=AF_INET;
	    address.sin_addr.s_addr=inet_addr(output_contig.udp_addr);	//ip
	    address.sin_port=htons(output_contig.udp_port);

        udp_out = socket(AF_INET,SOCK_DGRAM, 0);//IPV4  SOCK_DGRAM 数据报套接字（UDP协议）
    }

    if (output_contig.enable_pipe ||
        (output_contig.enable_file && file_out > 0) ||
        (output_contig.enable_udp && udp_out > 0))
    {
        return 0;
    }
    return -1;    
}

int Output(char *buf, int len)
{
    if (udp_out > 0)
    {
        int data_len = len;
        char *pdata = buf;
        while (data_len > output_contig.pack_len)
        {
            sendto(udp_out, pdata, output_contig.pack_len, 0, (struct sockaddr *)&address, sizeof(address));
            data_len -= output_contig.pack_len;
            pdata += output_contig.pack_len;
        }
        if (data_len > 0)
        {
            sendto(udp_out, pdata, data_len, 0, (struct sockaddr *)&address, sizeof(address));
        }
    }

    if (output_contig.enable_pipe)
    {
        write(STDOUT_FILENO, buf, len);
        fflush(stdout);
    }

    if (file_out > 0)
    {
        fwrite(buf, 1, len, file_out);
        fflush(file_out);
    }
    return len;
}

void Output_DeInit(void)
{
    if (file_out > 0)
    {
        fclose(file_out);
    }
    if (udp_out > 0)
    {
        close(udp_out);
    }
}
