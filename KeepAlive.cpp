#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <thread>
#include <queue>
#include <list>
#include <sys/time.h>
#include <signal.h>
#include <mysql.h>
#include <zmq.hpp>
#include <pthread.h>
#include <syslog.h>
#include <modbus.h>
#include "KeepAlive.h"
#include "RecvSnmp.h"
#include "Decomp.h"
#include "Conf.h"

#define  WARING         0
#define  KEEPALIVE      1
#define  DISCON         0
#define  CONNECT        1
#define  SYSSUB         0
#define  IPSUB          1
#define  PORTSUB        2
#define NB_CONNECTION   5

#define  KEEP_ALIVE_STR   "{ type: \"0xFFFF\", fnum: \"0\", flen: \"0\", son_sys: \"%d\", stop: \"%d\", eng: \"0\", node:\"0\", bug: \"0\", time: \"0\", res1: \"%s\", res2: \"%d\", res3: \"%d\", check: \"0\"}"
//#define  MYSQL_SYS_STR   "SELECT SUBSYSTEM_CODE,SERVER_IP,SERVER_PORT FROM TBL_RESMANAGE_SUBSYSTEMINFO;"
#define  MYSQL_SYS_STR    "SELECT SUBSYSTEM_CODE,SERVER_IP,SERVER_PORT FROM TBL_RESMANAGE_SUBSYSTEMINFO WHERE SUBSYSTEM_CODE != 1 AND SUBSYSTEM_CODE != 2 AND SUBSYSTEM_CODE < 10;"

//#define  MYSQL_ISCS_STR  "SELECT b.SUBSYSTEM_CODE,COUNT(*) FROM TBL_ALARM_ALARMINFO a LEFT JOIN TBL_RESMANAGE_SUBSYSTEMINFO b ON a.SUBSYSTEMID=b.SUBSYSTEMID WHERE a.ALARM_STATE=0 AND a.FAULTID!=\"\" AND a.FAULTUNITID!=\"\" AND a.SUBSYSTEMID!=\"\" AND a.STATIONID!=\"\"  GROUP BY a.SUBSYSTEMID  LIMIT  8" 
#define  MYSQL_ISCS_STR  "SELECT subsys_.SUBSYSTEM_CODE, IFNULL(sub_.QTY,0) qty FROM TBL_RESMANAGE_SUBSYSTEMINFO subsys_ LEFT JOIN (SELECT b.SUBSYSTEM_CODE,COUNT(*) QTY FROM TBL_ALARM_ALARMINFO a LEFT JOIN TBL_RESMANAGE_SUBSYSTEMINFO b ON a.SUBSYSTEMID=b.SUBSYSTEMID WHERE a.ALARM_STATE=0 AND a.FAULTID!=\"\" AND a.FAULTUNITID!=\"\" AND a.SUBSYSTEMID!=\"\" AND a.STATIONID!=\"\"  GROUP BY a.SUBSYSTEMID  LIMIT  8) sub_ ON subsys_.SUBSYSTEM_CODE=sub_.SUBSYSTEM_CODE LIMIT 9;"
#define  MYSQL_ISCSA_STR  "SELECT        station.STATION_NAME,   IFNULL(q.QTY,0) qty  FROM TBL_RESMANAGE_STATIONINFO station LEFT JOIN(SELECT IFNULL(count(*),0) QTY,alarm.STATIONID FROM TBL_ALARM_ALARMINFO  alarm, TBL_RESMANAGE_FAULTINFO  fault ,TBL_RESMANAGE_SUBSYSTEMINFO system  WHERE alarm.FAULTID=fault.FAULTID AND fault.FAULT_CODE=4226 AND alarm.SUBSYSTEMID=system.SUBSYSTEMID AND system.SUBSYSTEM_CODE=8 AND alarm.ALARM_STATE!=2 GROUP BY  STATIONID) q ON station.STATIONID=q.STATIONID WHERE STATION_ORDER_NO <> 97 AND STATION_ORDER_NO <> 98 AND STATION_ORDER_NO <> 99 ORDER BY station.STATION_ORDER_NO asc  LIMIT 34"
using namespace std;

