#include "cJSON.h"
#include "config.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <vencoder.h>
#include <veInterface.h>
#include <stdlib.h>

#define CONFIG_JSON     "/etc/camera_config.json"

camera_config_t camera_config;
encode_config_t encode_config;
output_contig_t output_contig;

/**
 * @brief 重置配置文件
 * 
 */
void Config_Reset(void)
{
    cJSON *root = cJSON_CreateObject();

    // 重置摄像头参数
    cJSON *sub_camera = cJSON_CreateObject();
    cJSON_AddItemToObject(sub_camera, "Sensor", cJSON_CreateString("ov5640 0-003c"));
    cJSON_AddItemToObject(sub_camera, "Width", cJSON_CreateNumber(640));
    cJSON_AddItemToObject(sub_camera, "Height", cJSON_CreateNumber(480));
    cJSON_AddItemToObject(sub_camera, "FPS", cJSON_CreateNumber(30));
    cJSON_AddItemToObject(sub_camera, "Rotation", cJSON_CreateNumber(0));

    // 重置h264编码器参数
    cJSON *sub_encode = cJSON_CreateObject();
    cJSON_AddItemToObject(sub_encode, "Bitrate", cJSON_CreateNumber(1024*1024));
    cJSON_AddItemToObject(sub_encode, "Maxqp", cJSON_CreateNumber(40));
    cJSON_AddItemToObject(sub_encode, "Minqp", cJSON_CreateNumber(20));
    cJSON_AddItemToObject(sub_encode, "MaxKeyInterval", cJSON_CreateNumber(30));
    cJSON_AddItemToObject(sub_encode, "BlockNumber", cJSON_CreateNumber(3));
    cJSON_AddItemToObject(sub_encode, "CodingMode", cJSON_CreateNumber(0));
    cJSON_AddItemToObject(sub_encode, "EntropyCodingCABAC", cJSON_CreateNumber(30));
    cJSON_AddItemToObject(sub_encode, "Profile", cJSON_CreateNumber(66));
    cJSON_AddItemToObject(sub_encode, "Level", cJSON_CreateNumber(32));

    // 重置输出配置
    cJSON *sub_output = cJSON_CreateObject();
    cJSON_AddItemToObject(sub_output, "enable_udp", cJSON_CreateBool(0));
    cJSON_AddItemToObject(sub_output, "enable_file", cJSON_CreateBool(1));
    cJSON_AddItemToObject(sub_output, "enable_pipe", cJSON_CreateBool(0));
    cJSON_AddItemToObject(sub_output, "udp_port", cJSON_CreateNumber(5600));
    cJSON_AddItemToObject(sub_output, "udp_addr", cJSON_CreateString("127.0.0.1"));
    cJSON_AddItemToObject(sub_output, "file_dir", cJSON_CreateString("/media/"));

    cJSON_AddItemToObject(root, "camera", sub_camera);
    cJSON_AddItemToObject(root, "encode", sub_encode);
    cJSON_AddItemToObject(root, "output", sub_output);

    FILE *fp = fopen(CONFIG_JSON, "w");
    char *buf = cJSON_Print(root);
    fwrite(buf, 1, strlen(buf) + 1, fp);
    fclose(fp);
    fprintf(stderr, "%s reset\n",CONFIG_JSON);
    exit(1);
}

void Get_Camera_Config(cJSON *root);
void Get_Encode_Config(cJSON *root);
void Get_Output_Config(cJSON *root);

/**
 * @brief 初始化配置参数
 * 
 */
void Config_Init(void)
{
    memset(&camera_config, 0, sizeof(camera_config));
    memset(&encode_config, 0, sizeof(encode_config));
    memset(&output_contig, 0, sizeof(output_contig));
    if (access(CONFIG_JSON, F_OK) == -1)
    {
        fprintf(stderr, "%s not find\n",CONFIG_JSON);
        Config_Reset();
    }
    FILE *fp = fopen(CONFIG_JSON, "r");
    char buf[4096] = {0};
    fread(buf, 1, sizeof(buf), fp);
    fclose(fp);

    cJSON *root = cJSON_Parse(buf);
    Get_Camera_Config(root);
    Get_Encode_Config(root);
    Get_Output_Config(root);
    cJSON_Delete(root);
}

/**
 * @brief 获取摄像头配置参数
 * 
 * @param root json文件root结点
 */
