#ifndef __MQ_API_H__
#define __MQ_API_H__

#include "mq_internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define IN
#define OUT
#define INOUT

typedef void (*RECV_CB_FUNC)(void);
typedef void (*MSG_FREE_PPARAM_CB)(void* ptr);

enum MSG_TYPE{
	GM_MSG_PULSE = 0x0002,
	GM_MSG_SAMPLE,
	GM_MSG_GM_TASK,
	GM_MSG_COAP
};

typedef enum {
    // status mode
    LED_MODE_MUTE = 1,
    LED_MODE_NET_CUSTOM,
    LED_MODE_NET_CONNECT,   // once time

    // normal mode
    LED_MODE_WAKEUP,
    LED_MODE_TTS,
    LED_MODE_MP3_PAUSE,     // once time
    LED_MODE_MP3,           // once time

    LED_MODE_NET_DIS,
    LED_MODE_STANDBY,

    // init
    LED_MODE_PWRON,     // once time

    LED_MODE_INVALID,
}e_LED_MODE;

typedef enum {
	KEY_EVT_VOL_UP = LED_MODE_INVALID + 1,//12
	KEY_EVT_VOL_DOWN,
	KEY_EVT_MUTE,
	KEY_EVT_PAUSE_AND_RESUME,//15
	KEY_EVT_TURN_ON, // system turn on , 
	KEY_EVT_NEXT,    // NEXT MUSIC
	KEY_EVT_LAST,    // LAST MUSIC
	KEY_EVT_VOIP_START,
	KEY_EVT_VOIP_END,
	KEY_EVT_NET,     // connect net 
	KEY_EVT_TEST,
	KEY_EVT_FM,
	KEY_EVT_LED,    //保留
	BRINGUP
}eKEY_EVENT;

typedef struct MqSubMsgInfo
{
    uint32_t sub_id;        //which is used for splite sub msg type;
	uint32_t status;
    uint32_t iparam;        //param format is integer
    uint8_t md5[64];  //param format is pointer
    uint8_t content[1024];  //param format is pointer
    uint32_t content_len;   //real lenth of content
}MQ_SUB_MQ_MSG_INFO_T;

typedef struct MqMsgInfo
{
    int64_t msg_type;              //which is msg type for msg interface
    MQ_SUB_MQ_MSG_INFO_T sub_msg_info; //real msg content
}MQ_MSG_INFO_T;

typedef struct MqSndInfo
{
    MQ_MSG_INFO_T msg_info;
    int32_t     mq_flag;
}MQ_SND_INFO_T;

typedef struct MqRecvInfo
{
    MQ_MSG_INFO_T msg_info;
    int32_t     mq_flag;
}MQ_RECV_INFO_T;

ssize_t mq_send(IN const MQ_SND_INFO_T* snd_info_ptr);

ssize_t mq_recv(INOUT MQ_RECV_INFO_T* recv_info_ptr);

#ifdef __cplusplus
}
#endif

#endif
