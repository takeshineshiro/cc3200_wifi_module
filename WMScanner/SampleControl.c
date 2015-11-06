#include "main.h"
#include "SampleControl.h"
#include "ProbeConf.h"
#include "QueueManager.h"
#include "MotorControl.h"


//	---------------------------------------
//	ȫ�ֺ���
//	---------------------------------------

//	�������Ƶĳ�ʼ������
static void EmitCtrlInit(void);
static void SampleInit(void);
void InitSampleControl(void)
{
	EmitCtrlInit();
	
	//UDMAInit();
	SampleInit();
}

//	�����ж���Ӧ����
static int s_nSampleIndex=0;			//	��ȡ�ߺŵ�����
static void EmitStart(unsigned long);

//	======================================================================
//	��̬����
//	======================================================================
static int s_nQueueIndex = 0;
#define SPI_IF_BIT_RATE  20000000
#define TR_BUFF_SIZE     512

//static unsigned char s_ucTxBuff[TR_BUFF_SIZE+1];
//static unsigned char s_ucRxBuff[TR_BUFF_SIZE+1];

static unsigned char s_ucTxBuff[TR_BUFF_SIZE+1];
static unsigned char s_ucRxBuff_0[TR_BUFF_SIZE+1];
static unsigned char s_ucRxBuff_1[TR_BUFF_SIZE+1];

static unsigned char* s_pucTx;
static unsigned char* s_pucRxRcv;
static unsigned char* s_pucRxSnd;

//	-------------------------------------------
//
//		����Ŀ���
//
//	-------------------------------------------

//	���䶨ʱ�����ж���Ӧ����
static unsigned long s_ulTimerEmitCtrl;	//	�жϾ��
static unsigned char s_ucFrame = 0;		//	��ǰ֡��
static unsigned char s_ucLine = 0;		//	��ǰ�ߺ�
static bool	s_bRxDone=false;			//	�����Ƿ����
static void EmitStop(void);
static void Emit(void);
static void SampleStart(unsigned short*);
unsigned char s_pTmpLine[512];
static void Command(bool,unsigned char,unsigned char,unsigned char);
#define WRITE_TO_SPI	{GPIOPinWrite(GPIOA0_BASE,0x80,0x80);}
#define READ_FROM_SPI	{GPIOPinWrite(GPIOA0_BASE,0x80,0x00);}
static void ReadSPI(unsigned char* pIn,unsigned char* pOut,unsigned long ulLen);
static void DMATransceive(	bool bPowerEn,unsigned char ucZoom,unsigned char ucGain,
							unsigned char ucLine,unsigned char* pSndBuf,unsigned char* pRcvBuf);

#define	FIX_E0DELAY		(70000)		//	��С 12 -- 45000
static void TimerEmitCtrlHandler(void)
{
	//	Clear interrupt
	unsigned long ulInts;
	ulInts = MAP_TimerIntStatus(s_ulTimerEmitCtrl,true);
	MAP_TimerIntClear(s_ulTimerEmitCtrl,ulInts);
	
	s_ucLine++;
	s_nSampleIndex++;
	
	//	���ڶ�ʱ��
	EmitStop();
	if (s_nSampleIndex < 129) {
		EmitStart(FIX_E0DELAY);
	}
	
	//	���������������
	bool bEnPower = g_bRunning;
	s_bRxDone = false;
	//TRACE("0x%x\r\n",s_ucLine);
	
	//	������ ƹ�Ҳ���
	unsigned char* pTmp;
	pTmp = s_pucRxRcv;	s_pucRxRcv = s_pucRxSnd;	s_pucRxSnd = pTmp;
	
	//	��ʼ����
	if ( 	((s_ucFrame & 0x01) == 0x00) 
		&&	(s_ucLine>127)
			)
	{
		DMATransceive((s_ucFrame & 0x01),g_ucZoom,g_ucGain,127,s_pucTx,s_pucRxRcv);
	}
	else {
		DMATransceive((s_ucFrame & 0x01),g_ucZoom,g_ucGain,s_ucLine,s_pucTx,s_pucRxRcv);
	}
	//	�������ݵ���������
	if (s_nSampleIndex >= 3) {
		ISR_LineSend(s_ucFrame,s_nSampleIndex-3,s_pucRxSnd,512);
	}
	
	/*
	//	��ʼ��������һ����
	s_ucLine++;
	Command(true,g_ucZoom,g_ucGain,s_ucLine);
	
	//	��ʼ������һ���ߣ�������ȡ�����������
	if (s_nSampleIndex < 128)
		EmitStart(85000);
	else
		EmitStop();
	ReadSPI((unsigned char*)s_ucRxBuff,(unsigned char*)s_ucTxBuff,512);
	ISR_LineSend(s_ucFrame,s_nSampleIndex,(unsigned char*)s_ucRxBuff,512);
	s_nSampleIndex++;
	*/
}

