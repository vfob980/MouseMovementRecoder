
#include "MouseRunner.h"//关联头文件

//本地全局变量
uint8_t             OrderCode;     //指令缓存区
uint8_t             OrderLen;      //指令长度缓冲区
uint8_t             OrderBuffer[4];//数据缓存区
uint8_t             SendBuffer [4]; //数据缓存区

//数据队列与互斥锁
extern QueueHandle_t      DataQueue;
extern EventGroupHandle_t DeviceState;
extern SemaphoreHandle_t  SerialPortMutex;

//测量计量
extern uint8_t            RoundNumber;
extern uint32_t           SampNumber;

//与前端通讯
extern QueueHandle_t      MesQueue_BTF;

//状态定时器
TimerHandle_t             PC_stateTimer;
uint32_t                  LastActTime = 0;


//方法
void  PC_StateTimerISR(TimerHandle_t xTimer);
void  OrderTask(void *parameter);
void  DataTransTask(void *parameter);
void  Order_Process();
void  Send(uint8_t Order,uint8_t Len);
void  Byte_Process(uint8_t Recive_Bytes);
void  StateReport();
void TimeReport(uint32_t Number);

/*
 * 帧头     0x55 0xAA 0x55
 * 事务码   0x--
 * 数据长度 0x--
 * 数据段   0x00  
 */

/*蓝牙接收指令字典
 * 1.握手要求 0xA0  
 * 2.控制     0xA1  
 */

/*反馈指令字典 
 * 1.应答     0xC0
 * 2.计时     0xC1
 * 3.数据包   0xB0
 */

void  PC_StateTimerISR(TimerHandle_t xTimer)
{
    if((xTaskGetTickCount() - LastActTime)>1100)
    {
        if(EVENT_GET(DeviceState)&AVA_PC)
        {
            if(EVENT_GET(DeviceState)&STA_REC)
                REPORT(TRUE,MesQueue_BTF,ERROR,PC_LOSS_CONNECT);
            EVENT_CLR(DeviceState,AVA_PC);
            REPORT(TRUE,MesQueue_BTF,PC_ACT,FALSE);
        }
    }
}

//控制流
void  OrderTask(void *parameter)
{
   PC_stateTimer = xTimerCreate("PC_STATE",250,pdTRUE,0,PC_StateTimerISR);
   xTimerStart(PC_stateTimer,portMAX_DELAY);
   uint8_t  Byte = 0x00;
   while(true)
   {
       while(Serial.available()>0)
       {
            Byte = (uint8_t)Serial.read();
            Byte_Process(Byte);
       }
       vTaskDelay(50);
   }
}

void  DataTransTask(void *parameter)
{
    struct  DataItem DataPacket;
    uint8_t i=0;
    uint8_t j=0;
    uint8_t TailL = 0;
    uint8_t PacketNum = 0;
    uint32_t Timetamp = 0;
    while (1)
    {
        //xQueueReceive(DataQueue,(void*)&DataPacket,portMAX_DELAY);
        QUEUE_WAIT(DataQueue,DataPacket);
        //Serial.printf("Data|Send\n");
        TailL     = (DataPacket.length*4)%(PACKET_LEN);
        PacketNum = (DataPacket.length*4)/(PACKET_LEN);
        if(TailL!=0)PacketNum += 1;
        //Timetamp  = (uint32_t)(xTaskGetTickCount()/1000);
        for(i=0;i<PacketNum;i++)
        {
            if (i == PacketNum-1 && TailL!=0 )
            {
                Serial.printf("Data|Send2\n");
                xSemaphoreTake(SerialPortMutex,portMAX_DELAY);
                Serial.write(0x55);
                Serial.write(0xAA);
                Serial.write(0x55);
                Serial.write(0xB0);
                Serial.write((uint8_t)(TailL+3));
                Serial.write(i);
                Serial.write(PacketNum);
                Serial.write((uint8_t)(RoundNumber));
                for(j=0;j<TailL;j++)
                {
                    Serial.write(DataPacket.Data[(i*PACKET_LEN)+j]);
                }
                xSemaphoreGive(SerialPortMutex); 
            }
            else
            {
                Serial.printf("Data|Send2\n");
                xSemaphoreTake(SerialPortMutex,portMAX_DELAY);
                Serial.write(0x55);
                Serial.write(0xAA);
                Serial.write(0x55);
                Serial.write(0xB0);
                Serial.write(PACKET_LEN+3);
                Serial.write(i);
                Serial.write(PacketNum);
                Serial.write((uint8_t)(RoundNumber));
                for(j=0;j<PACKET_LEN;j++)
                {
                    Serial.write(DataPacket.Data[(i*PACKET_LEN)+j]);
                }
                xSemaphoreGive(SerialPortMutex); 
            }
        }
    }
}

