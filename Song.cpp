#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <syslog.h>
#include <unistd.h>
#include <zmq.hpp>
#include <thread>
#include "Song.h"
#include "Conf.h"

#define  RR   0x01
#define  YY   0x02
#define  WW   0x04
#define  RS   0x08
#define  YS   0x10
#define  WS   0x20

#define  SERVER_PORT  50000
using namespace std;

static int exit_flag = 0;
static int rgb = 0;
static int control = 0;
static int sock_fd = 0;
static uint8_t  RGB_SC[10] = {0XCC, 0XDD, 0XA1, 0X01, 0X00, 0X00, 0X00, 0X38, 0XDA, 0XB4};

static pthread_rwlock_t rwlock;
static struct sockaddr_in ser_addr;

int create_udp_sock(const string  ip, const int port)
{
    memset(&ser_addr, 0, sizeof(ser_addr));
    ser_addr.sin_family = AF_INET;
    inet_aton(ip.c_str(), (struct in_addr *)&ser_addr.sin_addr);
    ser_addr.sin_port = htons(SERVER_PORT);

    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);

    return sock_fd;
}

#if 0
static int create_tcp_sock(const string ip, const int port)
{
    ///定义sockfd
    int sock_cli = socket(AF_INET,SOCK_STREAM, 0);

    ///定义sockaddr_in
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);  ///服务器端口
    servaddr.sin_addr.s_addr = inet_addr(ip.c_str());  ///服务器ip

    ///连接服务器，成功返回0，错误返回-1
    if(connect(sock_cli, (struct sockaddr *)&servaddr, sizeof(servaddr))  < 0)
    {
        syslog(LOG_INFO, "connect error\n");
        return -1;
    }

    return sock_cli;
}
#endif

int  read_write_udp(uint8_t* data, int len)
{
    char buf[1024];
    int  ret = 0;
    int  addr_len = sizeof(struct sockaddr_in);

    const  uint8_t  BB[9] = {0xEE, 0xFF, 0xC0, 0x01, 0x00, 0x01, 0x00, 0xC2, 0x84};
    
    fd_set fds;
    int    n = 0;
    struct timeval tv;

    sendto(sock_fd, data, len, 0, (struct sockaddr*)&ser_addr, sizeof(ser_addr));

    FD_ZERO(&fds);
    FD_SET(sock_fd, &fds);

    tv.tv_sec = 1;
    tv.tv_usec = 0;
    n = select(sock_fd+1, &fds, NULL, NULL, &tv);
    if(n == 0)
    {
        syslog(LOG_INFO, "timeout!\n");
        return 0;
    }
    else if(n == -1)
    {
        syslog(LOG_INFO, "error!\n");
        return -1;
    }

    bzero(buf, 1024);
    ret = recvfrom(sock_fd, buf, 1024, 0, (struct sockaddr*)&ser_addr, (socklen_t*)&addr_len);
    if(ret > 0)
    {
        if(memcmp(BB, buf, 9) == 0)
        {
            read_write_udp(RGB_SC, 10);
        }
    }
    return 0;
}

void  Sig_light()
{
    sleep(1);   
    
    int      i = 0;
    int      rgb1 = 0;
    uint8_t  RGB_O[10] = {0XCC, 0XDD, 0XA1, 0X01, 0X00, 0X00, 0X00, 0X07, 0XA9, 0X52};
    uint8_t  RGB_C[10] = {0XCC, 0XDD, 0XA1, 0X01, 0X00, 0X00, 0X00, 0X07, 0XA9, 0X52};
    uint8_t  RGB_SO[10] = {0XCC, 0XDD, 0XA1, 0X01, 0X00, 0X00, 0X00, 0X07, 0XA9, 0X52};
    while(1)
    {
        if(control == 1)
        {
TODO:
            pthread_rwlock_rdlock(&rwlock);
            rgb1 = rgb;
            pthread_rwlock_unlock(&rwlock);
            /****************************************/
            RGB_O[4] = 0;
            RGB_O[5] = rgb1 & 0x07;
            RGB_O[6] = 0;
            RGB_O[7] = (rgb1 & 0x07) != 0 ? rgb1 & 0x07 : 0x07;
            RGB_O[8] = (0xa2 + RGB_O[5] + RGB_O[7]) & 0xff;
            RGB_O[9] = (0xa2 + RGB_O[5] + RGB_O[7] + RGB_O[8]) & 0xff;

            /****************************************/
            RGB_C[4] = 0;
            RGB_C[5] = 0;
            RGB_C[6] = 0;
            RGB_C[7] = RGB_O[7];
            RGB_C[8] = (0xa2 + RGB_C[7]) & 0xff;
            RGB_C[9] = (0xa2 + RGB_C[7] + RGB_C[8]) & 0xff;

            /****************************************/
            RGB_SO[4] = 0;
            if(rgb1 & 0x08)
            {
                RGB_SO[5] = 0x08;
            }
            else if(rgb1 & 0x10)
            {
                RGB_SO[5] = 0x10;
            }
            else if(rgb1 & 0x20)
            {
                RGB_SO[5] = 0x20;
            }
            else
            {
                RGB_SO[5] = 0;
            }

            RGB_SO[6] = 0;
            RGB_SO[7] = 0x38;
            RGB_SO[8] = (0xa2 + RGB_SO[5] + RGB_SO[7]) & 0xff;
            RGB_SO[9] = (0xa2 + RGB_SO[5] + RGB_SO[7] + RGB_SO[8]) & 0xff;

            read_write_udp(RGB_SO, 10);
            if(memcmp(RGB_C, RGB_O, 10) == 0)
            {
                read_write_udp(RGB_C, 10);
            }
            else
            {
                for(i = 0; i < 10; i++)
                {
                    read_write_udp(RGB_C, 10);
                    usleep(500000);

                    read_write_udp(RGB_O, 10);
                    usleep(500000);

                    if(exit_flag == 1)
                    {
                        exit_flag = 0;
                        control = 1;
                        goto TODO;
                    }
                }
            }

            read_write_udp(RGB_SC, 10);
            control = 0;
        }
        else
        {
            usleep(10);
        }
    }
}

