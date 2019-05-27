#ifndef __MQ_INTERNAL_H__
#define __MQ_INTERNAL_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <errno.h>
#include <assert.h>

#ifdef __cplusplus
extern "C"
{
#endif

//#define TAG "MQ_TAG"
#define mq_log(fmt,arg...) do { printf("MQ_TAG, "fmt,##arg); } while(0)

typedef int MQ_ID;

typedef struct MqRegInfo
{
    int         msg_ipc_key;
    int         mq_flag;
}MQ_REG_INFO_T;

MQ_ID mq_create();

MQ_ID mq_getMqId();

void mq_ctrl();


#ifdef __cplusplus
}
#endif

#endif
