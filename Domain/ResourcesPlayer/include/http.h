/*************************************************************************
    > Copyright (C), 2018, GMJK Tech. Co., Ltd.
    > File Name: http.h
    > Author: llYao   Version:      Date: Thu 04 Jul 2019 02:30:34 PM CST
    > Description: // 模块描述 用于详细说明此程序文件完成的主要功能，与其他模块或函数的接口
      	 输出值、取值范围、含义及参数间的控制、顺序、独立或依赖等关系其它内容的说明
    > Other: // 其它内容说明
    > Function List: // 主要函数列表，每条记录应包括函数名及功能简要说明
    >  1.------
    > History: // 历史修改记录
    >    <author>  <time>   <version> <desc>
    >    llYao     19/07/04   1.0     build this module
 ************************************************************************/

#ifndef _MY_HTTP_H
#define _MY_HTTP_H
 
#define BUFFER_SIZE 1024
#define MY_HTTP_DEFAULT_PORT 80
#define HTTP_POST "POST /%s HTTP/1.1\r\nHOST: %s:%d\r\nAccept: */*\r\n"\
    "Content-Type:application/json\r\nContent-Length: %d\r\n\r\n%s"
#define TRACK_LINK_URI "http://content.xfyun.cn/music/tracklink?&timestamp=%llu&deviceId=%s&token=%s&appId=%s&kugouUserId=%s&kugouUserToken=%s&clientId=%s&clientDeviceId=%s&itemid=%s"

#ifdef __cplusplus
extern "C"{
#endif // __cplusplus
 
int getMusicUrl(char *aiuiUid, char *appId, char *kugouUserId, char *kugouUserToken, char *clientDeviceId, char *itemId, char *albumId, char *kugouMusicUrl);
#ifdef __cplusplus
}
#endif

 
#endif
