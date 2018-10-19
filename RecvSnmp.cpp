#include <libsnmp.h> 
#include <syslog.h>
#include <list>
#include <map>
#include <cstdlib>
#include "snmp_pp/snmp_pp.h"
#include "snmp_pp/collect.h"
#include "snmp_pp/notifyqueue.h"
#include "KeepAlive.h"

#include "g2u.h"
#define  SSYS      "1.3.6.1.4.1.3902.4101.1.3.1.12"
#define  ATIME     "1.3.6.1.4.1.3902.4101.1.3.1.3"
#define  ACODE     "1.3.6.1.4.1.3902.4101.1.3.1.11"
#define  BUS       "1.3.6.1.4.1.3902.4101.1.3.1.26"
#define  SLOT      "1.3.6.1.4.1.3902.4101.1.3.1.15"
#define  WARNING   "1.3.6.1.4.1.3902.4101.4.2.1.1"
#define  HEART     "1.3.6.1.4.1.3902.4101.4.1.2"
#define  RECOVER1   "1.3.6.1.4.1.3902.4101.1.4.1.2"
#define  RECOVER2   "1.3.6.1.4.1.3902.4101.1.4.1.3"
#define  ZXIP      "1.3.6.1.4.1.3902.4101.1.3.1.17"

#define  IPBUS     "/usr/local/warning/etc/ip_addr"
#define  ZXCODE     "/usr/local/warning/etc/zxcode"
#define  SNMP_JSON_STR   "{ type: \"65406\", fnum: \"0\", flen: \"26\", son_sys: \"%d\", stop: \"%d\", eng: \"%s\", node:\"%s\", bug: \"%d\", time: \"%s\", res1: \"0\", res2: \"0\", res3: \"0\", check: \"0\"}"

using namespace Snmp_pp;
using namespace std;

static  map<string, int>  ipmap;
static  map<long, int>    codemap;

#define  TIMEOUT  60
static  int  zte_gw_timeout   = 5;
static  int  zte_g_cs_timeout = 5;
static  int  zte_7_cs_timeout = 5;
static  string  zte_gw_ip   =  "10.71.20.129";
static  string  zte_g_cs_ip =  "10.71.14.11";
static  string  zte_7_cs_ip =  "10.71.14.12";

