
//---------------------------------------
//
//	ȫ�ֱ���
//

//	�����ŵ�
extern unsigned char g_ucChannel;

//	���۹�������
#define SALES_CODE_CHINA		0x01
#define SALES_CODE_ALL			0x7F
extern unsigned char g_ucSalesCode;


//	̽ͷ���к�
extern char g_szSN[16];

//	Gain
extern unsigned char g_ucGain;

//	Zoom
extern unsigned char g_ucZoom;


//	TGC����
extern unsigned short g_usTGC[512];


//	E0�ķ����ӳ٣����ڿ��ƶ�ʱ��
//	���еĵ�һ����Ϊͬ��ͷ
//
extern unsigned long g_ulE0Delay[128];

//
//	��ʼ��̽ͷ���ò���
//
extern void InitProbeConf(void);

extern void LoadFromFRam(bool bGainZoom,bool bSN,bool bE0Delay,bool bChannel,bool bSalesCode);

extern void StoreToFRam(bool bGainZoom,bool bSN,bool bE0Delay,bool bChannel,bool bSalesCode);

