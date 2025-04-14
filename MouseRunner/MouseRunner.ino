
#include "MouseRunner.h"  //关联头文件

extern QueueHandle_t      DataQueue;
extern QueueHandle_t      MesQueue_BTF;
extern EventGroupHandle_t DeviceState;
extern SemaphoreHandle_t  SerialPortMutex;

void System_Init();
void PB_Task(void* ptr);

uint32_t LastPBActivateTime = 0x00;

void IRAM_ATTR PB_PLAY_ISR()
{
    uint32_t PBActivateTime = (uint32_t)xTaskGetTickCountFromISR();
    if((PBActivateTime - LastPBActivateTime)>PB_DELAY_TIME_MS)
    {
        LastPBActivateTime    = PBActivateTime;
        BaseType_t yield_flag = pdFALSE;
        xEventGroupSetBitsFromISR(DeviceState,PB_ACT_PLAY,&yield_flag);
        portYIELD_FROM_ISR(yield_flag);
    }
}

void IRAM_ATTR PB_STOP_ISR()
{
    uint32_t PBActivateTime = (uint32_t)xTaskGetTickCountFromISR();
    if((PBActivateTime - LastPBActivateTime)>PB_DELAY_TIME_MS)
    {
        LastPBActivateTime    = PBActivateTime;
        BaseType_t yield_flag = pdFALSE;
        xEventGroupSetBitsFromISR(DeviceState,PB_ACT_STOP,&yield_flag);
        portYIELD_FROM_ISR(yield_flag);
    }
}

void PB_Task(void* ptr){
    uint8_t PB_Flag = 0x00;
    while(TRUE)
    {
        PB_Flag = (uint8_t)xEventGroupWaitBits(DeviceState,\
                                              PB_ACT_PLAY|PB_ACT_STOP,\
                                              pdTRUE,pdFALSE,\
                                              portMAX_DELAY);
        vTaskDelay(PB_CHECK_TIME_MS);
        if(PB_Flag&PB_ACT_PLAY)
        {
            if(digitalRead(IO_PB_PLAY) == LOW)
            {
               REPORT(TRUE,MesQueue_BTF,CTRL,PLAY);
            }
        }
        if(PB_Flag&PB_ACT_STOP)
        {
            if(digitalRead(IO_PB_STOP) == LOW)
            {
               REPORT(TRUE,MesQueue_BTF,CTRL,STOP);
            }
        }
    }
}


void System_Init() {
  //设备状态监控
  Serial.begin(115200);
  //装载数据队列
  DataQueue       = xQueueCreate(2, sizeof(struct DataItem));
  MesQueue_BTF    = xQueueCreate(32, sizeof(struct Message));
  //装载事件
  DeviceState     = xEventGroupCreate();
  //装载互斥锁
  SerialPortMutex = xSemaphoreCreateMutex();
  //装载中断
  attachInterrupt(IO_PB_PLAY, PB_PLAY_ISR, FALLING);
  attachInterrupt(IO_PB_STOP, PB_STOP_ISR, FALLING);
}

//  PB_Task
//  SensorTask
//  OrderTask
//  DataTransTask
//  FrontEnd_System

void setup() {
  //上电
  System_Init();  //系统初始化
  //数据进出
  xTaskCreate(SensorTask,    "SensorTask", 3000, NULL, 1, NULL);
  xTaskCreate(OrderTask ,    "OrderTask" , 3000, NULL, 1, NULL);
  xTaskCreate(DataTransTask, "DataTransTask ", 3000, NULL, 1, NULL);
  xTaskCreate(FrontEnd_Task, "FrontEnd_Task ", 6000, NULL, 1, NULL);
  xTaskCreate(PB_Task,       "PB_Task ", 1000, NULL, 1, NULL);
  //引脚
  pinMode(S0_C, OUTPUT);
  pinMode(S1_C, OUTPUT);
  pinMode(IO_PB_PLAY, INPUT);
  pinMode(IO_PB_STOP, INPUT);
}

void loop() {
  vTaskDelay(portMAX_DELAY);
}