static void callback( int reason, Snmp *snmp, Pdu &pdu, SnmpTarget &target, void *cd)
{
    Vb nextVb;
    int pdu_error;
    GenAddress addr;

   
    target.get_address(addr);
    UdpAddress from(addr);

    from.get_printable();
    syslog(LOG_INFO, "reason: %d, msg: %s, from: %s\n", reason, snmp->error_msg(reason), addr.get_printable());

    int   son_sys = 0;
    int   bus = 0;
    char  jk[256] = {0};
    char  cw[256] = {0};
    int   acode = 0;
    long  acode1 = 0;
    char  atime[1024];
    int   codeflag = 0;
    char  heartip[1024];

    bzero(heartip, 1024);
    sscanf(addr.get_printable(), "%[^/]s%*s", heartip);
    
    pdu_error = pdu.get_error_status();
    if (pdu_error){
        syslog(LOG_INFO, "Response contains error: %s\n",  snmp->error_msg(pdu_error));
    }

    Oid id;
    pdu.get_notify_id(id);
    //syslog(LOG_INFO, "ID:  %s\n", id.get_printable());
    //syslog(LOG_INFO, "Type: %d\n", pdu.get_type());

    char oid[1024];
    char value[1024];

    bzero(oid, 1024);
    snprintf(oid, 1024, "%s", id.get_printable());
    
    if(memcmp(WARNING,  oid, strlen(WARNING)) == 0)
    {
        if(memcmp(heartip, zte_7_cs_ip.c_str(), strlen(zte_7_cs_ip.c_str())) == 0)
        {
            if(zte_7_cs_timeout == 0)
            {
                keep_snmp_json_str(1, 1, zte_7_cs_ip.c_str(), 162, 1);
            }
            zte_7_cs_timeout = TIMEOUT;
        }
        else if(memcmp(heartip, zte_gw_ip.c_str(), strlen(zte_gw_ip.c_str())) == 0)
        {
            if(zte_gw_timeout == 0)
            {
                keep_snmp_json_str(2, 1, zte_gw_ip.c_str(), 162, 1);
            }
            zte_gw_timeout = TIMEOUT;
        }
        else if(memcmp(heartip, zte_g_cs_ip.c_str(), strlen(zte_g_cs_ip.c_str())) == 0)
        {
            if(zte_g_cs_timeout == 0)
            {
                keep_snmp_json_str(12, 1, zte_g_cs_ip.c_str(), 162, 1);
            }
            zte_g_cs_timeout = TIMEOUT;
        }

    }
    
    if(memcmp(RECOVER1,  oid, strlen(RECOVER1)) == 0)
    {
        codeflag = 4096; 
    }
    else if(memcmp(WARNING, oid, strlen(WARNING)) == 0)
    {
        codeflag = 0;
    }

    if(memcmp(RECOVER2,  oid, strlen(RECOVER2)) == 0)
    {
    
    }
    else
    {
        for (int i=0; i<pdu.get_vb_count(); i++)
        {
            pdu.get_vb(nextVb, i);
            bzero(oid, 1024);
            bzero(value, 1024);
            snprintf(oid, 1024, "%s", nextVb.get_printable_oid());
            snprintf(value, 1024, "%s", nextVb.get_printable_value());

            if(strncmp(HEART, oid, strlen(HEART)) == 0)
            {
                break;
            }

            if(strncmp(SSYS, oid, strlen(SSYS)) == 0)
            {
                if(strstr(value, "ngn_ss") != NULL)
                {
                    son_sys = 401; 
                }
                else  if(strstr(value, "ngn_msg9000") != NULL)
                {
                    son_sys = 414; 
                }
                else  if(strstr(value, "ZXMP S385") != NULL)
                {
                    if(memcmp(heartip, zte_g_cs_ip.c_str(), strlen(zte_g_cs_ip.c_str())) == 0)
                    {
                        son_sys = 12; 
                    }
                    else  if(memcmp(heartip, zte_7_cs_ip.c_str(), strlen(zte_7_cs_ip.c_str())) == 0)
                    {
                        son_sys = 1; 
                    }
                }
                else  if(strstr(value, "MSAG5200V3") != NULL)
                {
                    son_sys = 425;
                }
                syslog(LOG_INFO, "son_sys = %d\n", son_sys);
            }
            else if(strncmp(ATIME, oid, strlen(ATIME)) == 0)
            {
                bzero(atime, 1024);
                int year, mon, day, hh, mm, ss;
                sscanf(value, "%04d-%02d-%02d, %02d:%02d:%02d*", &year, &mon, &day, &hh, &mm, &ss);

                snprintf(atime, 1024, "%04d-%02d-%02d %02d:%02d:%02d", year, mon, day, hh, mm, ss);
                syslog(LOG_INFO, "atime = %s\n", atime);
            }
            else if(memcmp(ACODE, oid, strlen(ACODE)) == 0)
            {
                acode1 = atol(value);
                syslog(LOG_INFO, "acode src %ld\n", acode1);
            }
            else if(memcmp(ZXIP, oid, strlen(ZXIP)) == 0)
            {
                map<string, int>:: iterator iter;
                iter = ipmap.find(string(value));
                bus = iter->second;
                if(son_sys == 1 || son_sys == 12)
                {
                    bzero(jk, 256);
                    snprintf(jk, 256, "%s", value); 
                }
                syslog(LOG_INFO, "bus = %d, iter->second = %d\n", bus, iter->second);
            }
            else if(memcmp(SLOT, oid, strlen(SLOT)) == 0)
            {
                char  *slot = (char*)malloc(1024);
                bzero(slot, 1024);
                get_value(value, 1024, &slot);
                if(son_sys == 401 || son_sys == 425 || son_sys == 414)
                {
                    bzero(jk, 256);
                    if(son_sys == 401)
                    {
                        snprintf(jk, 256, "1");
                    }
                    else if(son_sys == 414)
                    {
                        snprintf(jk, 256, "2");
                    }
                    else if(son_sys == 425)
                    {
                        snprintf(jk, 256, "3");
                    }
                    
                    int  jk1 = 0;
                    int  cw1 = 0;
                    sscanf(slot, "%*[^=]=%*[^=]=%d,%*[^=]=%d,%*s", &jk1, &cw1);
                    syslog(LOG_INFO, "1-slot = %s, cw = %s\nvalue = %s, len = %ld\n", slot, cw, value, strlen(value));
                    bzero(cw, 256);
                    snprintf(cw, 256, "%d-%d", jk1, cw1);
                }
                else if(son_sys == 12 || son_sys == 1)
                {
                    bzero(cw, 256);
                    if((strstr(slot, "(") == NULL))
                    {
                        sscanf(slot, "%*[^,],%[^\n^#]s", cw);
                    }
                    else
                    {
                        sscanf(slot, "%*[^,],%[^(^#]s", cw);
                    }
                    syslog(LOG_INFO, "2-slot = %s, cw = %s\nvalue = %s, len = %ld\n", slot, cw, value, strlen(value));

                }
                free(slot);
                slot = NULL;
            }
        }
        //
        if(son_sys == 401 || son_sys == 425 || son_sys == 414 || son_sys == 1 || son_sys == 12)
        { 
            char code[1024];
            if(son_sys == 401)
            {
                snprintf(code, 1024, "401%ld", acode1);
                son_sys = 2;
            }
            else if(son_sys == 414)
            {
                snprintf(code, 1024, "414%ld", acode1); 
                son_sys = 2;
            }
            else if(son_sys == 425)
            {
                snprintf(code, 1024, "425%ld", acode1); 
                son_sys = 2;
            }
            else
            {
                snprintf(code, 1024, "%ld", acode1);
            }

            map<long, int>::iterator  iter;
            long  lcode = atol(code);
            syslog(LOG_INFO, "lcode = %ld\n", lcode);
            iter = codemap.find(lcode);
            if(iter != codemap.end())
            {
                acode = iter->second - codeflag;
            }
            else
            {
                acode = 0;
            }
            syslog(LOG_INFO, "acode %d\n", acode);
        }

        if(acode != 0 && son_sys != 0)
        {
            char buf[1024];
            snprintf(buf, 1024, SNMP_JSON_STR, son_sys, bus, jk, cw, acode, atime);
            syslog(LOG_INFO, "snmp json %s\n", buf);
            add_snmp_json_str(buf);
        }
    }

    if (pdu.get_type() == sNMP_PDU_INFORM) 
    {
        syslog(LOG_INFO, "pdu type: %d\n", pdu.get_type());
        nextVb.set_value("This is the response.");
        pdu.set_vb(nextVb, 0);
        snmp->response(pdu, target);
    }
}

