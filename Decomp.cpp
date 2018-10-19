#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <arpa/inet.h>
#include "Decomp.h"

using namespace std;

int  decomp(const uint8_t *input, const int inlen, char *output, int *outlen)
{
    uint8_t  stype1;
    uint8_t  stype2;
    stype1 = *(uint8_t*)input;
    stype2 = *(uint8_t*)(input+1);

    if((stype1 == 0x7e && stype2 == 0xff) || (stype1 == 0xff && stype2 == 0x7e))
    {
        SEGMENT *p = NULL;
        p = (struct SEGMENT*)input;

        if(p->son_sys == 0x02)
        {
            if(p->res1 == 1)
            {
                p->res1 = 0;
            }
            else if(p->res1 == 2 || p->res1 == 3)
            {
                p->res1 = 1;
            }
            else if(p->res1 == 4 || p->res1 == 5)
            {
                p->res1 = 2;
            }
        }
        else if(p->son_sys == 0x03)
        {
            if(p->res1 == 3)
            {
                p->res1 = 0;
            }
            else if(p->res1 == 2)
            {
                p->res1 = 1; 
            }
            else if(p->res1 == 1)
            {
                p->res1 = 2;
            }
        }

        *outlen = sprintf((char*)output, WAR_JSON_STR, ntohs(p->type), ntohs(p->fnum), p->flen, p->son_sys, p->stop,  p->eng,  p->node,  ntohs(p->bug),  p->tt.year_h,  p->tt.year_l,  p->tt.mon, p->tt.day,  p->tt.hh,  p->tt.mm,   p->tt.ss,   ntohs(p->res1), ntohs(p->res2), ntohs(p->res3), ntohs(p->check));
    }
    else if(stype1 == 0xaa || stype1 == 0xff)
    {
        SHAKE  *p = NULL;
        p = (struct SHAKE*)input;
        *outlen = sprintf((char*)output, SHAKE_JSON_STR, htons(p->type), p->len, p->son_sys, p->tt.year_h,  p->tt.year_l,  p->tt.mon, p->tt.day,  p->tt.hh,  p->tt.mm,   p->tt.ss);
    }
    else
    {
        *outlen = -1;
    }

    return 0;
}
