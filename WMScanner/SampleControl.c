#include "main.h"
#include "SampleControl.h"
#include "ProbeConf.h"
#include "QueueManager.h"
#include "MotorControl.h"


//	---------------------------------------
//	全局函数
//	---------------------------------------

//	采样控制的初始化操作
static void EmitCtrlInit(void);
static void SampleInit(void);
void InitSampleControl(void)
{
	EmitCtrlInit();
	
	//UDMAInit();
	SampleInit();
}

//	霍尔中断响应程序
static int s_nSampleIndex=0;			//	读取线号的索引
static void EmitStart(unsigned long);

//	======================================================================
//	静态函数
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
//		发射的控制
//
//	-------------------------------------------

//	发射定时器的中断响应函数
static unsigned long s_ulTimerEmitCtrl;	//	中断句柄
static unsigned char s_ucFrame = 0;		//	当前帧号
static unsigned char s_ucLine = 0;		//	当前线号
static bool	s_bRxDone=false;			//	接收是否完成
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

#define	FIX_E0DELAY		(70000)		//	最小 12 -- 45000
static void TimerEmitCtrlHandler(void)
{
	//	Clear interrupt
	unsigned long ulInts;
	ulInts = MAP_TimerIntStatus(s_ulTimerEmitCtrl,true);
	MAP_TimerIntClear(s_ulTimerEmitCtrl,ulInts);
	
	s_ucLine++;
	s_nSampleIndex++;
	
	//	调节定时器
	EmitStop();
	if (s_nSampleIndex < 129) {
		EmitStart(FIX_E0DELAY);
	}
	
	//	启动采样与命令发送
	bool bEnPower = g_bRunning;
	s_bRxDone = false;
	//TRACE("0x%x\r\n",s_ucLine);
	
	//	缓冲区 乒乓操作
	unsigned char* pTmp;
	pTmp = s_pucRxRcv;	s_pucRxRcv = s_pucRxSnd;	s_pucRxSnd = pTmp;
	
	//	开始采样
	if ( 	((s_ucFrame & 0x01) == 0x00) 
		&&	(s_ucLine>127)
			)
	{
		DMATransceive((s_ucFrame & 0x01),g_ucZoom,g_ucGain,127,s_pucTx,s_pucRxRcv);
	}
	else {
		DMATransceive((s_ucFrame & 0x01),g_ucZoom,g_ucGain,s_ucLine,s_pucTx,s_pucRxRcv);
	}
	//	发送数据到无线连接
	if (s_nSampleIndex >= 3) {
		ISR_LineSend(s_ucFrame,s_nSampleIndex-3,s_pucRxSnd,512);
	}
	
	/*
	//	开始采样第下一条线
	s_ucLine++;
	Command(true,g_ucZoom,g_ucGain,s_ucLine);
	
	//	开始采样下一条线，并将读取的线送入队列
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
//		采样的控制
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
			
			
			//	发送同步脉冲
			READ_FROM_SPI;
			WRITE_TO_SPI;
			READ_FROM_SPI;
			
			//	发送最后一根线
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
	//	发射	采集...
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

//	虚拟霍尔中断
void HallSyncHandler(void)
{
	//	Clear interrupt
	unsigned long ulInts;
	ulInts = MAP_TimerIntStatus(TIMERA2_BASE,true);
	MAP_TimerIntClear(TIMERA2_BASE,ulInts);
	
	
	//	送出控制命令
	if (ulInts & 0x01) {
		++g_nVirHallCnt;
		bool bCap = false;
		if (g_nVirHallCnt >= 10) {
			bCap = ISR_BufferCapacity();
		}

		//s_ucFrame++;	//	不论能不能缓冲均累计帧号，以此来区分丢帧
		if (bCap) {
			TRACE("VH\r\n");
			g_nVirHallCnt = 0;	// 复位计数器
			s_nSampleIndex = 0;
			s_ucFrame++;
			s_ucLine=0;
			s_pucTx = s_ucTxBuff;
	
			s_pucRxRcv = s_ucRxBuff_0;
			s_pucRxSnd = s_ucRxBuff_1;
			
			//	启动定时器
			TRACE("->\r\n");
			EmitStart(FIX_E0DELAY);
			//	启动DMA采样，同时发送命令
			s_bRxDone = false;
			
			DMATransceive( (s_ucFrame & 0x01) ,g_ucZoom,g_ucGain,s_ucLine,s_pucTx,s_pucRxRcv);
			
			/*
			EmitStart(85000);
			Command(true,g_ucZoom,g_ucGain,s_ucLine);	//	开始采样第一条线
			*/
		}
		else {
			//	注：前一次发射和采样还没有完成，无法开始新的一次采样
			if (g_nVirHallCnt == 10)
				EmitStop();
		}
	}
}

static void DMATransceive(	bool bPowerEn,
							unsigned char ucZoom,
							unsigned char ucGain,
							unsigned char ucLine,
							unsigned char* pSndBuf,	//	发送缓冲区
							unsigned char* pRcvBuf		//	接收缓冲区
							)
{
	//	准备发送的命令
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
	
	//	启动DMA
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