void Get_Camera_Config(cJSON *root)
{
    cJSON *sub_camera = cJSON_GetObjectItem(root, "camera");
    if (sub_camera <= 0)
    {
        fprintf(stderr, "%s error\n",CONFIG_JSON);
        Config_Reset();
    }
    cJSON *item = cJSON_GetObjectItem(sub_camera, "Sensor");
    if (item)
    {
        if (strlen(item->valuestring) > sizeof(camera_config.sensor))
        {
            fprintf(stderr, "Sensor name is too long\n");
            Config_Reset();
        }
        strcpy(camera_config.sensor, item->valuestring);
    }
    else
    {
        fprintf(stderr, "Sersor not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_camera, "Width");
    if (item)
    {
        camera_config.width = item->valueint;
    }
    else
    {
        fprintf(stderr, "Width not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_camera, "Height");
    if (item)
    {
        camera_config.height = item->valueint;
    }
    else
    {
        fprintf(stderr, "Height not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_camera, "FPS");
    if (item)
    {
        camera_config.fps = item->valueint;
    }
    else
    {
        fprintf(stderr, "FPS not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_camera, "Rotation");
    if (item)
    {
        camera_config.rotation = item->valueint;
    }
    else
    {
        fprintf(stderr, "Rotation not find\n");
        Config_Reset();
    }
}

/**
 * @brief 获取编码器配置参数
 * 
 * @param root json文件root结点
 */
void Get_Encode_Config(cJSON *root)
{
    cJSON *sub_encode = cJSON_GetObjectItem(root, "encode");
    if (sub_encode <= 0)
    {
        fprintf(stderr, "%s error\n",CONFIG_JSON);
        Config_Reset();
    }
    cJSON *item = cJSON_GetObjectItem(sub_encode, "Bitrate");
    if (item)
    {
        encode_config.h264_Bitrate = item->valueint;
    }
    else
    {
        fprintf(stderr, "Bitrate not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_encode, "Maxqp");
    if (item)
    {
        encode_config.h264_Maxqp = item->valueint;
    }
    else
    {
        fprintf(stderr, "Maxqp not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_encode, "Minqp");
    if (item)
    {
        encode_config.h264_Maxqp = item->valueint;
    }
    else
    {
        fprintf(stderr, "Minqp not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_encode, "MaxKeyInterval");
    if (item)
    {
        encode_config.h264_MaxKeyInterval = item->valueint;
    }
    else
    {
        fprintf(stderr, "MaxKeyInterval not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_encode, "BlockNumber");
    if (item)
    {
        encode_config.h264_BlockNumber = item->valueint;
    }
    else
    {
        fprintf(stderr, "BlockNumber not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_encode, "CodingMode");
    if (item)
    {
        encode_config.h264_CodingMode = item->valueint;
    }
    else
    {
        fprintf(stderr, "CodingMode not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_encode, "EntropyCodingCABAC");
    if (item)
    {
        encode_config.h264_EntropyCodingCABAC = item->valueint;
    }
    else
    {
        fprintf(stderr, "EntropyCodingCABAC not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_encode, "Profile");
    if (item)
    {
        encode_config.h264_Profile = item->valueint;
    }
    else
    {
        fprintf(stderr, "Profile not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_encode, "Level");
    if (item)
    {
        encode_config.h264_Level = item->valueint;
    }
    else
    {
        fprintf(stderr, "Level not find\n");
        Config_Reset();
    }
}

/**
 * @brief 获取输出配置参数
 * 
 * @param root json文件root结点
 */
void Get_Output_Config(cJSON *root)
{
    cJSON *sub_output = cJSON_GetObjectItem(root, "output");
    if (sub_output <= 0)
    {
        fprintf(stderr, "%s error\n",CONFIG_JSON);
        Config_Reset();
    }
    cJSON *item = cJSON_GetObjectItem(sub_output, "enable_udp");
    if (item)
    {
        output_contig.enable_udp = (item->type == cJSON_True);
    }
    else
    {
        fprintf(stderr, "enable_udp not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_output, "enable_file");
    if (item)
    {
        output_contig.enable_file = (item->type == cJSON_True);
    }
    else
    {
        fprintf(stderr, "enable_file not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_output, "enable_pipe");
    if (item)
    {
        output_contig.enable_pipe = (item->type == cJSON_True);
    }
    else
    {
        fprintf(stderr, "enable_pipe not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_output, "udp_port");
    if (item)
    {
        output_contig.udp_port = item->valueint;
    }
    else
    {
        fprintf(stderr, "udp_port not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_output, "udp_addr");
    if (item)
    {
        if (strlen(item->valuestring) > sizeof(output_contig.udp_addr))
        {
            fprintf(stderr, "udp_addr is too long\n");
            Config_Reset();
        }
        strcpy(output_contig.udp_addr, item->valuestring);
    }
    else
    {
        fprintf(stderr, "udp_addr not find\n");
        Config_Reset();
    }
    item = cJSON_GetObjectItem(sub_output, "file_dir");
    if (item)
    {
        if (strlen(item->valuestring) > sizeof(output_contig.file_dir))
        {
            fprintf(stderr, "file_dir is too long\n");
            Config_Reset();
        }
        strcpy(output_contig.file_dir, item->valuestring);
    }
    else
    {
        fprintf(stderr, "file_dir not find\n");
        Config_Reset();
    }
}
