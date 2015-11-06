



#define QUEUE_LENGTH		(128+0)
#define ITEM_LENGTH			514

extern void InitQueue();

extern bool LineSend(unsigned char ucFrame,unsigned char ucLine,unsigned char* pLine, unsigned long ulLength);
extern bool ISR_LineSend(unsigned char ucFrame,unsigned char ucLine,unsigned char* pLine, unsigned long ulLength);
extern bool ISR_LineSend_16(unsigned char ucFrame,unsigned char ucLine,unsigned short* pLine, unsigned long ulLength);
extern bool LineRecv(unsigned char* pRecvBuf,unsigned long ulLength);
extern bool ISR_LineRecv(unsigned char* pRecvBuf,unsigned long ulLength);

extern bool BufferCapacity();
extern bool ISR_BufferCapacity();