typedef struct TTT{
    uint8_t   year_h;
    uint8_t   year_l;
    uint8_t   mon;
    uint8_t   day;
    uint8_t   hh;
    uint8_t   mm;
    uint8_t   ss;
}TTT;

typedef struct ALIVE
{
    uint8_t type;
    uint8_t flen;
    uint8_t son_sys;
    TTT  tt;
}ALIVE;

typedef struct Node
{
    uint8_t  son_sys;
    uint8_t  stop;
    string  ip;
    int  port;
    int  flag;
    int  timeout;
    struct  sockaddr_in  sin;
}Node;

static modbus_t *ctx1 = NULL;
static modbus_t *ctx2 = NULL;
static modbus_mapping_t *mb_mapping1 = NULL;
static modbus_mapping_t *mb_mapping2 = NULL;
static const uint16_t UT_INPUT_REGISTERS_ADDRESS = 0x01;
static const uint16_t UT_INPUT_REGISTERS_NB = 50;
static int server_socket1 = -1;
static int server_socket2 = -1;

static int  sockfd = -1;
static list<Node> clist;
static list<string> jlist;
static list<string> jsnmplist;
static pthread_mutex_t clist_lock;
static pthread_mutex_t jlist_lock;
static pthread_mutex_t jsnmplist_lock;

int   Init_lock()
{
    pthread_mutex_init(&clist_lock, NULL);
    pthread_mutex_init(&jlist_lock, NULL);
    pthread_mutex_init(&jsnmplist_lock, NULL);
    return 0; 
}

int  add_snmp_json_str(const char* str)
{
    pthread_mutex_lock(&jsnmplist_lock);
    jsnmplist.push_back(str);
    pthread_mutex_unlock(&jsnmplist_lock);
    return 0;
}

int   add_json_str(const char* str)
{
    pthread_mutex_lock(&jlist_lock);
    jlist.push_back(str);
    pthread_mutex_unlock(&jlist_lock);
    return 0;
}

int   keep_snmp_json_str(const int  son_sys, const int stop, const char* ip, const int port, const int flag)
{
    char son_stat[256];
    bzero(son_stat, 256);
    int iLen = snprintf(son_stat, 256, KEEP_ALIVE_STR,  son_sys, stop, ip, port, flag);
    if(iLen > 0)
    {
        add_snmp_json_str(son_stat);
        syslog(LOG_INFO, "son_sys status is: %s\n", son_stat);
    }
    else
    {
        syslog(LOG_ERR, "keep_snmp_json_str error.\n");
    }

    return 0;
}

int   keep_json_str(const int  son_sys, const int stop, const char* ip, const int port, const int flag)
{
    char son_stat[256];
    bzero(son_stat, 256);
    int iLen = snprintf(son_stat, 256, KEEP_ALIVE_STR,  son_sys, stop, ip, port, flag);
    if(iLen > 0)
    {
        add_json_str(son_stat);
        syslog(LOG_INFO, "son_sys status is: %s\n", son_stat);
    }
    else
    {
        syslog(LOG_ERR, "keep_json_str error.\n");
    }

    return 0;
}


