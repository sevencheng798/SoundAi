/*
 * Copyright 2018 its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#include <stdio.h>
#include <string.h>

namespace aisdk {
namespace mediaPlayer {
namespace ffmpeg {
 
int checkHasChinese(const char *str) {
    int nRet = 0;
    char c;
    while((c=*str++))
    {
        //����ַ���λΪ1����һ�ַ���λҲ��1���������ַ�
        if( (c&0x80) && (*str & 0x80) )
        {
            nRet = 1;
            break;
        }
    }
    return nRet;
}

/**
 * @brief URLEncode ���ַ���URL����
 *
 * @param str ԭ�ַ���
 * @param strSize ԭ�ַ�������(����������\0)
 * @param result ����������ĵ�ַ
 * @param resultSize ����������Ĵ�С(��������\0)
 *
 * @return: >0:resultstring ��ʵ����Ч�ĳ���
 *            0: ����ʧ��.
 */
int URLEncode(const char* str, const int strSize, char* result, const int resultSize)
{
    int i;
    int j = 0;//for result index
    char ch;
 
    if ((str==NULL) || (result==NULL) || (strSize<=0) || (resultSize<=0)) {
        return 0;
    }
 
    for ( i=0; (i<strSize)&&(j<resultSize); ++i) {
        ch = str[i];
        if (((ch>='A') && (ch<'Z')) ||
            ((ch>='a') && (ch<'z')) ||
            ((ch>='0') && (ch<'9'))) {
            result[j++] = ch;
        //} else if (ch == ' ') {
           // result[j++] = '+';
        } else if (ch == '.' || ch == '-' || ch == '_' || ch == '*') {
            result[j++] = ch;
        } else {
            if (j+3 < resultSize) {
                sprintf(result+j, "%%%02X", (unsigned char)ch);
                j += 3;
            } else {
                return 0;
            }
        }
    }
 
    result[j] = '\0';
    return j;
}

}  // namespace ffmpeg
}  // namespace mediaPlayer
} //namespace aisdk