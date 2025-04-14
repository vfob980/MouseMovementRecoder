//浙江大学 生仪学院 李嘉宇 22315088   TEL:19177225480
#include "MouseRunner.h"//关联头文件
extern QueueHandle_t             DataQueue;
extern QueueHandle_t             MesQueue_BTF;
extern EventGroupHandle_t        DeviceState;
extern SemaphoreHandle_t         SerialPortMutex;

//状态定时器
TimerHandle_t                    S_stateTimer;
extern uint32_t                  SampNumber;

void    us_delay(uint32_t us);
void    S_StateTimerISR(TimerHandle_t xTimer);
void    SensorTask(void* ptr);
void    Init();
void    Running_Check();
void    Sensor_Check();
void    Sync(uint8_t En_0,uint8_t En_1);
void    DualWrite(uint8_t Addr,uint8_t Snd,uint8_t En_0,uint8_t En_1);
void    DualRead(uint8_t Addr,int8_t* ID0,int8_t* ID1,uint8_t En_0,uint8_t En_1);

void  us_delay(uint32_t us)
{
    vTaskSuspendAll();
    uint32_t start, curr;
    while (us--)
    {   
        __asm__ __volatile__("rsr %0, ccount" : "=r"(start));
        do
        {
            __asm__ __volatile__("rsr %0, ccount" : "=r"(curr));  
        }while (curr - start <= 240);
    }
    xTaskResumeAll();
}

void  S_StateTimerISR(TimerHandle_t xTimer)
{
    if(!EVENT_CHECK(DeviceState,STA_REC))Sensor_Check();
}

void  SensorTask(void* ptr)
{
    struct DataItem Buffer = {0,{}};
    int8_t  ST0,ST1;
    int8_t  DX0,DY0;
    int8_t  DX1,DY1;
    uint8_t state;
    TickType_t xLastWakeTime;
    S_stateTimer  = xTimerCreate("PC_STATE",1500,pdTRUE,0,S_StateTimerISR);
    xTimerStart(S_stateTimer,portMAX_DELAY);
    Buffer.length = 0;
    while(1)
    {
        xLastWakeTime = xTaskGetTickCount();
        while (EVENT_CHECK(DeviceState,STA_REC))
        {
            if(!(EVENT_GET(DeviceState)&STA_PAUSE))
            {
                //Serial.printf("Data3\n")
                DualRead(0x02,&ST0,&ST1);
                DualRead(0x03,&DX0,&DX1);
                DualRead(0x04,&DY0,&DY1);
                REPORT(ST0&BIT4,MesQueue_BTF,WARNING,OUT_SPEED_0_Y);
                REPORT(ST0&BIT3,MesQueue_BTF,WARNING,OUT_SPEED_0_X);
                REPORT(ST1&BIT4,MesQueue_BTF,WARNING,OUT_SPEED_1_Y);
                REPORT(ST1&BIT3,MesQueue_BTF,WARNING,OUT_SPEED_0_X);
                //Serial.printf("DX0:%d|DY0:%d|DY0:%d|DY1:%d\n",DX0,DY0,DX1,DY1);
                Buffer.Data[Buffer.length*4+0] = (uint8_t)(DX0>=0 ? DX0 : 127-DX0);
                Buffer.Data[Buffer.length*4+1] = (uint8_t)(DY0>=0 ? DY0 : 127-DY0);
                Buffer.Data[Buffer.length*4+2] = (uint8_t)(DX1>=0 ? DX1 : 127-DX1);
                Buffer.Data[Buffer.length*4+3] = (uint8_t)(DY1>=0 ? DY1 : 127-DY1);
                Buffer.length += 1;
                SampNumber    += 1;
                state = EVENT_GET(DeviceState);
                if((Buffer.length == DATAITEM_NUM/4))
                {
                    xQueueSend(DataQueue,(const void*)&Buffer,portMAX_DELAY);
                    Running_Check();
                    Buffer.length = 0;
                }
            }
            else
            {
                if((Buffer.length > 0))
                {
                    xQueueSend(DataQueue,(const void*)&Buffer,portMAX_DELAY);
                    Buffer.length = 0;
                }
                Running_Check();
            }
            vTaskDelayUntil(&xLastWakeTime,DELAY);
        }
        if(Buffer.length > 0)
        {
            xQueueSend(DataQueue,(const void*)&Buffer,portMAX_DELAY);
        }
        EVENT_WAIT(DeviceState,STA_REC);
        //Serial.printf("Data\n");
        Buffer.length = 0;
        SampNumber    = 0;
        //Serial.printf("Data2\n");
    }
}

void  Init()
{
    Sync();
    DualWrite(0x06,0b10000000);
    vTaskDelay(1);
    Sync();
    DualWrite(0x05,0b10100000);
    DualWrite(0x06,0b00000000);
}

