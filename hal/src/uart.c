#define DEBUG_TAG  "UART"
#include <string.h>

#include "config.h"
#include "cfassert.h"
#include "uart.h"
#include "sctp.h"

#include "stm32f10x.h"
#include "stm32f10x_usart.h"

/*FreeRtos includes*/
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "nvicconf.h"
#include "log.h"

#define SCTP_START_BYTE_1  0xAA
#define SCTP_START_BYTE_2  0xBB
static const uint8_t START_PATTERN[] = {
            SCTP_START_BYTE_1, 
            SCTP_START_BYTE_2 };
//static xQueueHandle uartDataDelivery;
static bool isInit = false;
static xQueueHandle sctpPacketQueue;
static int SCTP_PACKET_QUEUE_LENGTH = 4;
static portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;

static xSemaphoreHandle waitUntilSendDone = NULL;

static SCTPLink uartLink;

static uint8_t outBuffer[MAX_DATA_SIZE];
static uint8_t dataIndex;
static uint8_t dataSize;
static uint8_t crcIndex = 0;

// 以下变量用于协议接收
#if CONFIG_BLE_TEST
#define TEST_RECV_MAX_BYTES  15000  // 15K
#define TEST_START_BYTE      0xEE
#define TEST_DATA_BYTE       0xCC
static RX_STATE_TYPE rxState = waitForTestStart;
static uint32_t recvCount = 0;
static uint32_t errCount = 0;
#else
static RX_STATE_TYPE rxState = waitForStart;
static SCTPPacket p;
static uint8_t crc;
static uint8_t pos = 0;
#endif

extern port_set ps;

#if CONFIG_BLE_TEST
static void parseSCTPPakcetData(uint8_t c)
{
    //LOG_DEBUG("%d", c);
    COM_UART_TYPE->DR = c;
    ++recvCount;
        if(c != 0xAA) 
        {
            errCount++;
        }
//        if(rxState == waitForTestStart)
//        {
//            if(c == TEST_START_BYTE)
//            {
//                rxState = waitForTestData;
//            }
//            else
//            {
//                errCount++;
//            }
//        }
//        else if(rxState ==  waitForTestData)
//        {
//            if(c != TEST_DATA_BYTE)
//            {
//                errCount++;
//            }
//        }
}
#else
static void parseSCTPPakcetData(uint8_t c)
{
    switch(rxState)
    {
    case waitForStart:
        if(c == START_PATTERN[pos])
        {
            if(++pos == sizeof(START_PATTERN))
            {
                rxState = waitForSeq;
                pos = 0;
                break;
            }
        }
        else
        {
            pos = 0;
        }
        break;
        
    case waitForSeq:
        p.seq = c;
        crc = c;
        rxState = waitForPort;
        break;
    
    case waitForPort:
        if(c > MAX_NUM_OF_PORTS || !PORT_ISSET(c, &ps))
        {
            rxState = waitForStart;
            break;
        }
        p.port = c;
        crc = (crc + c) % 0xFF;
        rxState = waitForSize;
        break;
        
    case waitForSize:
        if(c < SCPT_MAX_DATA_SIZE)
        {
            p.size = c;
            crc = (crc + c) % 0xFF;
            rxState = waitForData;
        }
        else
        {
            rxState = waitForStart;
        }
        break;
        
    case waitForData:
        p.data[pos++] = c;
        crc = (crc + c) % 0xFF;
        if(pos == p.size)
        {
            pos = 0;
            rxState = waitForCRC;
        }
        break;
        
    case waitForCRC:
        //if(crc == c)
        {
            xQueueSendFromISR(sctpPacketQueue, &p, 0);
        }
        rxState = waitForStart;
        break;
    
    default:
        //TODO: assert
        break;
    }
    
}
#endif

void uartISR(void)
{
    uint16_t c;
    
    if (USART_GetITStatus(COM_UART_TYPE, USART_IT_TXE))
    {
        USART_ClearITPendingBit(COM_UART_TYPE, USART_IT_TXE);
        if (dataIndex < dataSize)
        {
            USART_SendData(COM_UART_TYPE, outBuffer[dataIndex] & 0xFF);
            dataIndex++;
            if (dataIndex < dataSize - 1 && dataIndex >= sizeof(START_PATTERN))
            {
                outBuffer[crcIndex] = (outBuffer[crcIndex] + outBuffer[dataIndex]) % 0xFF;
            }
        }
        else
        {
            USART_ITConfig(COM_UART_TYPE, USART_IT_TXE, DISABLE);
            xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(waitUntilSendDone, &xHigherPriorityTaskWoken);
        }
    }
    
    if (USART_GetITStatus(COM_UART_TYPE, USART_IT_RXNE))
    {
        c = USART_ReceiveData(COM_UART_TYPE) & 0xFF;
        parseSCTPPakcetData(c);
    }
}