int ConsumerTask(void)
{
TODO:
    zmq::context_t context (1);
    zmq::socket_t  socket (context, ZMQ_REQ);

    char         web_ip[256] = {0};
    char         constr[256] = {0};

    get_web_ip(web_ip);
    int  web_port = get_web_port();
    snprintf(constr, 256, "tcp://%s:%d", web_ip, web_port);
    syslog(LOG_INFO, "constr ============ %s\n", constr);
    socket.connect (constr);
    
    list<string>::iterator  p;
    while(1)
    {
       if(jsnmplist.size() > 0)
       {
            pthread_mutex_lock(&jsnmplist_lock);
            p = jsnmplist.begin();
            pthread_mutex_unlock(&jsnmplist_lock);

            zmq::message_t req(strlen(p->c_str()));
            memcpy((void*)req.data(), p->c_str(), strlen(p->c_str()));
            syslog(LOG_INFO, "req.data is %sa\n", p->c_str());
            if(socket.send(req) < 0)
            {
                syslog(LOG_ERR, "zmq send error!\n");
                goto TODO;
            }

            pthread_mutex_lock(&jsnmplist_lock);
            jsnmplist.pop_front();
            pthread_mutex_unlock(&jsnmplist_lock);

            zmq::message_t reply;
            if(socket.recv (&reply) < 0)
            {
                syslog(LOG_ERR, "zmq recv error!\n");
                goto TODO;
            }
            syslog(LOG_INFO, "snmp reply.data is %s\n", (char*)reply.data());
        }
        
        if(jlist.size() > 0)
        {
            pthread_mutex_lock(&jlist_lock);
            p = jlist.begin();
            pthread_mutex_unlock(&jlist_lock);

            zmq::message_t req(strlen(p->c_str()));
            memset((void*)req.data(), 0, strlen(p->c_str()));
            memcpy((void*)req.data(), p->c_str(), strlen(p->c_str()));
            syslog(LOG_INFO, "req.data is %sa\n", p->c_str());
            if(socket.send(req) < 0)
            {
                syslog(LOG_ERR, "zmq send error!\n");
                goto TODO;
            }

            pthread_mutex_lock(&jlist_lock);
            jlist.pop_front();
            pthread_mutex_unlock(&jlist_lock);

            zmq::message_t reply;
            if(socket.recv (&reply) < 0)
            {
                syslog(LOG_ERR, "zmq recv error!\n");
                goto TODO;
            }
            syslog(LOG_INFO, "udp reply.data is %s\n", (char*)reply.data());
        }
    }
    return 0;
}

static int  create_sock()
{
	struct sockaddr_in sin;  
    char sip[256] = {0};

    get_sip(sip);
    if(((sockfd = socket(AF_INET, SOCK_DGRAM, 0))< 0))
    {
        syslog(LOG_ERR, "create  socket error");
        return -1;
    } 

    int sport = get_sport();

	bzero(&sin,sizeof(sin));  
	sin.sin_family=AF_INET;  
	sin.sin_port=htons(sport);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);

	if(bind(sockfd, (struct sockaddr *)&sin, sizeof(struct sockaddr_in)) < 0)
    {
        syslog(LOG_ERR, "bind udp service error, sip = %s, port = %d, error = %s\n", sip, sport, strerror(errno));
        return -1;
    }

    return sockfd;
}

static  int CheckClient(string ip,  int port)
{
    int   timeout = get_keepalive();

    list<Node>::iterator itor;

    pthread_mutex_lock(&clist_lock);
    itor = clist.begin();
    while(itor != clist.end())
    {
        if((itor->ip == ip))
        {
            itor->timeout = timeout;

            if(itor->flag == DISCON)
            {
                itor->flag = CONNECT;
                keep_json_str(itor->son_sys, itor->stop, itor->ip.c_str(), itor->port, itor->flag);
            }
            break;
        }
        itor ++;
    }
    pthread_mutex_unlock(&clist_lock);

    return 0;
}

static  int check_pack(const uint8_t *str, const uint16_t iLen, short *fnum)
{
    if((str[0] == 0xFF && str[1] == 0x7E) || (str[0] == 0x7E && str[1] == 0xFF))
    {
        *fnum = *(short*)(str+2);
        return WARING;     
    }
    else if(str[0] == 0xFF)
    {
        return KEEPALIVE;
    }

    return  -1;
}

static int Getalive(ALIVE *alive)
{
    time_t tt = time(NULL);
    struct tm *t = localtime(&tt);

    alive->type = 0xaa;
    alive->flen = sizeof(ALIVE);
    alive->son_sys = 6;

    alive->tt.year_h = (t->tm_year + 1900) / 100;
    alive->tt.year_l = (t->tm_year + 1900) % 100;
    alive->tt.mon = t->tm_mon + 1;
    alive->tt.day = t->tm_mday + 1;
    alive->tt.hh = t->tm_hour;
    alive->tt.mm = t->tm_min;
    alive->tt.ss = t->tm_sec; 

    return 0;
}

