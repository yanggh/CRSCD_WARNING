#ifndef _KEEPALIVE_H
#define _KEEPALIVE_H
int add_json_str(const char*  str);
int add_snmp_json_str(const char*  str);
int keep_snmp_json_str(const int  son_sys, const int stop, const char* ip, const int port, const int flag);
int keep_json_str(const int  son_sys, const int stop, const char* ip, const int port, const int flag);
int RecvUdp();
int KeepAlive();
int UpdateSig();
int Sendmod();
int Sendmod_major();
int Sendmod_min();
int ConsumerTask(void);
int Iscs(void);
int Init_lock();
#endif