//以下应答库
void Order_Process()
{
    //时钟复位
    LastActTime    = xTaskGetTickCount();
    if (!(EVENT_GET(DeviceState)&AVA_PC))
    {
        EVENT_SET(DeviceState,AVA_PC);
        REPORT(TRUE,MesQueue_BTF,PC_ACT,TRUE);
    }
    switch(OrderCode)
    {
        case(0xA0):
        {
            SendBuffer[0] = EVENT_GET(DeviceState);
            Send(0xC0,1);
            break;
        }
        case(0xA1):
        {
            REPORT(TRUE,MesQueue_BTF,CTRL,(uint8_t)OrderBuffer[0]);
            SendBuffer[0] = EVENT_GET(DeviceState);
            Send(0xC0,1);
            break;
        }
    }
}

void StateReport()
{
    SendBuffer[0] = EVENT_GET(DeviceState);
    Send(0xC0,1);
}

void TimeReport(uint32_t Number)
{
    SendBuffer[0] = (uint8_t)(Number);
    SendBuffer[1] = (uint8_t)(Number>>8);
    SendBuffer[2] = (uint8_t)(Number>>16);
    SendBuffer[3] = (uint8_t)(Number>>24);
    Send(0xC1,4);
}

void Send(uint8_t Order,uint8_t Len)
{
    xSemaphoreTake(SerialPortMutex,portMAX_DELAY);
    Serial.write(0x55);
    Serial.write(0xAA);
    Serial.write(0x55);
    Serial.write(Order);
    Serial.write(Len);
    for (uint8_t i = 0;i < Len;i++)
    {
        Serial.write(SendBuffer[i]);
    }
    xSemaphoreGive(SerialPortMutex); 
}

void  Byte_Process(uint8_t Recive_Bytes)//蓝牙接收指令
{
    static uint8_t Port_State   = 0x00;    //接收状态机状态字
    static uint8_t Data_Len     = 0x00;    //数据写入长度
    static uint8_t Packet_Point = 0x00;    //数据写入指针
    if(Data_Len>0)
    {
        OrderBuffer[Packet_Point]  = (uint8_t)Recive_Bytes;//写入指令   
        Packet_Point             += 1;//指针加1
        Data_Len                 -= 1;//长度减
        if(Data_Len == 0)
        {
            Order_Process();          //包结束，处理包
        }
    }
    else
    {
        switch(Recive_Bytes)
        {
            case(0x55):
            {
                if     (Port_State == 0x00)Port_State = 0x01;
                else if(Port_State == 0x02)Port_State = 0x03;
                else                       Port_State = 0x00;
                break;
            }
            case(0xAA):
            {
                if     (Port_State == 0x01)Port_State = 0x02;
                else                       Port_State = 0x00;
                break;
            }
            default:
            {
                switch(Port_State)
                {
                    case 0x03:
                    {
                        OrderCode = (uint8_t)(Recive_Bytes);//读入命令码
                        Port_State  = 0x04;
                        break;
                    }
                    case 0x04:
                    {
                        Data_Len     = (uint8_t)(Recive_Bytes);//开始解包，读入指令长度
                        OrderLen     = (uint8_t)(Recive_Bytes);
                        Packet_Point = 0;   //写入指针归零
                        Port_State   = 0x00;//结束此包
                        if(Data_Len == 0)
                        {
                            Order_Process();    //包结束，处理包
                        }
                        break;
                    }
                    default:
                    {
                       Port_State  = 0x00;
                       break;
                    }
                }
                break;
            }
        }
    }
}