static void EmitCtrlInit(void)
{
	s_ulTimerEmitCtrl = TIMERA1_BASE;

	MAP_PRCMPeripheralClkEnable(PRCM_TIMERA1,PRCM_RUN_MODE_CLK);
	MAP_PRCMPeripheralReset(PRCM_TIMERA1);
	MAP_TimerConfigure(s_ulTimerEmitCtrl,TIMER_CFG_PERIODIC);
	MAP_TimerPrescaleSet(s_ulTimerEmitCtrl,TIMER_A,0);
	
	//MAP_TimerIntRegister(s_ulTimerEmitCtrl,TIMER_A,TimerEmitCtrlHandler);
	osi_InterruptRegister(INT_TIMERA1A, TimerEmitCtrlHandler, 32);
	
	MAP_TimerIntEnable(s_ulTimerEmitCtrl,TIMER_TIMA_TIMEOUT);
}

static void EmitStart(unsigned long nDelay)
{
	//	10Hz
	MAP_TimerLoadSet(s_ulTimerEmitCtrl,TIMER_A,nDelay);//g_ulE0Delay[s_nEmitIndex]);
	//	Enable GPT
	MAP_TimerEnable(s_ulTimerEmitCtrl,TIMER_A);
}

static void EmitStop(void)
{
	//	Disable GPT
	MAP_TimerDisable(s_ulTimerEmitCtrl,TIMER_A);
}

static void Emit(void)
{	
	unsigned int nDely=0;
	unsigned long ulReg;
	ulReg = GPIOA1_BASE + GPIO_O_GPIO_DATA + (0x02<<2);
	HWREG(ulReg) = 0x02;
	++nDely;	--nDely;
	++nDely;	--nDely;
	++nDely;	--nDely;
	++nDely;	--nDely;
	++nDely;	--nDely;
	HWREG(ulReg) = 0x00;
	
	//HWREG(ulPort + (GPIO_O_GPIO_DATA + (ucPins << 2))) = ucVal;
}


static void Command(bool bPowerEn,
					unsigned char ucZoom,
					unsigned char ucGain,
					unsigned char ucLine)
{	
	unsigned short usCommand = 0x0000;
	if (bPowerEn)
		usCommand |= 0x8000;
	usCommand |= (ucZoom & 0x03) << 6;
	//usCommand |= (ucGain & 0x3F);
	usCommand |= (ucGain & 0x7E) >> 1;
	TRACE("Gain: %d\r\n",ucGain);
	usCommand |= (ucLine & 0x7F) << 8;
	
	// Reset SPI
	MAP_SPIReset(GSPI_BASE);

	// Configure SPI interface
	MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
					20000000,SPI_MODE_MASTER,SPI_SUB_MODE_0,
					(SPI_SW_CTRL_CS |
					SPI_4PIN_MODE |
					SPI_TURBO_ON |
					SPI_CS_ACTIVEHIGH |
					SPI_WL_8));
	
	//	Enable SPI for communication
	MAP_SPIEnable(GSPI_BASE);

	WRITE_TO_SPI;
	unsigned char tmpbuf[2];
	MAP_SPITransfer(GSPI_BASE,
					(unsigned char*)&usCommand,
					tmpbuf,2,
					SPI_CS_ENABLE|SPI_CS_DISABLE);
	
	READ_FROM_SPI;
	
	MAP_SPIDisable(GSPI_BASE);
}

