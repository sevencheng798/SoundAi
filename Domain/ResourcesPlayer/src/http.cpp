/*************************************************************************
    > Copyright (C), 2018, GMJK Tech. Co., Ltd.
    > File Name: http.c
    > Author: llYao   Version:      Date: Thu 04 Jul 2019 02:31:24 PM CST
    > Description: // 模块描述
    > Version: // 版本信息
    > Function List: // 主要函数及其功能
    >  1.------
    > History: // 历史修改记录
    >    <author>  <time>   <version> <desc>
    >    llYao     19/07/04   1.0     build this module
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>
#include <sys/time.h>
#include <unistd.h>

#include "ResourcesPlayer/http.h"
#include "ResourcesPlayer/cJSON.h"
#include "ResourcesPlayer/Md5Compute.h"

#include <Utils/Logging/Logger.h>


/// String to identify log entries originating from this file.
static const std::string TAG{"HTTP"};
//
#define LX(event) aisdk::utils::logging::LogEntry(TAG, event)
 
static int httpTcpClientCreate(const char *host, int port)
{
    struct hostent *he;
    struct sockaddr_in server_addr; 
    int socketFd;
 
    if((he = gethostbyname(host)) == NULL)
	{
        return -1;
    }
 
   server_addr.sin_family = AF_INET;
   server_addr.sin_port = htons(port);
   server_addr.sin_addr = *((struct in_addr *)he->h_addr);
 
    if((socketFd = socket(AF_INET,SOCK_STREAM, 0)) == -1)
	{
        return -1;
    }
 
    if(connect(socketFd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
	{
        return -1;
    }
 
    return socketFd;
}
 
static void httpTcpClientClose(int socket)
{
    close(socket);
}
 
static int httpParseUrl(const char *url, char *host, char *file, int *port)
{
    char *ptr1,*ptr2;
    int len = 0;
    if(!url || !host || !file || !port){
        return -1;
    }
 
    ptr1 = (char *)url;
 
    if(!strncmp(ptr1,"http://",strlen("http://")))
	{
        ptr1 += strlen("http://");
    }
	else
	{
        return -1;
    }
 
    ptr2 = strchr(ptr1,'/');
    if(ptr2)
	{
        len = strlen(ptr1) - strlen(ptr2);
        memcpy(host,ptr1,len);
        host[len] = '\0';
        if(*(ptr2 + 1))
		{
            memcpy(file,ptr2 + 1,strlen(ptr2) - 1 );
            file[strlen(ptr2) - 1] = '\0';
        }
    }
	else
	{
        memcpy(host,ptr1,strlen(ptr1));
        host[strlen(ptr1)] = '\0';
    }
	//printf("func = %s, file = %s\n", __func__, file);
    //get host and ip
    ptr1 = strchr(host,':');
    if(ptr1)
	{
        *ptr1++ = '\0';
        *port = atoi(ptr1);
		//printf("func = %s, port = %d\n", __func__, atoi(ptr1));
    }
	else
	{
        *port = MY_HTTP_DEFAULT_PORT;
    }
 
    return 0;
}
 
static int httpTcpClientRecv(int socket, char *lpbuff)
{
    int recvnum = 0;
	int num = 0;
	char buf[BUFFER_SIZE +1] = {'\0'};
	char temp[BUFFER_SIZE+1] = {'\0'};
    char content[40]="";
	int length = 0;
 
	while(1)
	{
		if(recvnum >= length && recvnum != 0 && length != 0)
		{
			break;
		}
		memset(buf, 0, sizeof(buf));
    	num = recv(socket, buf, BUFFER_SIZE, 0);
		memcpy(temp, buf, strlen(buf)+1);
		if(length == 0)
		{
			char *p = NULL;
			char *p1 = NULL;
			p = strstr(temp, "Content-Length: ");
			if(p != NULL)
			{
				p1 = strtok(p, "\n");
				sscanf(p1, "%s %d", content, &length);
			}
		}
		//printf("func = %s[%d] \n__________buf = %s______________\n", __func__,__LINE__, buf);
        AISDK_INFO(LX("httpTcpClientRecv").d("reason: ", buf));
		strcat(lpbuff, buf);
		//printf("func = %s[%d] num = %d\n, content-length = %d\n", __func__,__LINE__, num, length);
		recvnum += num;
	}
 
    return recvnum;
}
 
static int httpTcpClientSend(int socket, char *buff, int size)
{
    int sent=0,tmpres=0;
 
    while(sent < size)
	{
        tmpres = send(socket, buff+sent, size-sent, 0);
        if(tmpres == -1)
		{
            return -1;
        }
        sent += tmpres;
    }
    return sent;
}
 
static char *httpParseResult(const char *lpbuf)
{
    char *ptmp = NULL; 
    char *response = NULL;
    ptmp = (char*)strstr(lpbuf,"HTTP/1.1");
    if(!ptmp)
	{
        AISDK_ERROR(LX("httpParseResult").d("reason: ", "http/1.1 not find"));
        return NULL;
    }
    if(atoi(ptmp + 9)!=200)
	{
        AISDK_INFO(LX("httpParseResult").d("lpbuf: ", lpbuf));
        return NULL;
    }
 
    ptmp = (char*)strstr(lpbuf,"\r\n\r\n");
    if(!ptmp)
	{
        AISDK_ERROR(LX("httpParseResult").d("reason: ", "ptmp is NULL"));
        return NULL;
    }
    response = (char *)malloc(strlen(ptmp)+1);
    if(!response)
	{
        AISDK_ERROR(LX("httpParseResult").d("reason: ", "malloc failed"));
        return NULL;
    }
    strcpy(response, ptmp+4);
    AISDK_INFO(LX("httpParseResult").d("response: ", response));
    return response;
}
 
static char *httpPost(const char *url, const char *postStr)
{
    int port = 0;
    int socketFd = -1;
    char file[BUFFER_SIZE] = {'\0'};
    char hostAddr[BUFFER_SIZE] = {'\0'};
    char sendBuf[BUFFER_SIZE*10] = {'\0'};
    char recvBuf[BUFFER_SIZE*10] = {'\0'};

	memset(file, 0x00, sizeof(file));
	memset(hostAddr, 0x00, sizeof(hostAddr));
	memset(sendBuf, 0x00, sizeof(sendBuf));
	memset(recvBuf, 0x00, sizeof(recvBuf));
 
    if(!url || !postStr)
	{
        AISDK_ERROR(LX("httpPost").d("reason: ", "http post failed"));
        return NULL;
    }
 
    if(httpParseUrl(url, hostAddr, file, &port))
	{
        AISDK_ERROR(LX("httpPost").d("reason: ", "httpParseUrl failed"));
        return NULL;
    }
    //printf("hostAddr : %s\tfile:%s\t,%d\n",hostAddr,file,port);
 
    socketFd = httpTcpClientCreate(hostAddr, port);
    if(socketFd < 0)
	{
        AISDK_ERROR(LX("httpPost").d("reason: ", "httpTcpClientCreate failed"));
        return NULL;
    }
     
    sprintf(sendBuf, HTTP_POST, file, hostAddr, port, (int)strlen(postStr), postStr);
 
    if(httpTcpClientSend(socketFd, sendBuf, strlen(sendBuf)) < 0)
	{
        AISDK_ERROR(LX("httpPost").d("reason: ", "httpTcpClientSend failed"));
        return NULL;
    }
    AISDK_INFO(LX("httpPost").d("sendBuf: ", sendBuf));
 
    /*it's time to recv from server*/
    if(httpTcpClientRecv(socketFd, recvBuf) <= 0)
	{
        AISDK_ERROR(LX("httpPost").d("reason: ", "httpTcpClientRecv failed"));
        return NULL;
    }
    AISDK_INFO(LX("httpPost").d("recvBuf: ", recvBuf));
 
    httpTcpClientClose(socketFd);
 
    return httpParseResult(recvBuf);
}

