

//	连接状态参数
extern unsigned char  g_ulStatus;
extern unsigned long  g_ulStaIp;
extern unsigned long  g_ulPingPacketsRecv;
extern unsigned long  g_uiGatewayIP;

//	保护Flash免受SimpleLink操作，在SimpleLink操作过程中禁止使能扫描
extern bool g_bRunable;	

//	 Wifi连接线程
void WifiConnectionThread( void *pvParameters );