static void Alive(int signo)
{
    if(sockfd == -1)
        return;
    Update_timeout();

    ALIVE alive;
    Getalive(&alive);
   
    list<Node>::iterator itor;
    
    pthread_mutex_lock(&clist_lock);
    itor = clist.begin();
    while(itor != clist.end())
    {
        if(itor->timeout > 0)
        {
            itor->timeout --;
        }
        else if(itor->timeout == 0)
        {
            if(itor->flag == CONNECT)
            {
                itor->flag = DISCON;
                keep_json_str(itor->son_sys, itor->stop, itor->ip.c_str(), itor->port, itor->flag);
            }
        }

        alive.son_sys = itor->son_sys;
        sendto(sockfd, (void*)&alive, sizeof(ALIVE), 0, (struct sockaddr*)&(itor->sin), sizeof(itor->sin));
        itor++;
    }
    pthread_mutex_unlock(&clist_lock);
}

int  Iscs(void)
{
    int  sub = 0;
    char stat = 0;
    int  ret = 0;

    sleep(5);
    char mysqlip[256] = {0};
    char username[256] = {0};
    char password[256] = {0};
    char database[256] = {0};

    MYSQL mysql;
    MYSQL_RES *my_res;
    MYSQL_ROW row;

    mysql_init(&mysql);

    get_mysql_ip(mysqlip);
    get_database(database);
    get_username(username);
    get_password(password);

    if(!mysql_real_connect(&mysql, mysqlip, username, password, database, 0, NULL, 0))
    {
        syslog(LOG_ERR, "无法连接到数据库:%s, %s, %s, %s，错误原因是:%s, Iscs exit!.\n", mysqlip, username, password, database, mysql_error(&mysql));
        return -1;
    }

    while(1) 
    {
        unsigned  int t=mysql_real_query(&mysql, MYSQL_ISCS_STR, (unsigned int)strlen(MYSQL_ISCS_STR));
        if(t == 0)
        {
            sub = 0;
            my_res = mysql_store_result(&mysql);//返回查询的全部结果集
            if(my_res != NULL)
            {
                while((row = mysql_fetch_row(my_res)) > 0)     
                {
                    stat = atoi(row[1]) > 0 ? 1 : 0;
                    if(stat > 0)
                    {
                        mb_mapping1->tab_input_registers[sub] = 1;
                        mb_mapping2->tab_input_registers[sub] = 1;

                    }
                    else
                    {
                        mb_mapping1->tab_input_registers[sub] = 0;
                        mb_mapping2->tab_input_registers[sub] = 0;

                    }
                    sub++;
                }
            }
        }
        else
        {
            syslog(LOG_ERR, "modbus mysql_real_query error!\n");
        }

        t = mysql_real_query(&mysql, MYSQL_ISCSA_STR, (unsigned int)strlen(MYSQL_ISCSA_STR));
        if(t == 0)
        {
            my_res = mysql_store_result(&mysql);//返回查询的全部结果集
            if(my_res != NULL)
            {
                while((row = mysql_fetch_row(my_res)) > 0)     
                {
                    stat = atoi(row[1]) > 0 ? 1 : 0;
                    if(stat > 0)
                    {
                        mb_mapping1->tab_input_registers[sub] = 1;
                        mb_mapping2->tab_input_registers[sub] = 1;

                    }
                    else
                    {
                        mb_mapping1->tab_input_registers[sub] = 0;
                        mb_mapping2->tab_input_registers[sub] = 0;
                    }
                    sub++;
                }
            }
        }
        else
        {
            syslog(LOG_ERR, "modbus mysql_real_query error!\n");
        }
        sleep(5);
    }

    mysql_free_result(my_res);
    mysql_close(&mysql);
    return ret;
}

