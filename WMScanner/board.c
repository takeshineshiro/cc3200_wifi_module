//
//	CC3200�ĳ�ʼ������
//

#include "board.h"

#include "main.h"

//	drivers
#include "MotorControl.h"
#include "SampleControl.h"

// ----------------------------------
//	ȫ�ֺ���
// ----------------------------------
static void BoardInit(void);
static void PinMuxConfig(void);
void HardwareInit(void)
{
	BoardInit();
	PinMuxConfig();
	
#ifndef NOTERM
	InitTerm();
#endif
	
	MotorInit();
	
	InitSampleControl();
}


// ==================================
// ��̬����
// ==================================
extern uVectorEntry __vector_table;
static void BoardInit(void)
{
    MAP_IntVTableBaseSet((unsigned long)&__vector_table);
	//
	// Enable Processor
	//
	MAP_IntMasterEnable();
	MAP_IntEnable(FAULT_SYSTICK);
	PRCMCC3200MCUInit();
}

static void PinMuxConfig(void)
{
	//
	//	����/���ᰴť
	//	
	PRCMPeripheralClkEnable(PRCM_GPIOA0,PRCM_RUN_MODE_CLK);
	PinTypeGPIO(PIN_60,PIN_MODE_0,false);
	GPIODirModeSet(GPIOA0_BASE,0x20,GPIO_DIR_MODE_IN);
	
	//
	//	��Դ�����ź�, 
	//	
	//	ע��
	//		Ĭ��Ϊ���룬�رյ�Դʱ�����
	//					2015 - 2 - 15
	//
	PinTypeGPIO(PIN_59,PIN_MODE_0,false);
	//GPIODirModeSet(GPIOA0_BASE,0x10,GPIO_DIR_MODE_OUT);
	//GPIOPinWrite(GPIOA0_BASE,0x10,0x10);
	GPIODirModeSet(GPIOA0_BASE,0x10,GPIO_DIR_MODE_IN);
	//GPIOPinWrite(GPIOA0_BASE,0x10,0x10);
	

	//
	//	���״̬�ź�
	//
	PRCMPeripheralClkEnable(PRCM_GPIOA1,PRCM_RUN_MODE_CLK);
	PinTypeGPIO(PIN_64,PIN_MODE_0,false);
	GPIODirModeSet(GPIOA1_BASE,0x02,GPIO_DIR_MODE_IN);
	
	
	//
	//	����ָʾLED��
	//
	PRCMPeripheralClkEnable(PRCM_GPIOA2,PRCM_RUN_MODE_CLK);
	PinTypeGPIO(PIN_15,PIN_MODE_0,false);
	GPIODirModeSet(GPIOA2_BASE,0x40,GPIO_DIR_MODE_OUT);
	GPIOPinWrite(GPIOA2_BASE,0X40,0x00);
	
	
	//
	//	������� (ʹ����PWM�����
	//
	PRCMPeripheralClkEnable(PRCM_GPIOA0,PRCM_RUN_MODE_CLK);
	PinTypeGPIO(PIN_61,PIN_MODE_0,false);		//	GPIO-06	��Ϊ���ʹ������
	GPIODirModeSet(GPIOA0_BASE,0x40,GPIO_DIR_MODE_OUT);
	GPIOPinWrite(GPIOA0_BASE,0x40,0x00);
    MAP_PRCMPeripheralClkEnable(PRCM_TIMERA3, PRCM_RUN_MODE_CLK);
    //MAP_PinTypeTimer(PIN_01, PIN_MODE_3);	// Configure PIN_01 for TIMERPWM6 GT_PWM06
    MAP_PinTypeTimer(PIN_02, PIN_MODE_3);	// Configure PIN_02 for TIMERPWM7 GT_PWM07

	//
	//	�����ź�
	//
	//PinTypeGPIO(PIN_63,PIN_MODE_0,false);
	//GPIOIntTypeSet(GPIOA1_BASE,0x01,GPIO_FALLING_EDGE);
	
	//	��дѡ�����
	PRCMPeripheralClkEnable(PRCM_GPIOA0,PRCM_RUN_MODE_CLK);
	PinTypeGPIO(PIN_62,PIN_MODE_0,false);
	GPIODirModeSet(GPIOA0_BASE,0x80,GPIO_DIR_MODE_OUT);
	GPIOPinWrite(GPIOA0_BASE,0x80,0x00);
	
	
	//
	//	��ѹʹ������
	//
	//	GPIO-07 ��Ϊ��ѹʹ�ܽ�
	PinTypeGPIO(PIN_62,PIN_MODE_0,false);
	GPIODirModeSet(GPIOA0_BASE,0x80,GPIO_DIR_MODE_OUT);
	GPIOPinWrite(GPIOA0_BASE,0x80,0x00);
	
	/*
	//
	//	E0��������
	//
	//	LED1	GPIO9	������E0�ţ�ֻ�ò���
	PRCMPeripheralClkEnable(PRCM_GPIOA1,PRCM_RUN_MODE_CLK);
	PinTypeGPIO(PIN_64,PIN_MODE_0,false);
	GPIODirModeSet(GPIOA1_BASE,0x02,GPIO_DIR_MODE_OUT);	
	GPIOPinWrite(GPIOA1_BASE,0x02,0x00);	//	��ʼ��Ϊ��
	*/
	
	//
    // SPI��AD��DA���� 
    //
    MAP_PRCMPeripheralClkEnable(PRCM_GSPI, PRCM_RUN_MODE_CLK);
    // Configure PIN_05 for SPI0 GSPI_CLK
    MAP_PinTypeSPI(PIN_05, PIN_MODE_7);
    // Configure PIN_06 for SPI0 GSPI_MISO
    MAP_PinTypeSPI(PIN_06, PIN_MODE_7);
    // Configure PIN_07 for SPI0 GSPI_MOSI
    MAP_PinTypeSPI(PIN_07, PIN_MODE_7);
    // Configure PIN_08 for SPI0 GSPI_CS
    MAP_PinTypeSPI(PIN_08, PIN_MODE_7);
	
	//
	//	Ƭ�������洢��
	//
	MAP_PRCMPeripheralClkEnable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);
    // Configure PIN_01 for I2C0 I2C_SCL
    MAP_PinTypeI2C(PIN_01, PIN_MODE_1);
    // Configure PIN_02 for I2C0 I2C_SDA
    MAP_PinTypeI2C(PIN_02, PIN_MODE_1);

	//
	//	�����õĴ����ն˽ӿ�
	//
#ifndef NOTERM
	MAP_PRCMPeripheralClkEnable(PRCM_UARTA0,PRCM_RUN_MODE_CLK);
	MAP_PinTypeUART(PIN_55,PIN_MODE_3);
	MAP_PinTypeUART(PIN_57,PIN_MODE_3);
#endif
}
