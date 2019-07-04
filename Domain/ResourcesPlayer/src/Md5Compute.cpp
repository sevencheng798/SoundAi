/*************************************************************************
    > Copyright (C), 2018, GMJK Tech. Co., Ltd.
    > File Name: md5_main.c
    > Author: llYao   Version:      Date: Mon 08 Oct 2018 11:10:28 AM CST
    > Description: 文件信息MD5校验值生成
    > Version: V1.0.1
    > Function List: 
    >  1.md5_main 根据传入file_path文件名称, 获取MD5校验值MD5_str
	>  2.Compute_file_md5 
    > History: // 历史修改记录
    >    <author>  <time>   <version> <desc>
    >    llYao     18/09/26   1.0     build this module
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ResourcesPlayer/md5.h"
#include "ResourcesPlayer/Md5Compute.h"

/*************************************************************************
 *   Function:       compute_file_md5
 *   Description:    计算文件MD5校验值
 *   Calls:          MD5Init、MD5Update、MD5Final
 *   Called By:      res_json_result.c
 *   Table Accessed: 无
 *   Table Updated:  无
 *   Input:          file_path 文件路径名称 
 *   Output:         md5_str 文件的MD5校验值 
 *   Return:         0 成功, -1 失败 
 *   Others:         无
 ************************************************************************/
int compute_file_md5(const char *file_path, char *md5_str)
{
	int i;
	int fd;
	int ret;
	unsigned char data[READ_DATA_SIZE];
	unsigned char md5_value[MD5_SIZE];
	MD5_CTX md5;

	fd = open(file_path, O_RDONLY);
	if (-1 == fd)
	{
		perror("open");
		return -1;
	}

	// init md5
	MD5Init(&md5);

	while (1)
	{
		ret = read(fd, data, READ_DATA_SIZE);
		if (-1 == ret)
		{
			perror("read");
			close(fd);
			return -1;
		}

		MD5Update(&md5, data, ret);

		if (0 == ret || ret < READ_DATA_SIZE)
		{
			break;
		}
	}

	close(fd);

	MD5Final(&md5, md5_value);

	// convert md5 value to md5 string
	for(i = 0; i < MD5_SIZE; i++)
	{
		snprintf(md5_str + i*2, 2+1, "%02x", md5_value[i]);
	}

	return 0;
}

/*************************************************************************
 *   Function:       compute_string_md5
 *   Description:    计算字符串MD5校验值
 *   Calls:          MD5Init、MD5Update、MD5Final
 *   Called By:      无
 *   Table Accessed: 无
 *   Table Updated:  无
 *   Input:          dest_str 字符串 dest_len 字符串长度 
 *   Output:         md5_str 字符串MD5校验值 
 *   Return:         0
 *   Others:         无
 ************************************************************************/
int compute_string_md5(unsigned char *dest_str, unsigned int dest_len, char *md5_str)
{
	int i;
	unsigned char md5_value[MD5_SIZE];
	MD5_CTX md5;

	// init md5
	MD5Init(&md5);

	MD5Update(&md5, dest_str, dest_len);

	MD5Final(&md5, md5_value);

	// convert md5 value to md5 string
	for(i = 0; i < MD5_SIZE; i++)
	{
		snprintf(md5_str + i*2, 2+1, "%02x", md5_value[i]);
	}

	return 0;
}
