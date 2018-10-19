#ifndef __DECOMP__H__
#define __DECOMP__H__
#pragma  pack (1)

#define  WAR_JSON_STR   "{ type: \"%d\", fnum: \"%d\", flen: \"%d\", son_sys: \"%d\", stop: \"%d\", eng: \"%d\", node:\"%d\", bug: \"%d\", time: \"%02d%02d-%02d-%02d %02d:%02d:%02d\", res1: \"%d\", res2: \"%d\", res3: \"%d\", check: \"%d\"}"
#define  SHAKE_JSON_STR   "{ type:\"%d\", len: \"%d\", son_sys: \"%d\", time: \"%02d%02d-%02d-%02d  %02d:%02d:%02d\"}"

typedef struct TT{
    uint8_t   year_h;
    uint8_t   year_l;
    uint8_t   mon;
    uint8_t   day;
    uint8_t   hh;
    uint8_t   mm;
    uint8_t   ss;
}TT;

typedef struct SEGMENT
{
    uint16_t  type;
    uint16_t  fnum;
    uint8_t   flen;
    uint8_t   son_sys;
    uint8_t   stop;
    uint8_t   eng;
    uint8_t   node;

    uint16_t   bug;

    TT         tt;

    uint16_t  res1;
    uint16_t  res2;
    uint16_t  res3;

    uint16_t  check;
}SEGMENT;

typedef struct SHAKE
{
    uint8_t   type;
    uint8_t   len;
    uint8_t   son_sys;

    TT         tt;
}SHAKE;

int  decomp(const uint8_t *input, const int inlen, char *output, int *outlen);
#endif
