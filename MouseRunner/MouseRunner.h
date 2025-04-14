//浙江大学 生仪学院 李嘉宇 22315088  TEL:19177225480
#if !defined(CONFIG_H)
#define CONFIG_H

//背景宏定义
#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include <U8g2lib.h>
#include "esp_random.h"
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#define DEBUG

//Device
#define S0_D                19
#define S0_C                18
#define S1_D                5
#define S1_C                17

//Button
#define IO_PB_PLAY          34
#define IO_PB_STOP          35

//IIC
#define SDA                 22
#define SCL                 21

#define TRUE                0x01
#define FALSE               0x00

#define AVA_DEV_0           (1<<0)
#define AVA_DEV_1           (1<<1)
#define AVA_PC              (1<<2)
#define STA_REC             (1<<3)
#define STA_PAUSE           (1<<4)
#define PB_ACT_PLAY         (1<<5)
#define PB_ACT_STOP         (1<<6)

#define FEQ                 120 //200 20
#define DATAITEM_NUM        24
#define PACKET_LEN          12
#define DELAY               1000/FEQ
#define PB_DELAY_TIME_MS    100
#define PB_CHECK_TIME_MS    70

//快捷定义
#define EVENT_SET(EVENT,FLAG)   (xEventGroupSetBits(EVENT,(FLAG)))         
#define EVENT_CLR(EVENT,FLAG)   (xEventGroupClearBits(EVENT,(FLAG)))
#define EVENT_GET(EVENT)        ((uint8_t)xEventGroupGetBits(EVENT))
#define EVENT_WAIT(EVENT,FLAG)  ((uint8_t)xEventGroupWaitBits(EVENT,(FLAG),\
                                                             pdFALSE,pdTRUE,\
                                                             portMAX_DELAY))
//#define EVENT_CHECK(EVENT,FLAG) (EVENT_GET(EVENT)&FLAG)
#define EVENT_CHECK(EVENT,FLAG) (((uint8_t)xEventGroupWaitBits(EVENT,(FLAG),pdFALSE,pdFALSE,0))&(FLAG))
#define REPORT(cond,queue,event,item) if(cond){struct Message Mes= {event,item};xQueueSend(queue,(const void*)&(Mes),0);}
#define QUEUE_WAIT(queue,Mes)   xQueueReceive(queue,(void*)&(Mes),portMAX_DELAY)
#define QUEUE_CHECK(queue)      (uint8_t)uxQueueMessagesWaiting(queue)

#define HALT                    vTaskDelay(1000000000)

//数据队列 
QueueHandle_t              DataQueue;
QueueHandle_t              MesQueue_BTF;

//消息
struct Message{uint8_t Event;uint8_t Item;};
struct DataItem{uint8_t length;
                uint8_t Data[DATAITEM_NUM];};
enum   Event{NONE,
             PC_ACT,DEV0_ACT,
             DEV1_ACT,CTRL,
             ERROR,WARNING,REFRESH};
enum   WARNING_ITEM  {OUT_SPEED_0_X,\
                      OUT_SPEED_0_Y,\
                      OUT_SPEED_1_X,\
                      OUT_SPEED_1_Y,\
                      UNABLE};
enum   ERROR_ITEM    {PC_LOSS_CONNECT,\
                      DEV0_LOSS_CONNECT,\
                      DEV1_LOSS_CONNECT};
enum   CTRL_ITEM     {PLAY,STOP};


//事件组
EventGroupHandle_t         DeviceState;

//互斥锁
SemaphoreHandle_t          SerialPortMutex;

//包计量
uint32_t                   SampNumber;

//测量计量
uint8_t                   RoundNumber;

//函数体定义

void    PB_Task(void* ptr);
void    SensorTask(void* ptr);
void    DataTransTask(void *parameter);
void    OrderTask(void *parameter);
void    FrontEnd_Task(void* point);
void    StateReport();
void TimeReport(uint32_t Number);


//前端结构体定义
struct Position
{
   uint8_t x;
   uint8_t y;
   uint8_t w;
   uint8_t h;
};

struct Item_type
{
   struct Position Pos;
   const uint8_t * font;
   char*           text;
   uint8_t         isFrame;
};

class Page
{
   public:
      void         Draw();
      uint8_t      Refresh();
      void*        PageItem;
      int8_t       SparkCount;
      int8_t       SparkTarget;
};
#endif // CONFIG_H