/*************************************************************************
 *   Function:       getMusicUrl
 *   Description:    获取酷狗音乐url链接
 *   Input:Input 	 aiuiUid、appId、kugouUserId、kugouUserToken
 *                   clientDeviceId、itemId、albumId 
 *   Output:         kugouMusicUrl 音乐播放链接 
 *   Return:         0成功、-1获取失败、-2字符串JSON 解析失败
 *   Others:         无
 ************************************************************************/
int getMusicUrl(const char *aiuiUid,const char *appId, const char *appKey, const char *kugouUserId,const char *kugouUserToken,const char *clientDeviceId,const char *itemId,const char *albumId, char *kugouMusicUrl)
{
	char *p = NULL;
	char token[64] = {'\0'};
	char md5Token[64] = {'\0'};
	char postSendBuf[BUFFER_SIZE] = {'\0'};

	memset(token, 0x00, sizeof(token));
	memset(md5Token, 0x00, sizeof(md5Token));
	memset(postSendBuf, 0x00, sizeof(postSendBuf));

    struct timeval tv;
    gettimeofday(&tv,NULL);
	long long timeStamp = (long long)tv.tv_sec*1000 + tv.tv_usec/1000;
	sprintf(token, "%s%s%llu", appId, appKey, timeStamp);
    AISDK_INFO(LX("getMusicUrl").d("token: ", token));

	compute_string_md5((unsigned char *)token, strlen(token), md5Token);

	sprintf(postSendBuf, TRACK_LINK_URI, timeStamp, aiuiUid, md5Token, appId, kugouUserId, kugouUserToken, appId, clientDeviceId, itemId);
	if(NULL != albumId)
	{
		sprintf(postSendBuf, "%s&albumid=%s", postSendBuf, albumId);
	}
    AISDK_INFO(LX("getMusicUrl").d("postSendBuf: ", postSendBuf));

	p = httpPost(postSendBuf, "");
	if(p != NULL)
	{
    	AISDK_INFO(LX("getMusicUrl").d("p: ", p));

		cJSON *json = NULL, *json_msg = NULL, *json_result = NULL, *json_audiopath = NULL;
		json = cJSON_Parse(p);
		if(!json)
		{
    		AISDK_ERROR(LX("getMusicUrl").d("JSON ERROR before: ", cJSON_GetErrorPtr()));
			return -2;
		}
		else
		{
			json_msg = cJSON_GetObjectItem( json , "msg" );  //获取键值内容
			if(0 == strcmp(json_msg->valuestring, "success"))
			{
    			AISDK_INFO(LX("getMusicUrl").d("json_msg: ", json_msg->valuestring));
				json_result = cJSON_GetObjectItem( json , "result" );  //获取键值内容
				if(json_result != NULL)
				{
					int array_size = cJSON_GetArraySize(json_result);
					int i;

					for(i = 0; i < array_size; i++)
					{
						cJSON *pSub = cJSON_GetArrayItem(json_result, i);
						if(NULL == pSub)
						{
							continue;
						}
						if(pSub != NULL)
						{
							json_audiopath = cJSON_GetObjectItem(pSub, "audiopath");
							sprintf(kugouMusicUrl, "%s", json_audiopath->valuestring);
    						AISDK_INFO(LX("getMusicUrl").d("kugouMusicUrl: ", kugouMusicUrl));
						}
					}
				}
			}
			cJSON_Delete(json);  //释放内存
		}
	}
	else
	{
    	AISDK_INFO(LX("getMusicUrl").d("kugouMusicUrl: ", "url is null"));
		return -1;
	}

	return 0;
}
