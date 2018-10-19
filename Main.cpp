#include <iostream>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <thread>
#include <list>
#include <unistd.h>
#include <iostream>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <syslog.h>
#include "Song.h"
#include "KeepAlive.h"
#include "RecvSnmp.h"
#include "Conf.h"
#include "SendMod.h"

#define MAXFILE 65535
#define SYSCONF "/usr/local/warning/etc/database.conf"
using namespace std;

void dameno()
{
    pid_t pid;
    int i;
    pid = fork();     //创建一个进程用来做守护进程
    if(pid<0)
    {
        syslog(LOG_ERR, "dameno error fork \n");
        exit(1);
    }
    else if(pid>0)
    {
        exit(0);    //结束父进程
    }

    setsid();       //使子进程独立1.摆脱原会话控制 2.摆脱原进程组的控制 3.摆脱控制中端的控制
    chdir("/");     //改变当前工作目录，这也是为了摆脱父进程的影响
    umask(0);      //重设文件权限掩码
    for(i=0;i<MAXFILE;i++)
    {
        close(i);
    }
}

int main(int argc, char **argv)
{
    //open syslog
    openlog("Waring", LOG_CONS | LOG_PID, LOG_USER);
    
    dameno();

    Init_lock();
    init_conf(SYSCONF);
    
    thread  producer0(KeepAlive);
    thread  producer1(RecvUdp);
    thread  producer2(RecvSnmp);
    thread  producer3(ConsumerTask);
    thread  producer4(UpdateSig);

    thread  producer5(Iscs);
    thread  producer6(Sendmod_major);
    thread  producer7(Sendmod_min);

    thread  producer8(Sig_light);
//    thread  producer9(Hop);
    thread  producer10(Song);
    
	producer0.join();
    producer1.join();
	producer2.join();
    producer3.join();
    producer4.join();
    producer5.join();
    producer6.join();
    producer7.join();
    producer8.join();
//	producer9.join();
	producer10.join();
   
    closelog();
	return 0;
}