int  RecvUdp()
{
    int  ret = 0;
    char message[1024];
    socklen_t  sin_len;
    struct sockaddr_in sin;
    char jsonstr[1024];
    int  iLen = 0;
    int  outlen = 0;
    int type = 0;
    short fnum = 0;

    ret = create_sock();
    if(ret == -1)
    {
        syslog(LOG_ERR, "create udp service sock error\n");
        return -1;
    }

    int i = 0;
    while(1)
    {
        bzero(message, 1024);
        size_t  len = 1024;
        iLen = recvfrom(sockfd,  message, len, 0, (struct sockaddr*)&sin, (socklen_t*)&sin_len);
        syslog(LOG_INFO, "i = %d, sockfd = %d, iLen = %d, jlist = %ld\n", i, sockfd, iLen, jlist.size());
        i++;
        if(iLen > 0)
        {
            type = check_pack((uint8_t*)message, iLen, &fnum);
            if(type == WARING) 
            {
                sendto(sockfd, (uint8_t*)&fnum, 2, 0, (struct sockaddr *)&sin, sin_len);
                decomp((uint8_t*)message, iLen, jsonstr, &outlen);
                add_json_str(jsonstr);
            }
            else if(type == KEEPALIVE)
            {
                CheckClient(inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
                
            }
        }
    }
    return 0;
}

static  int  init_client(void)
{
    int  ret = 0;
    int  timeout = 0;

    char mysqlip[256] = {0};
    char username[256] = {0};
    char password[256] = {0};
    char database[256] = {0};

    timeout = get_keepalive();

    syslog(LOG_INFO, "timeout == %d\n", timeout);
    pthread_mutex_lock(&clist_lock);
    if(!clist.empty())
    {
        clist.clear();
    }
    pthread_mutex_unlock(&clist_lock);

    MYSQL mysql;
    MYSQL_RES *my_res;
    MYSQL_ROW row;

    mysql_init(&mysql);

    get_mysql_ip(mysqlip);
    get_database(database);
    get_username(username);
    get_password(password);

    if(!mysql_real_connect(&mysql, mysqlip, username, password, database, 0, NULL, 0))
    {
        syslog(LOG_ERR, "无法连接到数据库:%s, %s, %s, %s，错误原因是:%s.\n", mysqlip, username, password, database, mysql_error(&mysql));
        return -1;
    }

    unsigned  int t=mysql_real_query(&mysql, MYSQL_SYS_STR, (unsigned int)strlen(MYSQL_SYS_STR));
    if(t == 0)
    {
        Node p; 
        my_res = mysql_store_result(&mysql);//返回查询的全部结果集
        if(my_res != NULL)
        {
            while((row = mysql_fetch_row(my_res)) > 0)     
            {
                p.stop = 1;
                p.son_sys = atoi(row[SYSSUB]);
                p.ip = string(row[IPSUB]);
                p.port = atoi(row[PORTSUB]);

                p.flag = CONNECT;
                p.timeout = timeout;

                bzero(&p.sin,sizeof(p.sin));  
                p.sin.sin_family=AF_INET;  
                p.sin.sin_addr.s_addr=inet_addr(p.ip.c_str());
                p.sin.sin_port=htons(p.port);

                pthread_mutex_lock(&clist_lock);
                clist.push_back(p);
                pthread_mutex_unlock(&clist_lock);
            }
        }
    }
    else
    {
        syslog(LOG_ERR, "client mysql_real_query error!\n");
        return -1;
    }

    mysql_free_result(my_res);
    mysql_close(&mysql);
    return ret;
}

//timer work
int KeepAlive()
{
    struct itimerval tick;

    int ret = init_client();
    if(ret == -1)
    {
        syslog(LOG_ERR, "not find udp client, keepalive fail!\n");
        return -1;
    }
    
    signal(SIGALRM,  Alive);
    memset(&tick, 0, sizeof(tick));

    tick.it_value.tv_sec = 1;
    tick.it_value.tv_usec = 0;

    tick.it_interval.tv_sec = 5;
    tick.it_interval.tv_usec = 0;

    if(setitimer(ITIMER_REAL, &tick, NULL) < 0)
    {
        syslog(LOG_ERR, "Set timer failed.");
        return -1;
    }

    while(true)
    {
        pause();
    }
    return 0;
}

int  UpdateSig()
{
    char     update_ip[256];
    char     constr[256];
    bzero(constr, 256);
    bzero(update_ip, 256);

    get_update_ip(update_ip);
    int update_port = get_update_port();
    snprintf(constr, 256, "tcp://*:%d", update_port);

    zmq::context_t context (1);
    zmq::socket_t socket (context, ZMQ_REP);
	socket.bind (constr);
        
    while (true) 
    {
        // 等待客户端请求
        zmq::message_t request;
        socket.recv (&request);
        
        if(memcmp(request.data(), "update", 6) == 0)
        {
            int  ret = init_client();
            if(ret == -1)
            {
                syslog(LOG_INFO, "update error.");
                continue;
            }
        }
        else
        {
            syslog(LOG_ERR, "error: request.size() = %ld.", request.size());
        }

        // 应答World
        zmq::message_t reply (5);
        memcpy ((void *) reply.data (), "World", 5);
        socket.send (reply);
    }

    return 0;
}

static void close_sigint()
{
    if (server_socket1 != -1) 
    {
        close(server_socket1);
        server_socket1 = -1;
    }

    if (server_socket2 != -1) 
    {
        close(server_socket2);
        server_socket2 = -1;
    }

    if(ctx1 != NULL)
    {
        modbus_free(ctx1);
        ctx1 = NULL;
    }

    if(ctx2 != NULL)
    {
        modbus_free(ctx2);
        ctx2 = NULL;
    }

    if(mb_mapping1 != NULL)
    {
        modbus_mapping_free(mb_mapping1);
    }

    if(mb_mapping2 != NULL)
    {
        modbus_mapping_free(mb_mapping2);
    }

    syslog(LOG_INFO, "close_sigint ");
}

int Sendmod_min()
{
    int rc;
    int i = 0;
    int master_socket;
    fd_set refset;
    fd_set rdset;
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    char  ip[256] = {0};
    /* Maximum file descriptor number */
    int fdmax;

    get_modbus_min_ip(ip);
    ctx2 = modbus_new_tcp(ip, 1502);
    if(ctx2 == NULL)
    {
        modbus_free(ctx2);
        ctx2 = NULL;
        return -1;
    }
    
    mb_mapping2 = modbus_mapping_new_start_address( 
            0, 0,
            0, 0,
            0, 0,
            UT_INPUT_REGISTERS_ADDRESS, UT_INPUT_REGISTERS_NB);
    if (mb_mapping2 == NULL) 
    {
        syslog(LOG_ERR, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        return -1;
    }

    for(i = 0; i < UT_INPUT_REGISTERS_NB; i++)
    {
        mb_mapping2->tab_input_registers[i] = 0;
    }

    modbus_set_slave(ctx2, 2);
#ifdef BUG
    modbus_set_debug(ctx2, TRUE);
#else
    modbus_set_debug(ctx2, FALSE);
#endif
    server_socket2 = modbus_tcp_listen(ctx2, NB_CONNECTION);
    if (server_socket2 == -1) 
    {
        syslog(LOG_ERR, "Unable to listen TCP connection\n");
        modbus_free(ctx2);
        ctx2 = NULL;
        return -1;
    }

    /* Clear the reference set of socket */
    FD_ZERO(&refset);
    /* Add the server socket */
    FD_SET(server_socket2, &refset);

    /* Keep track of the max file descriptor */
    fdmax = server_socket2;

    for (;;) 
    {
        rdset = refset;
        if (select(fdmax+1, &rdset, NULL, NULL, NULL) == -1) 
        {
            syslog(LOG_ERR, "Server select() failure.");
            close_sigint();
        }

        /* Run through the existing connections looking for data to be read */
        for (master_socket = 0; master_socket <= fdmax; master_socket++) 
        {
            if (!FD_ISSET(master_socket, &rdset)) 
            {
                continue;
            }

            if (master_socket == server_socket2) 
            {
                /* A client is asking a new connection */
                socklen_t addrlen;
                struct sockaddr_in clientaddr;
                int newfd;

                /* Handle new connections */
                addrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, sizeof(clientaddr));
                newfd = accept(server_socket2, (struct sockaddr *)&clientaddr, &addrlen);
                if (newfd == -1) 
                {
                    syslog(LOG_ERR, "Server accept() error");
                }
                else 
                {
                    FD_SET(newfd, &refset);

                    if (newfd > fdmax) 
                    {
                        /* Keep track of the maximum */
                        fdmax = newfd;
                    }
                    syslog(LOG_INFO, "New connection from %s:%d on socket %d", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
                }
            } 
            else 
            {
                modbus_set_socket(ctx2, master_socket);
                rc = modbus_receive(ctx2, query);

                if (rc > 0) 
                {
                    while(mb_mapping2 == NULL)
                    {
                        syslog(LOG_INFO, "mb_mapping2 is NULL!\n");
                        sleep(1);
                    }

                    modbus_reply(ctx2, query, rc, mb_mapping2);
                }
                else if (rc == -1) 
                {
                    /* This example server in ended on connection closing or
                     * any errors. */
                    syslog(LOG_INFO, "(Connection closed on socket %d", master_socket);
                    close(master_socket);

                    /* Remove from reference set */
                    FD_CLR(master_socket, &refset);

                    if (master_socket == fdmax) 
                    {
                        fdmax--;
                    }
                }
            }
        }
    }

    return 0;
}

int Sendmod_major()
{
    int rc;
    int i = 0;
    int master_socket;
    fd_set refset;
    fd_set rdset;
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    char  ip[256] = {0};
    /* Maximum file descriptor number */
    int fdmax;

    get_modbus_major_ip(ip);
    ctx1 = modbus_new_tcp(ip, 1502);
    if(ctx1 == NULL)
    {
        modbus_free(ctx1);
        ctx1 = NULL;
        return -1;
    }
    
    mb_mapping1 = modbus_mapping_new_start_address( 
            0, 0,
            0, 0,
            0, 0,
            UT_INPUT_REGISTERS_ADDRESS, UT_INPUT_REGISTERS_NB);
    if (mb_mapping1 == NULL) 
    {
        syslog(LOG_ERR, "Failed to allocate the mapping: %s\n", modbus_strerror(errno));
        return -1;
    }

    for(i = 0; i < UT_INPUT_REGISTERS_NB; i++)
    {
        mb_mapping1->tab_input_registers[i] = 0;
    }

    modbus_set_slave(ctx1, 2);
#ifdef BUG
    modbus_set_debug(ctx1, TRUE);
#else
    modbus_set_debug(ctx1, FALSE);
#endif
    server_socket1 = modbus_tcp_listen(ctx1, NB_CONNECTION);
    if (server_socket1 == -1) 
    {
        syslog(LOG_ERR, "Unable to listen TCP connection\n");
        modbus_free(ctx1);
        ctx1 = NULL;
        return -1;
    }

    /* Clear the reference set of socket */
    FD_ZERO(&refset);
    /* Add the server socket */
    FD_SET(server_socket1, &refset);

    /* Keep track of the max file descriptor */
    fdmax = server_socket1;

    for (;;) 
    {
        rdset = refset;
        if (select(fdmax+1, &rdset, NULL, NULL, NULL) == -1) 
        {
            syslog(LOG_ERR, "Server select() failure.");
            close_sigint();
        }

        /* Run through the existing connections looking for data to be read */
        for (master_socket = 0; master_socket <= fdmax; master_socket++) 
        {
            if (!FD_ISSET(master_socket, &rdset)) 
            {
                continue;
            }

            if (master_socket == server_socket1) 
            {
                /* A client is asking a new connection */
                socklen_t addrlen;
                struct sockaddr_in clientaddr;
                int newfd;

                /* Handle new connections */
                addrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, sizeof(clientaddr));
                newfd = accept(server_socket1, (struct sockaddr *)&clientaddr, &addrlen);
                if (newfd == -1) 
                {
                    syslog(LOG_ERR, "Server accept() error");
                }
                else 
                {
                    FD_SET(newfd, &refset);

                    if (newfd > fdmax) 
                    {
                        /* Keep track of the maximum */
                        fdmax = newfd;
                    }
                    syslog(LOG_INFO, "New connection from %s:%d on socket %d", inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
                }
            } 
            else 
            {
                modbus_set_socket(ctx1, master_socket);
                rc = modbus_receive(ctx1, query);

                if (rc > 0) 
                {
                    while(mb_mapping1 == NULL)
                    {
                        syslog(LOG_INFO, "mb_mapping1 is NULL!\n");
                        sleep(1);
                    }

                    modbus_reply(ctx1, query, rc, mb_mapping1);
                }
                else if (rc == -1) 
                {
                    /* This example server in ended on connection closing or
                     * any errors. */
                    syslog(LOG_INFO, "(Connection closed on socket %d", master_socket);
                    close(master_socket);

                    /* Remove from reference set */
                    FD_CLR(master_socket, &refset);

                    if (master_socket == fdmax) 
                    {
                        fdmax--;
                    }
                }
            }
        }
    }

    return 0;
}
