
#include "MouseRunner.h"//关联头文件

#define ICON_WIDTH 32
#define ICON_HEIGHT 32
#define ICON_GAP 4
#define ICON_BGAP 16
#define ICON_Y 32+ ICON_GAP

extern QueueHandle_t      MesQueue_BTF;
extern EventGroupHandle_t DeviceState;
extern uint32_t           SampNumber;
extern uint8_t           RoundNumber;


//U8G2定义                                           
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE,SCL,SDA); 
void    FrontEnd_Task(void* point);
void    MessageResponse(struct Message Mes,Page* Cur_Page);
void    TimeCountTickISR(TimerHandle_t xTimer);

//page
struct Item_type   HomeInfomation[]
{
  //ROW 1
  {{2 ,5 ,45,12},u8g2_font_wqy12_t_gb2312a,(char*)"右传感X",1},
  {{2 ,25,45,12},u8g2_font_wqy12_t_gb2312a,(char*)"上位机X",1},
  {{2 ,45,45,12},u8g2_font_wqy12_t_gb2312a,(char*)"底传感X",1},
   //ROW 2
  {{69,10,36,12},u8g2_font_wqy16_t_gb2312,(char*)"待机中",0},// "00:00:00"
  {{63,32,72,12},u8g2_font_wqy16_t_gb2312,(char*)"00:00:00",0},
  {{60,52,60,12},u8g2_font_wqy12_t_gb2312a,(char*)"",0},
  //ROW 3
  {{0,0,0,0},NULL,NULL,0} 
};

//=================================

Page           MainPage;
TimerHandle_t  TimeCountTimer;
void    TimeCountTickISR(TimerHandle_t xTimer)
{
    uint32_t TimeCount     = 0x00;
    uint8_t  Hour          = 0;
    uint8_t  Minute        = 0;
    uint8_t  Second        = 0;
    static char TimeChar[] = "00:00:00";
    if(EVENT_GET(DeviceState)&STA_REC)
    {
        TimeCount  = SampNumber/FEQ;
        Hour       = (TimeCount/3600);
        TimeCount -= Hour*3600;
        Minute     = TimeCount/60;
        Second     = TimeCount%60;
        sprintf(TimeChar,"%02d:%02d:%02d",Hour,Minute,Second);
        HomeInfomation[4].text = (char*)&TimeChar;
        REPORT(TRUE,MesQueue_BTF,REFRESH,FALSE);
        TimeReport(SampNumber);
    }
}

void    Page::Draw()
{   
    uint8_t i = 0;
    while(!((((struct Item_type*)(this->PageItem))+i)->text == NULL))  
    {
        uint8_t x  = (((struct Item_type*)(this->PageItem))+i)->Pos.x;
        uint8_t y  = (((struct Item_type*)(this->PageItem))+i)->Pos.y;
        const uint8_t* TarFont = (((struct Item_type*)(this->PageItem))+i)->font;
        u8g2.setFont(TarFont);
        uint8_t sw = u8g2.getStrWidth((((struct Item_type*)(this->PageItem))+i)->text);
        uint8_t ch = (((struct Item_type*)(this->PageItem))+i)->Pos.h;
        if(sw<((((struct Item_type*)(this->PageItem))+i)->Pos.w))sw = (((struct Item_type*)(this->PageItem))+i)->Pos.w;
        if((((struct Item_type*)(this->PageItem))+i)->isFrame)u8g2.drawFrame(x-2,y-2,sw+4,ch+4);
        if(TarFont == u8g2_font_wqy12_t_gb2312a)u8g2.setCursor(x,y+ch-1);
        else u8g2.setCursor(x,y+ch);
        if(((this->SparkCount)!=0)&&((this->SparkTarget)==i))
        {
            if((this->SparkCount)%2 == 0)u8g2.print((((struct Item_type*)(this->PageItem))+i)->text);   
        }
        else
        {
            u8g2.print((((struct Item_type*)(this->PageItem))+i)->text);  
        }
        i ++;
    }
    //------Normal Zone------
    u8g2.drawLine(52,0,52,64);
}