static void ReadSPI(unsigned char* pIn,unsigned char* pOut,unsigned long ulLen)
{	
	// Reset SPI
	MAP_SPIReset(GSPI_BASE);

	// Configure SPI interface
	MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
					20000000,SPI_MODE_MASTER,SPI_SUB_MODE_3,
					(SPI_HW_CTRL_CS |
					SPI_4PIN_MODE |
					SPI_TURBO_ON |
					SPI_CS_ACTIVEHIGH |
					SPI_WL_8));
	
	//	Enable SPI for communication
	MAP_SPIEnable(GSPI_BASE);

	READ_FROM_SPI;
	unsigned char tmpbuf[2];
	MAP_SPITransfer(GSPI_BASE,
					pOut,
					pIn,
					ulLen,
					SPI_CS_ENABLE|SPI_CS_DISABLE);
	
	MAP_SPIDisable(GSPI_BASE);
}
//	-------------------------------------------------
//
//		�����Ŀ���
//
//	-------------------------------------------------
static void DMAIntHandler(void)
{
	MAP_SPIIntClear(GSPI_BASE,SPI_INT_DMATX|SPI_INT_DMARX);
	//MAP_SPIDmaDisable(GSPI_BASE, SPI_RX_DMA | SPI_TX_DMA);
	
    unsigned long ulMode;
    ulMode = MAP_uDMAChannelModeGet(UDMA_CH30_GSPI_RX | UDMA_PRI_SELECT);
    if(ulMode == UDMA_MODE_STOP)
    {
		//MAP_SPIDmaDisable(GSPI_BASE, SPI_RX_DMA );
		if (!s_bRxDone) {
			s_bRxDone = true;
			
			
			//	����ͬ������
			READ_FROM_SPI;
			WRITE_TO_SPI;
			READ_FROM_SPI;
			
			//	�������һ����
			if (s_nSampleIndex >= 129) {
				ISR_LineSend(s_ucFrame,127,s_pucRxRcv,512);
			}
		}
    }
	
    ulMode = MAP_uDMAChannelModeGet(UDMA_CH31_GSPI_TX | UDMA_PRI_SELECT);
    if(ulMode == UDMA_MODE_STOP)
    {
		//MAP_SPIDmaDisable(GSPI_BASE, SPI_TX_DMA );
    }
}

void SampleInit(void)
{
	/*
	//UDMAInit();
	// Reset SPI
	//MAP_SPIReset(GSPI_BASE);

	MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_3,
                     (SPI_HW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_ON|
                     SPI_CS_ACTIVELOW |
                     SPI_WL_8)); 
	MAP_SPIEnable(GSPI_BASE);
	MAP_SPICSEnable(GSPI_BASE);

	MAP_SPIFIFOEnable(GSPI_BASE,SPI_RX_FIFO);
    MAP_SPIFIFOEnable(GSPI_BASE,SPI_TX_FIFO);   
    SPIFIFOLevelSet(GSPI_BASE,1,1);
	
	MAP_SPIIntEnable(GSPI_BASE,SPI_INT_DMARX|SPI_INT_DMATX);
	//MAP_SPIIntEnable(GSPI_BASE,SPI_INT_DMATX);
	
	//MAP_SPIIntRegister(GSPI_BASE,DMAIntHandler);
	osi_InterruptRegister(INT_GSPI, DMAIntHandler, 32);
	*/
	
	UDMAInit();
	
	MAP_SPIConfigSetExpClk(GSPI_BASE,MAP_PRCMPeripheralClockGet(PRCM_GSPI),
                     SPI_IF_BIT_RATE,SPI_MODE_MASTER,SPI_SUB_MODE_3,
                     (SPI_HW_CTRL_CS |
                     SPI_4PIN_MODE |
                     SPI_TURBO_ON|
                     SPI_CS_ACTIVEHIGH |
                     SPI_WL_8)); 
	MAP_SPIEnable(GSPI_BASE);
	MAP_SPICSEnable(GSPI_BASE);
   
	MAP_SPIFIFOEnable(GSPI_BASE,SPI_RX_FIFO);
    MAP_SPIFIFOEnable(GSPI_BASE,SPI_TX_FIFO);   
    SPIFIFOLevelSet(GSPI_BASE,1,1);
	
	MAP_SPIIntEnable(GSPI_BASE,SPI_INT_DMARX);
	//MAP_SPIIntEnable(GSPI_BASE,SPI_INT_DMATX);
	
	//MAP_SPIIntRegister(GSPI_BASE,DMAIntHandler);
	osi_InterruptRegister(INT_GSPI, DMAIntHandler, 32);
}

static void SampleStart(unsigned short* pusRxBuff)
{
	//	����	�ɼ�...
	//Emit();
/*
	SetupTransfer(UDMA_CH31_GSPI_TX ,
                  	  	  UDMA_MODE_BASIC,
                  	  	  512,
                  	  	  UDMA_SIZE_8,
                  	  	  UDMA_ARB_1,
                  	  	  (void *)s_usTxBuff,
                  	  	  UDMA_SRC_INC_8,
                  	  	  (void *)(GSPI_BASE+MCSPI_O_TX0),
                  	  	  UDMA_DST_INC_NONE);
  
    SetupTransfer(UDMA_CH30_GSPI_RX ,
                  	  	  UDMA_MODE_BASIC,
                  	  	  512,
                  	  	  UDMA_SIZE_8,
                  	  	  UDMA_ARB_1,
                  	  	  (void *)(GSPI_BASE+MCSPI_O_RX0),
                  	  	  UDMA_SRC_INC_NONE,
                  	  	  (void *)pusRxBuff,
                  	  	  UDMA_DST_INC_8); 
    
    
   //MAP_SPIDmaEnable(GSPI_BASE, SPI_TX_DMA);
   MAP_SPIDmaEnable(GSPI_BASE, SPI_RX_DMA | SPI_TX_DMA);
*/
}

