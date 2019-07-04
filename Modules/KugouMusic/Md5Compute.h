/*************************************************************************
    > Copyright (C), 2018, GMJK Tech. Co., Ltd.
    > File Name: Md5Compute.h
    > Author: llYao   Version:      Date: Mon 08 Oct 2018 11:19:27 AM CST
    > Description: 用于MD5文件校验的定义 
    > Other: 无 
    > Function List: 
    >  1.compute_file_md5 计算文件MD5校验值
    >  2.compute_string_md5 计算字符串MD5校验值
    > History: // 历史修改记录
    >    <author>  <time>   <version> <desc>
    >    llYao     18/10/08   1.0     build this module
 ************************************************************************/

#ifndef MD5COMPUTE_H
#define MD5COMPUTE_H

#define READ_DATA_SIZE	1024
#define MD5_SIZE		16
#define MD5_STR_LEN		(MD5_SIZE * 2)

int compute_file_md5(const char *file_path, char *md5_str);
int compute_string_md5(unsigned char *dest_str, unsigned int dest_len, char *md5_str);

#endif /* MD5COMPUTE_H */
