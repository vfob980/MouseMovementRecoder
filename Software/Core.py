from  os import remove,getcwd
from  serial import Serial
from  serial.tools import list_ports
import time
from  copy import deepcopy
import threading
import datetime 

def c_GetPort():
    ports_list = list(list_ports.comports())
    if len(ports_list) <= 0:
        print("无串口设备。")
        return None
    else:
        print("可用的串口设备如下：")
        for i,comport in enumerate(ports_list):
            print(i,list(comport)[0], list(comport)[1])
    tar      = input(">>>")
    portname = list(ports_list[int(tar)])[0]
    S        = Serial(portname, 115200)    # 打开COM17，将波特率配置为115200，其余参数使用默认值
    if S.isOpen():                        # 判断串口是否成功打开
        print("打开串口成功。")
        print(S.name)    # 输出串口号
    else:
        print("打开串口失败。")
        return None
    return S
 
def n_GetPort():
    ports_list = list(list_ports.comports())
    return ports_list
 
def ExportToFile(path,DataItem):
    st    = DataItem[0]
    ed    = DataItem[1]
    stamp = DataItem[2]
    info  = DataItem[3]
    data  = DataItem[4]
    with open(path,"w") as f:
        f.write("##########INFO##########\n")
        f.write("Start time:%s\n"%(st))
        f.write("End time:%s\n"%(ed))
        f.write("Export time:%s\n"%(datetime.datetime.fromtimestamp(time.time())\
                                                .strftime('%H:%M:%S %d/%m/%Y')))
        f.write("Sample rate:120Hz\n")
        f.write("Data length:%d\n"%(len(data[0])))
        f.write("Stamp:0x%x\n"%(stamp))
        f.write("##########LOG###########\n")
        for i,item in enumerate(info):
            if len(item) == 2:
                if item[0] == "#LOST PACKET":
                    f.write("%d>%s index:%d\n"%(i,item[0],item[1]))
                else:
                    f.write("%d>%s %s\n"%(i,item[0],item[1]))
            else:
                f.write("%d>%s %s index:%d\n"%(i,item[0],item[1],item[2]))
        f.write("##########DATA##########\n")
        f.write("INDEX> Botton_X_axis Botton_Y_axis Right_X_axis Right_Y_axis\n")
        for i in range(len(data[0])):
            f.write("%d> %d %d %d %d\n"%\
                (i,data[0][i],data[1][i],data[2][i],data[3][i]))

class Agreement():
    def __init__(self):
        self.s        = 0
        self.event    = 0xFF
        self.DL       = 0
        self.Data     = []
        self.ReadFlag = False    
        Packet = {"Event":0xA0,"Data":[]}  
        self.HeartBag = self.__Send(Packet)
        Packet = {"Event":0xA1,"Data":[0x00]}  
        self.PB_Play_Bag = self.__Send(Packet)
        Packet = {"Event":0xA1,"Data":[0x01]}  
        self.PB_Stop_Bag = self.__Send(Packet) 
    def Rece(self,b):
        b   = int.from_bytes(b,signed=False,byteorder="big")
        if   self.s == 0x00 and b == 0x55:self.s = 0x01
        elif self.s == 0x01 and b == 0xAA:self.s = 0x02
        elif self.s == 0x02 and b == 0x55:self.s = 0xA0
        elif self.s == 0xA0:
            self.s     = 0xA1
            self.event = b
        elif self.s == 0xA1:
            self.s     = 0xB0
            self.DL    = b
            if self.DL == 0:
                self.s     = 0x00
                self.ReadFlag = True
        elif self.s == 0xB0:
            self.Data.append(b) 
            self.DL   -= 1
            if self.DL == 0:
                self.s     = 0x00
                self.ReadFlag = True
        else:
            self.ReadFlag = False
            self.s        = 0x00
            self.Data     = []
        if self.ReadFlag:
            self.ReadFlag = False
            Mes           = {"Event":self.event,\
                             "Data":deepcopy(self.Data)}
            self.event    = 0x00
            self.Data     = []
            return Mes
        else:
            return None
    def __Send(self,Packet):
        temp = [0x55,0xAA,0x55]
        temp.append(Packet["Event"])
        temp.append(len(Packet["Data"]))
        if len(Packet["Data"]) > 0:
            temp += Packet["Data"]
        return temp
    