void uartSendBuff(uint32_t size, const uint8_t *buff)
{
    uint32_t i;

    for(i = 0; i < size; i++)
    {
        while (!(COM_UART_TYPE->SR & USART_FLAG_TXE));
        COM_UART_TYPE->DR = (buff[i] & 0xFF);
    }
}

int uartPutchar(int ch)
{
    uartSendBuff(1, (uint8_t *)&ch);
    
    return (unsigned char)ch;
}

static int uartSetEnable(bool enable)
{
    return 0;
}

static int uartSendSCTPPacket(SCTPPacket *p)
{
    int pos = 0;
    
    dataSize = sizeof(START_PATTERN) + 1 + 1 + 1 + p->size + 1; // start bytes + seq + port + size + data size + crc
    
    //LOG_FATAL("size = %d\n", dataSize);
    
    ASSERT(dataSize < sizeof(outBuffer));
    
    memcpy(outBuffer, START_PATTERN, sizeof(START_PATTERN));
    pos = sizeof(START_PATTERN);
    outBuffer[pos++] = p->seq;
    outBuffer[pos++] = p->port;
    outBuffer[pos++] = p->size;
    memcpy(&outBuffer[pos], p->data, p->size);
    
    dataIndex = 1;
    crcIndex = dataSize - 1;
    outBuffer[crcIndex] = 0;
    
    USART_SendData(COM_UART_TYPE, outBuffer[0] & 0xFF);
    USART_ITConfig(COM_UART_TYPE, USART_IT_TXE, ENABLE);
    xSemaphoreTake(waitUntilSendDone, portMAX_DELAY);
    p->crc = outBuffer[crcIndex];
    
    #ifdef SCTP_DEBUG_BUFF
    sctpPrint(p);
    #endif
    
    return 0;
}

static int uartRecvSCTPPacket(SCTPPacket *p)
{
    if (xQueueReceive(sctpPacketQueue, p, portMAX_DELAY) == pdTRUE)
    {
        return 0;
    }

    return -1;
}


SCTPLink *uartGetLink()
{
    return &uartLink;
}

void uartInit(void)
{
    if(isInit)
    {
        return;
    }
    
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(COM_GPIO_PERIF, ENABLE);
    RCC_APB1PeriphClockCmd(COM_UART_PERIF, ENABLE);	

    GPIO_InitStructure.GPIO_Pin = COM_UART_GPIO_TX;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(COM_UART_GPIO, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = COM_UART_GPIO_RX; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(COM_UART_GPIO, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate  = COM_UART_BAUD ;
    USART_InitStructure.USART_WordLength  = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits  = USART_StopBits_1;
    USART_InitStructure.USART_Parity  = USART_Parity_No ;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode  = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(COM_UART_TYPE, &USART_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = COM_UART_IRQ;			     //配置中断源
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = NVIC_PB_UART_PRI; 	//设置占先优先级为
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;		  	    //设置副优先级为0
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;			  	//使能串口1中断
    NVIC_Init(&NVIC_InitStructure);                                         //根据参数初始化中断寄存器

    USART_ITConfig(COM_UART_TYPE, USART_IT_RXNE, ENABLE);
    USART_Cmd(COM_UART_TYPE, ENABLE);

    uartLink.setEnable  = uartSetEnable;
    uartLink.sendPacket = uartSendSCTPPacket;
    uartLink.recvPacket = uartRecvSCTPPacket;

    sctpPacketQueue = xQueueCreate(SCTP_PACKET_QUEUE_LENGTH, sizeof(SCTPPacket)); // TODO: 2 hardcode

    vSemaphoreCreateBinary(waitUntilSendDone);
    xSemaphoreTake(waitUntilSendDone, 0);
}

bool uartTest()
{
    return isInit;
}
