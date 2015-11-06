//
//	CC3200的初始化工作
//

#include "board.h"

#include "main.h"

//	drivers
#include "MotorControl.h"
#include "SampleControl.h"

// ----------------------------------
//	全局函数
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
// 静态函数
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
	//	运行/冻结按钮
	//	
	PRCMPeripheralClkEnable(PRCM_GPIOA0,PRCM_RUN_MODE_CLK);
	PinTypeGPIO(PIN_60,PIN_MODE_0,false);
	GPIODirModeSet(GPIOA0_BASE,0x20,GPIO_DIR_MODE_IN);
	
	//
	//	电源开关信号, 
	//	
	//	注：
	//		默认为输入，关闭电源时输出低
	//					2015 - 2 - 15
	//
	PinTypeGPIO(PIN_59,PIN_MODE_0,false);
	//GPIODirModeSet(GPIOA0_BASE,0x10,GPIO_DIR_MODE_OUT);
	//GPIOPinWrite(GPIOA0_BASE,0x10,0x10);
	GPIODirModeSet(GPIOA0_BASE,0x10,GPIO_DIR_MODE_IN);
	//GPIOPinWrite(GPIOA0_BASE,0x10,0x10);
	

	//
	//	充电状态信号
	//
	PRCMPeripheralClkEnable(PRCM_GPIOA1,PRCM_RUN_MODE_CLK);
	PinTypeGPIO(PIN_64,PIN_MODE_0,false);
	GPIODirModeSet(GPIOA1_BASE,0x02,GPIO_DIR_MODE_IN);
	
	
	//
	//	连接指示LED灯
	//
	PRCMPeripheralClkEnable(PRCM_GPIOA2,PRCM_RUN_MODE_CLK);
	PinTypeGPIO(PIN_15,PIN_MODE_0,false);
	GPIODirModeSet(GPIOA2_BASE,0x40,GPIO_DIR_MODE_OUT);
	GPIOPinWrite(GPIOA2_BASE,0X40,0x00);
	
	
	//
	//	电机控制 (使能与PWM输出）
	//
	PRCMPeripheralClkEnable(PRCM_GPIOA0,PRCM_RUN_MODE_CLK);
	PinTypeGPIO(PIN_61,PIN_MODE_0,false);		//	GPIO-06	作为电机使能引脚
	GPIODirModeSet(GPIOA0_BASE,0x40,GPIO_DIR_MODE_OUT);
	GPIOPinWrite(GPIOA0_BASE,0x40,0x00);
    MAP_PRCMPeripheralClkEnable(PRCM_TIMERA3, PRCM_RUN_MODE_CLK);
    //MAP_PinTypeTimer(PIN_01, PIN_MODE_3);	// Configure PIN_01 for TIMERPWM6 GT_PWM06
    MAP_PinTypeTimer(PIN_02, PIN_MODE_3);	// Configure PIN_02 for TIMERPWM7 GT_PWM07

	//
	//	霍尔信号
	//
	//PinTypeGPIO(PIN_63,PIN_MODE_0,false);
	//GPIOIntTypeSet(GPIOA1_BASE,0x01,GPIO_FALLING_EDGE);
	
	//	读写选择操作
	PRCMPeripheralClkEnable(PRCM_GPIOA0,PRCM_RUN_MODE_CLK);
	PinTypeGPIO(PIN_62,PIN_MODE_0,false);
	GPIODirModeSet(GPIOA0_BASE,0x80,GPIO_DIR_MODE_OUT);
	GPIOPinWrite(GPIOA0_BASE,0x80,0x00);
	
	
	//
	//	高压使能引脚
	//
	//	GPIO-07 作为高压使能脚
	PinTypeGPIO(PIN_62,PIN_MODE_0,false);
	GPIODirModeSet(GPIOA0_BASE,0x80,GPIO_DIR_MODE_OUT);
	GPIOPinWrite(GPIOA0_BASE,0x80,0x00);
	
	/*
	//
	//	E0发射引脚
	//
	//	LED1	GPIO9	正好是E0脚，只好不用
	PRCMPeripheralClkEnable(PRCM_GPIOA1,PRCM_RUN_MODE_CLK);
	PinTypeGPIO(PIN_64,PIN_MODE_0,false);
	GPIODirModeSet(GPIOA1_BASE,0x02,GPIO_DIR_MODE_OUT);	
	GPIOPinWrite(GPIOA1_BASE,0x02,0x00);	//	初始化为低
	*/
	
	//
    // SPI的AD、DA引脚 
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
	//	片外的铁电存储器
	//
	MAP_PRCMPeripheralClkEnable(PRCM_I2CA0, PRCM_RUN_MODE_CLK);
    // Configure PIN_01 for I2C0 I2C_SCL
    MAP_PinTypeI2C(PIN_01, PIN_MODE_1);
    // Configure PIN_02 for I2C0 I2C_SDA
    MAP_PinTypeI2C(PIN_02, PIN_MODE_1);

	//
	//	调试用的串行终端接口
	//
#ifndef NOTERM
	MAP_PRCMPeripheralClkEnable(PRCM_UARTA0,PRCM_RUN_MODE_CLK);
	MAP_PinTypeUART(PIN_55,PIN_MODE_3);
	MAP_PinTypeUART(PIN_57,PIN_MODE_3);
#endif
}