void Running_Check()
{
    int8_t ID0,ID1;
    DualRead(0x00,&ID0,&ID1);
    //------------DEV0-----------------
    if((ID0!=0x30))
    {
        EVENT_CLR(DeviceState,AVA_DEV_0);
        REPORT(TRUE,MesQueue_BTF,ERROR,DEV0_LOSS_CONNECT)
    }
    //-------------DEV1-----------------
    if((ID1!=0x30))
    {
        EVENT_CLR(DeviceState,AVA_DEV_1);
        REPORT(TRUE,MesQueue_BTF,ERROR,DEV1_LOSS_CONNECT)
    }
}

void  Sensor_Check()
{
    int8_t ID0,ID1;
    DualRead(0x00,&ID0,&ID1);
    uint8_t State = EVENT_GET(DeviceState);
    uint8_t Sync_State = FALSE;
    //-------------DEV0-----------------
    if((ID0==0x30)&&(!(State&AVA_DEV_0)))
    {
        EVENT_SET(DeviceState,AVA_DEV_0);
        REPORT(TRUE,MesQueue_BTF,DEV0_ACT,TRUE)
        Sync_State = TRUE;
    }
    if((ID0!=0x30)&&(State&AVA_DEV_0))
    {
        EVENT_CLR(DeviceState,AVA_DEV_0);
        REPORT(TRUE,MesQueue_BTF,DEV0_ACT,FALSE)
    }
    //-------------DEV1-----------------
    if((ID1==0x30)&&(!(State&AVA_DEV_1)))
    {
        EVENT_SET(DeviceState,AVA_DEV_1);
        REPORT(TRUE,MesQueue_BTF,DEV1_ACT,TRUE)
        Sync_State = TRUE;
    }
    if((ID1!=0x30)&&(State&AVA_DEV_1))
    {
        EVENT_CLR(DeviceState,AVA_DEV_1);
        REPORT(TRUE,MesQueue_BTF,DEV1_ACT,FALSE)
    }
    if(Sync_State)Init();
}

void  Sync()
{
    pinMode(S0_D,OUTPUT);
    pinMode(S1_D,OUTPUT);
    digitalWrite(S0_C,LOW);
    digitalWrite(S1_C,LOW);
    us_delay(1);
    digitalWrite(S0_C,HIGH);
    digitalWrite(S1_C,HIGH);
    vTaskDelay(1);
}

void  DualWrite(uint8_t Addr,uint8_t Snd)
{   
    Addr |= 0b10000000;
    pinMode(S0_D,OUTPUT);
    pinMode(S1_D,OUTPUT);
    for(uint8_t P=0b10000000;P>0;P=P>>1)
    {   
        digitalWrite(S0_C,LOW);
        digitalWrite(S1_C,LOW);
        digitalWrite(S0_D,Addr&P ? HIGH:LOW);
        digitalWrite(S1_D,Addr&P ? HIGH:LOW);
        us_delay(1);
        digitalWrite(S0_C,HIGH);
        digitalWrite(S1_C,HIGH);
        us_delay(1);
    }
    for(uint8_t P=0b10000000;P>0;P=P>>1)
    {   
        digitalWrite(S0_C,LOW);
        digitalWrite(S1_C,LOW);
        digitalWrite(S0_D,Snd&P ? HIGH:LOW);
        digitalWrite(S1_D,Snd&P ? HIGH:LOW);
        us_delay(1);
        digitalWrite(S0_C,HIGH);
        digitalWrite(S1_C,HIGH);
        us_delay(1);
    }
}

void  DualRead(uint8_t Addr,int8_t* ID0,int8_t* ID1)
{
    Addr &= ~0b10000000;
    (*ID0) = 0x00;
    (*ID1) = 0x00;
    pinMode(S0_D,OUTPUT);
    pinMode(S1_D,OUTPUT);
    for(uint8_t P=0b10000000;P>0;P=P>>1)
    {   
        digitalWrite(S0_C,LOW);
        digitalWrite(S1_C,LOW);
        digitalWrite(S0_D,Addr&P ? HIGH:LOW);
        digitalWrite(S1_D,Addr&P ? HIGH:LOW);
        us_delay(1);
        digitalWrite(S0_C,HIGH);
        digitalWrite(S1_C,HIGH);
        us_delay(1);
    }
    us_delay(3);
    pinMode(S0_D,INPUT);
    pinMode(S1_D,INPUT);
    for(uint8_t P=0b10000000;P>0;P=P>>1)
    {   
        digitalWrite(S0_C,LOW);
        digitalWrite(S1_C,LOW);
        if(digitalRead(S0_D)==HIGH)(*ID0) |= P;
        if(digitalRead(S1_D)==HIGH)(*ID1) |= P;
        us_delay(1);
        digitalWrite(S0_C,HIGH);
        digitalWrite(S1_C,HIGH);
        us_delay(1);
    }
}