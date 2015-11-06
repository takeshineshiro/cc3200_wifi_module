#include "main.h"

#include "ProbeConf.h"

#include "i2c_if.h"

//---------------------------------------
//
//	ȫ�ֱ���
//

//	̽ͷ���к�
unsigned char  g_ucChannel;

//	�����������
unsigned char g_ucSalesCode;

//	̽ͷ���к�
char g_szSN[16];

//	Gain
unsigned char g_ucGain;

//	Zoom
unsigned char g_ucZoom;

//	TGC����
unsigned short g_usTGC[512];

//
//	E0�ķ����ӳ٣����ڿ��ƶ�ʱ��
//	���еĵ�һ����Ϊͬ��ͷ
//
unsigned long g_ulE0Delay[128];

//---------------------------------------
//
//	ȫ�ֺ���
//

//	��ʼ��̽ͷ���ò���
static unsigned char ReadReg(unsigned short usReg);
static void WriteReg(unsigned short usReg, unsigned char ucData);
void InitProbeConf(void)
{
	//	��ʼ��I2C
	I2C_IF_Open(I2C_MASTER_MODE_STD);
	
	//	Ĭ�ϵ�̽ͷ���к�
	memset(g_szSN,0,sizeof(g_szSN));
	char szSN[16]={"UKNOWN"};
	memcpy(g_szSN,szSN,strlen(szSN));
	
	//	Ĭ�ϵ������Zoom
	g_ucGain = 100;
	g_ucZoom = 0;
	
	//	��ʼ��TGC����
	for (unsigned int i=0;i<512;i++) {
		//g_usTGC[i] = (256+(i/4))<<5;	// TGC: 0-127
		g_usTGC[i] = (127+(i/2))<<5;	// TGC: 0-127
	}
	g_usTGC[511] = g_usTGC[0];
	
	//	Ĭ�ϵ�E0�ӳ�
	g_ulE0Delay[0] = 3700000;
	for (int i=1;i<128;i++)
		g_ulE0Delay[i] = 50000;//Delay[i];
	
	//	Ĭ�ϵ������ŵ�
	g_ucChannel = 6;
	
	//	 �����������
	g_ucSalesCode = SALES_CODE_ALL;
	
	//
	//	������洢���ж������ֲ���
	//
	LoadFromFRam(true,true,true,true,true);
}

void LoadFromFRam(bool bGainZoom,bool bSN,bool bE0Delay,bool bChannel,bool bSalesCode)
{
	if (bSN) {
		//	̽ͷ���к���Ч�Ա�־
		if ( ReadReg(0x10)==0x5A && ReadReg(0x11)==0xA5 ) {
			for (unsigned short i=0;i<16;i++) {
				g_szSN[i] = ReadReg(0x0100 + i); 
			}
		}
	}
	
	if (bGainZoom) {
		//	��ȡĬ������
		if ( ReadReg(0x20)==0x5A && ReadReg(0x21)==0xA5 ) {
			g_ucGain = ReadReg(0x0120);
		}
		
		//	��ȡĬ��Zoom
		if ( ReadReg(0x30)==0x5A && ReadReg(0x31)==0xA5 ) {
			g_ucZoom = ReadReg(0x0130);
		}
	}
	
	/*
	if (bE0Delay) {
		//	��ȡͬ��ͷ��E0�ӳ�
		if ( ReadReg(0x40)==0x5A && ReadReg(0x41)==0xA5 ) {
			unsigned char* pE0 = (unsigned char*)g_ulE0Delay;
			for (unsigned short i=0;i<128*sizeof(unsigned long);i++) {
				pE0[i] = ReadReg(0x0400+i);
			}
		}
	}
	*/
	
	if (bChannel) {
		//	��ȡĬ������
		unsigned char ucChannel;
		if ( ReadReg(0x50)==0x5A && ReadReg(0x51)==0xA5 ) {
			ucChannel = ReadReg(0x0150);
		}
		if (ucChannel>=1 && ucChannel<=13) {
			g_ucChannel = ucChannel;
		}
	}
	
	if (bSalesCode) {
		//	��ȡ�����������
		unsigned char ucSalesCode;
		if ( ReadReg(0x60)==0x5A && ReadReg(0x61)==0xA5 ) {
			ucSalesCode = ReadReg(0x0160);
		}
		g_ucSalesCode = ucSalesCode;
	}
}

void StoreToFRam(bool bGainZoom,bool bSN,bool bE0Delay, bool bChannel, bool bSalesCode)
{
	//	�������к�
	if (bSN) {
		WriteReg(0x10,0x5A);	WriteReg(0x11,0xA5);
		for (unsigned short i=0;i<16;i++) {
			WriteReg(0x0100+i,g_szSN[i]);
		}
	}
	
	if (bGainZoom) {
		//	��������/ZOOM
		WriteReg(0x0020,0x5A);	WriteReg(0x0021,0xA5);
		WriteReg(0x0120,g_ucGain);

		//	����Zoom
		WriteReg(0x0030,0x5A);	WriteReg(0x0031,0xA5);
		WriteReg(0x0130,g_ucZoom);
	}
	/*
	if (bE0Delay) {
		WriteReg(0x0040,0x5A);	WriteReg(0x0041,0xA5);
		unsigned char* pE0 = (unsigned char*)g_ulE0Delay;
		for (unsigned short i=0;i<128*sizeof(unsigned long);i++) {
			WriteReg(0x0400+i,pE0[i]);
		}
	}
	*/
	
	if (bChannel) {
		//	���������ŵ����
		WriteReg(0x0050,0x5A);	WriteReg(0x0051,0xA5);
		WriteReg(0x150,g_ucChannel);
	}
	
	if (bSalesCode) {
		//	���������������
		WriteReg(0x0060,0x5A);	WriteReg(0x0061,0xA5);
		WriteReg(0x160,g_ucSalesCode);
	}
}


static unsigned char ReadReg(unsigned short usReg)
{
	usReg &= 0x7FF;
	
	unsigned char ucDevAdr, ucRegAdr;
	ucDevAdr = ((usReg & 0x0700)>>8) | (0xA0 >> 1);
	ucRegAdr = (usReg & 0x00FF);
	
	I2C_IF_Write(ucDevAdr,&ucRegAdr,1,0);
    
	unsigned char ucRet;
	I2C_IF_Read(ucDevAdr, &ucRet, 1);
	
	return ucRet;
}

static void WriteReg(unsigned short usReg, unsigned char ucData)
{
	usReg &= 0x7FF;
	
	unsigned char ucDevAdr, ucRegAdr;
	ucDevAdr = ((usReg & 0x0700)>>8) | (0xA0 >> 1);
	ucRegAdr = (usReg & 0x00FF);
	
	unsigned char ucSnd[2] = {ucRegAdr, ucData};
	
	I2C_IF_Write(ucDevAdr,ucSnd,2,1);
}