class Data():
    def __init__(self,Path):
        self.stamp      = 0
        self.cachepath  = Path
    def D_Write(self,Pack_id,Pack_N,stamp,Packet):
        Sign = lambda x: x if x <= 127 else 127-x 
        if self.stamp != stamp:
            self.stamp = stamp
            with open(self.cachepath,"a") as f:
                f.write("#START %d %d\n"%(time.time(),stamp))
        with open(self.cachepath,"a") as f:
            f.write("#PACKET %d %d %d\n"%(Pack_id,Pack_N,len(Packet)))  
            for i in range(len(Packet)//4):
                f.write("> %d %d %d %d\n"%( Sign(Packet[i*4+0]),\
                                            Sign(Packet[i*4+1]),\
                                            Sign(Packet[i*4+2]),\
                                            Sign(Packet[i*4+3])))
    def D_Event(self,event):
        if   event == "PAUSE":
            with open(self.cachepath,"a") as f:
                f.write("#PAUSE %d\n"%(time.time()))
        elif event == "END":
            with open(self.cachepath,"a") as f:
                f.write("#END %d\n"%(time.time()))
        elif event == "RESUME":
            with open(self.cachepath,"a") as f:
                f.write("#RESUME %d\n"%(time.time())) 
    def Export(self):
        Raw       = []
        Database  = []
        stamp     = -1
        try:
            with open(self.cachepath,"r") as f:
                Raw = f.readlines()
        except:
            return []
        Raw += ["#START 0 0"]
        for line in Raw:
            line_s  = line.split(" ")
            tag     = str(line_s[0])
            if tag == ">":
                i_rec[0].append(int(line_s[1]))
                i_rec[1].append(int(line_s[2]))
                i_rec[2].append(int(line_s[3]))
                i_rec[3].append(int(line_s[4]))
            elif tag == "#START":
                if stamp != -1:
                    i_start = datetime.datetime.fromtimestamp(i_start)\
                                    .strftime('%H:%M:%S %d/%m/%Y')
                    i_end   = datetime.datetime.fromtimestamp(i_end)\
                                    .strftime('%H:%M:%S %d/%m/%Y') \
                                        if i_end != -1 else "NONE"
                    Database.append((i_start,i_end,stamp,\
                                     deepcopy(i_msg),\
                                     deepcopy(i_rec)))
                i_rec   = [[],[],[],[]]
                i_pack  = -1
                i_start = int(line_s[1])
                i_end   = -1
                stamp   = int(line_s[2])
                i_date  = datetime.datetime.fromtimestamp(i_start)\
                                .strftime('%H:%M:%S %d/%m/%Y')
                i_msg   = [[tag,i_date],]
            elif tag == "#END":
                i_end   = int(line_s[1])
                i_date  = datetime.datetime.fromtimestamp(i_end)\
                                .strftime('%H:%M:%S %d/%m/%Y')
                i_msg.append([tag,i_date])
            elif tag == "#PACKET":
                pack_id = int(line_s[1])
                pack_N  = int(line_s[2])
                if pack_id != i_pack+1:
                    i_msg.append(["#LOST PACKET",len(i_rec[0])])
                else:
                    if pack_id != pack_N-1:
                        i_pack = pack_id 
                    else:
                        i_pack = -1
            elif tag == "#PAUSE":
                i_date  = datetime.datetime.fromtimestamp(int(line_s[1]))\
                                .strftime('%H:%M:%S %d/%m/%Y')
                i_msg.append(["#PAUSE",i_date])
            elif tag == "#RESUME":
                i_date  = datetime.datetime.fromtimestamp(int(line_s[1]))\
                                .strftime('%H:%M:%S %d/%m/%Y')
                i_msg.append(["#RESUME",i_date,len(i_rec[0])])
        return Database
    def Clear(self):
        remove(self.cachepath)


class Core(threading.Thread):
    def __init__(self,Q):
        self.Q         = Q
        self.SendLock  = threading.Lock()
        self.cachepath = getcwd() + "/" + "cache.temp"
        self.exit      = False
        self.D         = Data(self.cachepath)
        self.Time      = 0
        super().__init__()
        
    def getQueue(self):
        return self.Q
      
    def init(self):
        self.A = Agreement()
        self.AVA_DEV_0 = False
        self.AVA_DEV_1 = False
        self.AVA_PC    = False
        self.STA_REC   = False
        self.STA_PAUSE = False
        self.s         = c_GetPort()
        self.timer     = threading.Timer(1,self.__Heart_Irq)
        self.exit      = False
        self.running   = True 
        self.DispalyFlag = False
        self.lastacttime = 0
        if self.s is not None:
            self.running = True
            return True
        else:
            self.running = False
            return False
        
    def connect(self,portname):
        def OpenPort(portname):
            try:
                S        = Serial(portname, 115200)  
                return S 
            except:
                return None
        self.A           = Agreement()
        self.D           = Data(self.cachepath)
        self.AVA_DEV_0   = False
        self.AVA_DEV_1   = False
        self.AVA_PC      = False
        self.STA_REC     = False
        self.STA_PAUSE   = False
        self.s           = OpenPort(portname)
        if self.s is None:
            self.running = False
            self.exit    = True
            self.Q.put(["Error","Port busy"],False)
            return False
        self.running     = True
        self.timer       = threading.Timer(1,self.__Heart_Irq)
        self.exit        = False
        self.running     = True 
        self.DispalyFlag = False
        self.start()
        self.Q.put(["Message","Port open",portname],False)
        self.lastacttime = float("inf")
        self.CONNECT     = False
        return True
        
    def __GetState(self,Packet):
        self.lastacttime = time.time()
        State = Packet["Data"][0]
        self.AVA_DEV_0 = bool(State & 0x01)
        self.AVA_DEV_1 = bool(State & 0x02)
        self.AVA_PC    = bool(State & 0x04)
        STA_REC        = bool(State & 0x08)
        STA_PAUSE      = bool(State & 0x10)
        if (not self.STA_PAUSE) and STA_PAUSE:
            self.D.D_Event("PAUSE")
            self.Q.put(["Sample","rec pause"],False)
        if not (self.STA_REC) and  STA_REC:
            self.Q.put(["Sample","rec start"],False)
        if (self.STA_REC) and not STA_REC:
            self.D.D_Event("END")
            self.Q.put(["Sample","rec end"],False)
        elif (self.STA_PAUSE) and not STA_PAUSE:
            self.D.D_Event("RESUME")
            self.Q.put(["Sample","rec resume"],False)
        self.STA_REC   = STA_REC
        self.STA_PAUSE = STA_PAUSE
        self.Q.put(["Message","State updata",State],False)
        
    def __GetData(self,Packet):
        Data   = Packet["Data"]
        Packid = int(Data[0])
        PackN  = int(Data[1])
        Stamp  = int(Data[2])
        Data   = Data[3:]
        Data   = list(map(lambda x: x if x < 128 else 127-x , Data))
        self.D.D_Write(Packid,PackN,Stamp,Data)
        if self.DispalyFlag:
            self.Q.put(["Data","Data",Data],False)
            
    def __Heart_Irq(self):
        self.__Send(self.A.HeartBag)
        time.sleep(0.01)
        if time.time() - self.lastacttime>2:
            self.CONNECT     = False
            self.Q.put(["Error","Device lost"],False)
            self.stop()
        else:
            self.CONNECT     = True
        if self.running:
            self.timer = threading.Timer(1,self.__Heart_Irq)
            self.timer.start()
        
    def __GetTime(self,Packet):
        Data    = Packet["Data"]
        SampleNum = int(Data[3])*65535*256 + \
                    int(Data[2])*65535 + \
                    int(Data[1])*256 + \
                    int(Data[0])
        Time     = SampleNum//50
        if self.DispalyFlag:
            self.Q.put(["Message","Time updata",Time],False)
        
    def isRuning(self):
        if self.running:
            return True
        else:
            return False
        
    def stop(self):
        self.exit        = True
        self.running     = False
        self.AVA_DEV_0   = False
        self.AVA_DEV_1   = False
        self.AVA_PC      = False
        self.STA_REC     = False
        self.STA_PAUSE   = False
        
    def exportcache(self):
        Database = self.D.Export()
        return Database
    
    def clearcache(self):
        try:
            self.D.Clear()
            return True
        except:
            return False
        
    def ctrl(self,Putton):
        if Putton == "PLAY":
            self.__Send(self.A.PB_Play_Bag)
        if Putton == "STOP":
            self.__Send(self.A.PB_Stop_Bag)
        
    def preview(self,flag=True):
        if flag == True:
            self.DispalyFlag = True
        else:
            self.DispalyFlag = False
            
    def __Send(self,bytesstream):
        try:
            self.SendLock.acquire()
            self.s.write(bytesstream)
            self.SendLock.release()
        except:
            self.SendLock.release()
            self.running = False
            self.timer.cancel()
            self.Q.put(["Error","Loss conenect"],False)
        
    def run(self):
        self.timer.start()
        while not self.exit:
            while self.running:
                try:
                    if self.s.in_waiting > 0:
                        data = self.s.read()
                        if data:
                            Mes = self.A.Rece(data)
                            if not Mes is None:
                                if Mes["Event"] == 0xC0:
                                    self.__GetState(Mes)
                                if Mes["Event"] == 0xC1:
                                    self.__GetTime(Mes)
                                if Mes["Event"] == 0xB0:
                                    self.__GetData(Mes)
                    #else:time.sleep(0.005)
                except Exception as e:
                    self.running = False
                    self.exit    = True
                    self.timer.cancel()
                    self.Q.put(["Error","Core cursh",str(e)],False)
            self.timer.cancel()
        self.s.close()
        self.Q.put(["Message","Core halt"],False)        
   
# if __name__ == "__main__":
#     C = Core()
#     Q = C.getQueue()
#     Database = C.exportcache()
#     for i,e in enumerate(Database):
#         ExportToFile("test-%d.txt"%(i),Database[i])
            
# if __name__ == "__main__":
#     from matplotlib import pyplot as plt
#     C = Core()
#     Q = C.getQueue()
#     C.init()
#     C.start()
#     C.preview()
#     PN = 0
#     DX0,DY0,DX1,DY1 = [],[],[],[] 
#     c  = 0
#     while True:
#         Mes = Q.get()
#         EVENT = Mes[0]
#         if EVENT == "Sample":
#             print(">>>",Mes[1:])
#         if EVENT == "Message":
#             print(">>>",Mes[1:])
#         if EVENT == "Data":
#             c   += 1
#             Data = Mes[2]
#             DX0 += Data[0::4]
#             DY0 += Data[1::4]
#             DX1 += Data[2::4]
#             DY1 += Data[3::4]
#             PN  += 1
#             if PN <= 100:
#                 PN += 1
#             else:
#                 DX0 = DX0[3:]
#                 DY0 = DY0[3:]
#                 DX1 = DX1[3:]
#                 DY1 = DY1[3:]
#             if c >= 4:  
#                 c = 0
#                 plt.pause(0.001)
#                 plt.subplot(4,1,1)
#                 plt.cla()
#                 plt.plot(DX0)
#                 plt.subplot(4,1,2)
#                 plt.cla()
#                 plt.plot(DY0)
#                 plt.subplot(4,1,3)
#                 plt.cla()
#                 plt.plot(DX1)
#                 plt.subplot(4,1,4)
#                 plt.cla()
#                 plt.plot(DY1)
#                 plt.draw()
