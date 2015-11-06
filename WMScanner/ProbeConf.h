
//---------------------------------------
//
//	全局变量
//

//	无线信道
extern unsigned char g_ucChannel;

//	销售国家掩码
#define SALES_CODE_CHINA		0x01
#define SALES_CODE_ALL			0x7F
extern unsigned char g_ucSalesCode;


//	探头序列号
extern char g_szSN[16];

//	Gain
extern unsigned char g_ucGain;

//	Zoom
extern unsigned char g_ucZoom;


//	TGC曲线
extern unsigned short g_usTGC[512];


//	E0的发射延迟，用于控制定时器
//	其中的第一个数为同步头
//
extern unsigned long g_ulE0Delay[128];

//
//	初始化探头配置参数
//
extern void InitProbeConf(void);

extern void LoadFromFRam(bool bGainZoom,bool bSN,bool bE0Delay,bool bChannel,bool bSalesCode);

extern void StoreToFRam(bool bGainZoom,bool bSN,bool bE0Delay,bool bChannel,bool bSalesCode);