int map_zx_code()
{
    int   ret = 0;
    int   id = 0;
    long  zxcode;

    FILE *fp = fopen(ZXCODE, "r");
    if(fp == NULL)
    {
        syslog(LOG_ERR, "fopen %s error!\n", ZXCODE);
        fclose(fp);
        return -1;
    }

    while(1)
    {
        ret = fscanf(fp, "%ld  %d", &zxcode, &id);
        if(ret == -1)
        {
            fclose(fp);
            fp = NULL;
            break;
        }
        codemap.insert(pair<long, int>(zxcode, id));
    }

    return 0;
}

int  map_ip_bus()
{
    char  temp[1024];
    int   ret = 0;
    int   id = 0;

    FILE *fp = fopen(IPBUS, "r");
    if(fp == NULL)
    {
        syslog(LOG_ERR, "fopen %s error!\n", IPBUS);
        fclose(fp);
        return -1;
    }

    while(1)
    {
        bzero(temp, 1024);
        ret = fscanf(fp, "%s  %d", temp, &id);
        if(ret == -1)
        {
            break;
        }
        ipmap.insert(pair<string, int>(temp, id));
    }
    fclose(fp);
    fp = NULL;

    return 0;
}

int RecvSnmp()
{
    int ret = 0;
    ret = map_ip_bus();
    if(ret != 0)
    {
        syslog(LOG_INFO, "map_ip_bus_no error!\n");
        return -1;
    }

    ret = map_zx_code();
    if(ret != 0)
    {
        syslog(LOG_INFO, "map_zx_code error!\n");
        return -1;
    }
    int trap_port = 162; // no need to be root
    //----------[ create a SNMP++ session ]-----------------------------------
    int status; 
    Snmp::socket_startup();  // Initialize socket subsystem
    Snmp snmp(status);                // check construction status
    if ( status != SNMP_CLASS_SUCCESS)
    {
        syslog(LOG_INFO, "SNMP++ Session Create Fail, %s\n", snmp.error_msg(status));
        return 1;
    }

    OidCollection oidc;
    TargetCollection targetc;

    syslog(LOG_INFO, "Trying to register for traps on port %d.\n", trap_port);
    snmp.notify_set_listen_port(trap_port);
    status = snmp.notify_register(oidc, targetc, callback, NULL);
    if (status != SNMP_CLASS_SUCCESS)
    {
        syslog(LOG_INFO, "Error register for notify (%d): %s\n", status, snmp.error_msg(status));
        return 1;
    }
    else
    {
        syslog(LOG_INFO, "Waiting for traps/informs...\n");
    }

    snmp.start_poll_thread(10);
    while(1)
    {
        pause();
    }
    snmp.stop_poll_thread();
    Snmp::socket_cleanup();  // Shut down socket subsystem

    return 0;
}

void Update_timeout()
{
    if(zte_7_cs_timeout > 0)
    {
        zte_7_cs_timeout -- ;
        if(zte_7_cs_timeout == 0)
        {
            keep_snmp_json_str(1, 1, zte_7_cs_ip.c_str(), 162, 0);
        }
    }

    if(zte_gw_timeout > 0)
    {
        zte_gw_timeout --;
        if(zte_gw_timeout == 0)
        {
            keep_snmp_json_str(2, 1, zte_gw_ip.c_str(), 162, 0);
        }
    }

    if(zte_g_cs_timeout > 0)
    {
        zte_g_cs_timeout -- ;

        if(zte_g_cs_timeout == 0)
        {
            keep_snmp_json_str(12, 1, zte_g_cs_ip.c_str(), 162, 0);
        }
    }

}

#if  0
//timer work
int ZteAlive()
{
    struct itimerval tick;

    signal(SIGALRM,  Update_timeout);
    memset(&tick, 0, sizeof(tick));

    tick.it_value.tv_sec = 1;
    tick.it_value.tv_usec = 0;

    tick.it_interval.tv_sec = 1;
    tick.it_interval.tv_usec = 0;

    if(setitimer(ITIMER_REAL, &tick, NULL) < 0)
        syslog(LOG_ERR, "Set timer failed.");

    while(true)
    {
        pause();
    }
    return 0;
}
#endif