void  Hop()
{
    sleep(1);   
    uint8_t  AA[9] = {0xEE, 0xFF, 0xC0, 0x01, 0xFF, 0xFF, 0x0D, 0xCC, 0x98};
    while(1)
    {
        sleep(1);
        read_write_udp(AA, 9);
    }
}

void  Song()
{
    sock_fd = create_udp_sock("10.71.14.100", 50000);
    pthread_rwlock_init(&rwlock, NULL);

    char  ip[256] = {0};
    char  constr[256] = {0};
    get_web_ip(ip);
    snprintf(constr, 256, "tcp://%s:7777", ip);

    syslog(LOG_INFO, "web ip is %s", ip);
    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REP);
    socket.bind (constr);
    
    int  fatal = 0;
    int  serious = 0;
    int  common = 0;

    int  fatal_s = 0;
    int  serious_s = 0;
    int  common_s = 0;

    int  fatal_o = -1;
    int  serious_o = -1;
    int  common_o = -1;

    int  rgb1 = 0;
    while(1)
    {
        syslog(LOG_INFO, "等待客户端请求\n");
        zmq::message_t request(1024);
        memset(request.data(), 0, 1024);
        socket.recv (&request);

        if((0 == strncmp((char*)request.data(), "recoverRing", strlen("recoverRing"))) || (0 == strncmp((char*)request.data(), "removeRing", strlen("removeRing"))))
        {
            read_write_udp(RGB_SC, 10);
            syslog(LOG_INFO, "removeRing\n");
        }
        else
        {
            fatal_o = fatal;
            serious_o = serious;
            common_o = common;
            //sscanf((const char*) request.data(), "%*[^0-9]%d%*[^0-9]%d%*[^0-9]%d", &fatal, &serious, &common);
            sscanf((const char*) request.data(), "%*[^0-9]%d%*[^0-9]%d%*[^0-9]%d%*[^0-9]%d%*[^0-9]%d%*[^0-9]%d", &fatal_s, &fatal, &serious_s, &serious, &common_s, &common);
            syslog(LOG_INFO, "request = %s\nfatal = %d: %d, serious = %d:%d, common = %d:%d\n", (char*)request.data(), fatal, fatal_s, serious, serious_s, common, common_s);

            rgb1 = 0;
            if(fatal != fatal_o || serious != serious_o || common != common_o)
            {
                if(fatal > fatal_o || serious > serious_o || common > common_o)
                {
                    if(fatal > fatal_o)
                    {
                        rgb1 |= RR; 
                        fatal_s > 0 ? (rgb1 |= RS) : (rgb1 &= ~RS);
                    }
                    else if(fatal == 0)
                    {
                        rgb1 &= ~RR; 
                        rgb1 &= ~RS;
                    }

                    if(serious > serious_o)
                    {
                        rgb1 |= YY;
                        serious_s > 0 ? (rgb1 |= YS) : (rgb1 &= ~YS);
                    }
                    else if(serious == 0)
                    {
                        rgb1 &= ~YY;
                        rgb1 &= ~YS; 
                    }

                    if(common > common_o)
                    {
                        rgb1 |= WW; 
                        common_s > 0 ? (rgb1 |= WS) : (rgb1 &= ~WS); 
                    }
                    else if(common == 0)
                    {
                        rgb1 &= ~WW; 
                        rgb1 &= ~WS; 
                    }

                    pthread_rwlock_wrlock(&rwlock);
                    rgb = rgb1;
                    pthread_rwlock_unlock(&rwlock);

                    control = 1;
                    exit_flag = 1;
                }
                else if(fatal == 0 || serious ==  0 || common == 0)
                {
                    uint8_t  flag = 0;
                    uint8_t  data[10] = {0XCC, 0XDD, 0XA1, 0X01, 0X00, 0X00, 0X00, 0X07, 0XA9, 0X52};
                     
                    rgb1 = 0;

                    if(fatal == 0)
                    {
                        rgb1 &= ~RR; 
                        rgb1 &= ~RS;
                        flag |= 0x09;
                    }

                    if(serious == 0)
                    {
                        rgb1 &= ~YY;
                        rgb1 &= ~YS;
                        flag |= 0x12;
                    }

                    if(common == 0)
                    {
                        rgb1 &= ~WW; 
                        rgb1 &= ~WS; 
                        flag |= 0x24;
                    }

                    control = 0;
                    exit_flag = 1;
                    usleep(500000);

                    data[4] = 0;
                    data[5] = rgb1 & 0x3f;
                    data[6] = 0;
                    data[7] = flag;
                    data[8] = (0xa2 + data[5] + data[7]) & 0xff;
                    data[9] = (0xa2 + data[5] + data[7] + data[8]) & 0xff;
                    read_write_udp(data, 10);
                }
           }
           syslog(LOG_INFO,  "rgb = %02x\n", rgb);
       }
       

        zmq::message_t reply (5);
        memcpy ((void *) reply.data (), "World", 5);
        socket.send (reply);
    }
    pthread_rwlock_destroy(&rwlock);
}