uint8_t Page::Refresh()
{   
    uint8_t Flag = FALSE;
    if(this->SparkCount > 0)
    {
        this->SparkCount -= 1;
        Flag = TRUE;
    }
    uint8_t State = EVENT_GET(DeviceState);
    if((State&STA_REC)&(!(State&STA_PAUSE)))Flag = TRUE;
    return Flag;
}

void MessageResponse(struct Message Mes,Page* Cur_Page)
{
    //Serial.printf("MessageResponse|Event:%d|Item:%d\n",Mes.Event,Mes.Item);
    switch (Mes.Event)
    {
    case(PC_ACT):
    {
        if(Mes.Item==TRUE)
        {
            HomeInfomation[1].text = (char*)"上位机O";
            HomeInfomation[1].isFrame = 0;
        }
        else if(Mes.Item==FALSE)
        {
            HomeInfomation[1].text = (char*)"上位机X";
            HomeInfomation[1].isFrame = 1;
        }
        break;
    }
    case(DEV0_ACT):
    {
        if(Mes.Item==TRUE)
        {
            HomeInfomation[0].text = (char*)"右传感O";
            HomeInfomation[0].isFrame = 0;
        }
        else if(Mes.Item==FALSE)
        {
            HomeInfomation[0].text = (char*)"右传感X";
            HomeInfomation[0].isFrame = 1;
        }
        break;
    }
    case(DEV1_ACT):
    {
        if(Mes.Item==TRUE)
        {
            HomeInfomation[2].text = (char*)"底传感O";
            HomeInfomation[2].isFrame = 0;
        }
        else if(Mes.Item==FALSE)
        {
            HomeInfomation[2].text = (char*)"底传感X";
            HomeInfomation[2].isFrame = 1;
        }
        break;
    }
    case(CTRL):
    {
        if(Mes.Item==PLAY)
        {
            uint8_t State = EVENT_GET(DeviceState);
            if((State&AVA_DEV_0)&&(State&AVA_DEV_1)&&(State&AVA_PC))
            {
                if((State&STA_REC)&&(!(State&STA_PAUSE)))
                {
                    EVENT_SET(DeviceState,STA_PAUSE);
                    StateReport();
                    HomeInfomation[3].text = (char*)"暂停中";
                    xTimerStop(TimeCountTimer,portMAX_DELAY);
                }
                else if((State&STA_REC)&&((State&STA_PAUSE)))
                {
                    //Serial.printf("State|A\n",State);
                    EVENT_CLR(DeviceState,STA_PAUSE);
                    StateReport();
                    HomeInfomation[3].text = (char*)"记录中";
                    xTimerStart(TimeCountTimer,0);
                }
                else if(!(State&STA_REC))
                {
                    RoundNumber            = (uint8_t)esp_random();
                    EVENT_SET(DeviceState,STA_REC);
                    EVENT_CLR(DeviceState,STA_PAUSE);
                    StateReport();
                    HomeInfomation[3].text = (char*)"记录中";
                    HomeInfomation[4].text = (char*)"00:00:00";
                    HomeInfomation[5].text = (char*)"";
                    xTimerStart(TimeCountTimer,0);
                    TimeReport(0);
                }
            }
            else
            {
                HomeInfomation[5].text = (char*)"条件未具备";
                Cur_Page->SparkCount   = 11;
                Cur_Page->SparkTarget  = 5;
            }
        }
        else if(Mes.Item==STOP)
        {
            uint8_t State = EVENT_GET(DeviceState);
            if((State&(STA_REC)))
            {
                EVENT_CLR(DeviceState,STA_REC);
                EVENT_CLR(DeviceState,STA_PAUSE);
                StateReport();
                HomeInfomation[3].text = (char*)"已停止";
                xTimerStop(TimeCountTimer,portMAX_DELAY);
            }
            else if(!(State&(STA_REC)))
            {
                HomeInfomation[3].text = (char*)"待机中";
                HomeInfomation[4].text = (char*)"00:00:00";
                HomeInfomation[5].text = (char*)"";
                Cur_Page->SparkCount   = 0;
                Cur_Page->SparkTarget  = -1;
                TimeReport(0);
            }
        }
        break;
    }
    case(WARNING):
    {
        switch (Mes.Item)
        {
            case OUT_SPEED_0_X:
            {
                HomeInfomation[5].text = (char*)"底X轴超速";
                Cur_Page->SparkCount   = 11;
                Cur_Page->SparkTarget  = 5;
                break;
            }
            case OUT_SPEED_0_Y:
            {
                HomeInfomation[5].text = (char*)"底Y轴超速";
                Cur_Page->SparkCount   = 11;
                Cur_Page->SparkTarget  = 5;
                break;
            }
            case OUT_SPEED_1_X:
            {
                HomeInfomation[5].text = (char*)"右X轴超速";
                Cur_Page->SparkCount   = 11;
                Cur_Page->SparkTarget  = 5;
                break;
            }
            case OUT_SPEED_1_Y:
            {
                HomeInfomation[5].text = (char*)"右Y轴超速";
                Cur_Page->SparkCount   = 11;
                Cur_Page->SparkTarget  = 5;
                break;
            }
            case UNABLE:
            {
                HomeInfomation[5].text = (char*)"无法采样";
                Cur_Page->SparkCount   = 11;
                Cur_Page->SparkTarget  = 5;
                break;
            }
            default:
                break;
        }
        break;
    }
    case(ERROR):
    {
        switch (Mes.Item)
        {
            case PC_LOSS_CONNECT:
            {
                HomeInfomation[5].text = (char*)"!上位机离线!";
                HomeInfomation[1].text = (char*)"上位机X";
                Cur_Page->SparkCount   = 11;
                Cur_Page->SparkTarget  = 5;
                REPORT(TRUE,MesQueue_BTF,CTRL,STOP);
                break;
            }
            case DEV0_LOSS_CONNECT:
            {
                HomeInfomation[5].text = (char*)"!右传感离线!";
                HomeInfomation[0].text = (char*)"右传感X";
                Cur_Page->SparkCount   = 11;
                Cur_Page->SparkTarget  = 5;
                break;
            }
            case DEV1_LOSS_CONNECT:
            {
                HomeInfomation[5].text = (char*)"!底传感离线!";
                HomeInfomation[2].text = (char*)"底传感X";
                Cur_Page->SparkCount   = 11;
                Cur_Page->SparkTarget  = 5;
                break;
            }
            default:
                break;
        }
        break;
    }
    }
}

void      FrontEnd_Task(void* point)
{
    u8g2.begin(); 
    u8g2.enableUTF8Print();
    //--------------------------------
    Page* Top_Page       = &MainPage;
    MainPage             = Page();      
    MainPage.PageItem    = (void*)HomeInfomation;
    MainPage.SparkCount  = 0;
    MainPage.SparkTarget = 0;
    //--------------------------------
    struct Message Mes  = {NONE,FALSE};
    uint8_t  Cycle_Flag = 1;
    TimeCountTimer      = xTimerCreate("REC_TIME",333,pdTRUE,(void*)0,TimeCountTickISR);
    //--------------------------------
    REPORT(TRUE,MesQueue_BTF,NONE,NONE);
    //--------------------------------
    u8g2.setPowerSave(0);
    while(1)
    {
        QUEUE_WAIT(MesQueue_BTF,Mes);
        MessageResponse(Mes,Top_Page);
        do{
            u8g2.clearBuffer();
            Top_Page->Draw();
            u8g2.sendBuffer();
            if(QUEUE_CHECK(MesQueue_BTF))break;
            Cycle_Flag = Top_Page->Refresh();
        }while(Cycle_Flag);
    }
}
