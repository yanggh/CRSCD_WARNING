#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <iconv.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include "g2u.h"
#define OUTLEN 1024
//extern 'C'
//代码转换:从一种编码转为另一种编码
static  const int code_convert(const char *from_charset,const char *to_charset,char *inbuf,size_t inlen,char *outbuf,size_t outlen)
{
    iconv_t cd;
    char **pin = &inbuf;
    char **pout = &outbuf;

    cd = iconv_open(to_charset,from_charset);
    if (cd==0) return -1;
    memset(outbuf,0,outlen);
    if (iconv(cd,pin,&inlen,pout,&outlen) == (size_t)1) 
        return -1;
    iconv_close(cd);
    return 0;
}
//UNICODE码转为GB2312码
//static int u2g(char *inbuf,size_t inlen,char *outbuf,size_t outlen)
//{
//    return code_convert("utf-8","gb2312",inbuf,inlen,outbuf,outlen);
//}
//GB2312码转为UNICODE码
static int g2u(char *inbuf,size_t inlen,char *outbuf,size_t outlen)
{
    return code_convert("gb2312","utf-8",inbuf,inlen,outbuf,outlen);
}

int  get_value(char  *str, int length, char** out)
{
    int  i = 0;
    int  ret = 0;
    char ps[1024];
    char in_gb2312[1024];
    int  value = 0;

    bzero(in_gb2312, 1024);
    memcpy(ps, str,  length);

    char *token = NULL;
    token = strtok(ps, " ");
    if(token != NULL)
    {
        in_gb2312[i++] = strtol(token, NULL, 16);
    }

    while(1)
    {
            token = strtok(NULL, " ");
            if(token != NULL)
            {
                if(strlen(token) == 2)
                {
                    value = strtol(token, NULL, 16);
                }
                else
                {
                    value = 0;
                }
                
                syslog(LOG_INFO, "aaaaaaaaaaaa ===========%s  %d\n", token, value);
                if(value > 0)
                {
                    in_gb2312[i++] = value & 0xff;
                }
                value = 0;
            }
            else
            {
                break;
            }
    }

    //gb2312码转为unicode码
    ret = g2u(in_gb2312,strlen(in_gb2312),*out,OUTLEN);
    return ret;
}