//	��������ж�
void HallSyncHandler(void)
{
	//	Clear interrupt
	unsigned long ulInts;
	ulInts = MAP_TimerIntStatus(TIMERA2_BASE,true);
	MAP_TimerIntClear(TIMERA2_BASE,ulInts);
	
	
	//	�ͳ���������
	if (ulInts & 0x01) {
		++g_nVirHallCnt;
		bool bCap = false;
		if (g_nVirHallCnt >= 10) {
			bCap = ISR_BufferCapacity();
		}

		//s_ucFrame++;	//	�����ܲ��ܻ�����ۼ�֡�ţ��Դ������ֶ�֡
		if (bCap) {
			TRACE("VH\r\n");
			g_nVirHallCnt = 0;	// ��λ������
			s_nSampleIndex = 0;
			s_ucFrame++;
			s_ucLine=0;
			s_pucTx = s_ucTxBuff;
	
			s_pucRxRcv = s_ucRxBuff_0;
			s_pucRxSnd = s_ucRxBuff_1;
			
			//	������ʱ��
			TRACE("->\r\n");
			EmitStart(FIX_E0DELAY);
			//	����DMA������ͬʱ��������
			s_bRxDone = false;
			
			DMATransceive( (s_ucFrame & 0x01) ,g_ucZoom,g_ucGain,s_ucLine,s_pucTx,s_pucRxRcv);
			
			/*
			EmitStart(85000);
			Command(true,g_ucZoom,g_ucGain,s_ucLine);	//	��ʼ������һ����
			*/
		}
		else {
			//	ע��ǰһ�η���Ͳ�����û����ɣ��޷���ʼ�µ�һ�β���
			if (g_nVirHallCnt == 10)
				EmitStop();
		}
	}
}

static void DMATransceive(	bool bPowerEn,
							unsigned char ucZoom,
							unsigned char ucGain,
							unsigned char ucLine,
							unsigned char* pSndBuf,	//	���ͻ�����
							unsigned char* pRcvBuf		//	���ջ�����
							)
{
	//	׼�����͵�����
	unsigned short usCommand = 0x0000;
	if (bPowerEn)
		usCommand |= 0x8000;
	usCommand |= (ucZoom & 0x03) << 6;
	//usCommand |= (ucGain & 0x3F);
	usCommand |= (ucGain & 0x7E) >> 1;
	usCommand |= (ucLine & 0x7F) << 8;
	//usCommand = 0x3F80;
	unsigned char ucL = (usCommand & 0x00FF);
	unsigned char ucH = ((usCommand & 0xFF00) >> 8);
	for (int i=0;i<20;i++) {
		pSndBuf[i*2] = ucL;
		pSndBuf[i*2+1] = ucH;
	}
	//TRACE("0x%x -- 0x%x\r\n",usCommand,ucLine);
	
	//	����DMA
	SetupTransfer(UDMA_CH31_GSPI_TX ,
                  	  	  UDMA_MODE_BASIC,
                  	  	  512,
                  	  	  UDMA_SIZE_8,
                  	  	  UDMA_ARB_1,
                  	  	  (void *)pSndBuf,
                  	  	  UDMA_SRC_INC_8,
                  	  	  (void *)(GSPI_BASE+MCSPI_O_TX0),
                  	  	  UDMA_DST_INC_NONE);
  
    SetupTransfer(UDMA_CH30_GSPI_RX ,
                  	  	  UDMA_MODE_BASIC,
                  	  	  512,
                  	  	  UDMA_SIZE_8,
                  	  	  UDMA_ARB_1,
                  	  	  (void *)(GSPI_BASE+MCSPI_O_RX0),
                  	  	  UDMA_SRC_INC_NONE,
                  	  	  (void *)pRcvBuf,
                  	  	  UDMA_DST_INC_8); 
	
   MAP_SPIDmaEnable(GSPI_BASE, SPI_RX_DMA | SPI_TX_DMA);
}




