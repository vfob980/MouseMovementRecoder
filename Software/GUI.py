from Ui_UI import *
from Core import Core as Core_cls
from Core import n_GetPort,ExportToFile
from PyQt5.QtWidgets import QDialog,QFileDialog,QMessageBox
from pyqtgraph import PlotWidget
from PyQt5.QtCore    import QThread, pyqtSignal
from time import strftime,localtime,time,gmtime
from queue import Queue

class QThread_MessageResponse(QThread):
    trigger = pyqtSignal(list)
    def __init__(self,Queue):
        super(QThread_MessageResponse, self).__init__()
        self.Queue = Queue
    def run(self):
        while(1):
            Mes = self.Queue.get(block=True)
            self.trigger.emit(Mes)
class Main_Dialog(QDialog,Ui_Dialog):
    def __init__(self,parent=None):
        super(Main_Dialog, self).__init__(parent)
        self.Q   = Queue()
        self.setupUi(self)
        self.C   = Core_cls(self.Q)
        #------------------------
        self.ResponseThread = QThread_MessageResponse(self.C.Q)
        self.ResponseThread.trigger.connect(self.__ResponseProcesser)
        self.ResponseThread.start()
        #------------------------
        self.S_Port_connect = False
        #------------------------
        self.S_Ctrl_connect = False
        #------------------------
        self.Figure_RX   =  PlotWidget(self)
        self.Figure_RY   =  PlotWidget(self)
        self.Figure_LX   =  PlotWidget(self)
        self.Figure_LY   =  PlotWidget(self)
        self.DX0.addWidget(self.Figure_LX)
        self.DY0.addWidget(self.Figure_LY)
        self.DX1.addWidget(self.Figure_RX)
        self.DY1.addWidget(self.Figure_RY)
        self.data_rx = self.data_ry = []
        self.data_lx = self.data_ly = []
        #------------------------
        self.DataBase       = []
        self.DataTarget     = 0
        #------------------------
        self.log            = ""
        #------------------------
        self.__PortState()
        self.__CtrlState()
        self.on_PB_recordrefersh_released()
        #------------------------
    def __ResponseProcesser(self,Mes):
        MesType = str(Mes[0])
        Event   = str(Mes[1])
        if MesType == "Message":
            if Event == "Port open":
                pass
            elif Event == "State updata":
                self.S_Ctrl_connect = True
                self.__CtrlState()

            elif Event == "Core halt":
                self.S_Ctrl_connect = False
                self.S_Port_connect = False
                self.__CtrlState()
                self.__PortState()
            elif Event == "Time updata":
                self.__TimeDisplay(int(Mes[2]))
        elif MesType == "Error":
            if Event in ["Loss conenect",\
                         "Core cursh"]:
                self.S_Ctrl_connect = False
                self.S_Port_connect = False
                self.__CtrlState()
                self.__PortState()
                self.__PerviewClear()
            elif Event == "Port busy":
                pass
            elif Event == "Device lost":
                self.S_Ctrl_connect = False
                self.__CtrlState()
                self.__PortState()
        elif MesType == "Data":
            self.__PerviewPush(Mes[2])
        elif MesType == "Sample":
            if Event == "rec start":
                self.__PerviewClear()
                self.__TimeClear()
            if Event == "rec pause":
                pass
            if Event == "rec end":
                self.on_PB_recordrefersh_released()
            if Event == "rec resume":
                pass
        if MesType in ["Message","Sample","Error"] and Event not in ["State updata","Time updata"]:
            self.log  += strftime("%H:%M:%S",localtime(time())) +\
                                ">>>" + MesType + "|" + Event + "\n"
            self.TB_log.setText(self.log) 
    #---------------------    
    def __PortState(self):
        if not self.S_Port_connect:
            #----------------------
            self.CB_port   .setEnabled(True)
            self.CB_port   .clear()
            self.portlist = n_GetPort()
            if len(self.portlist) == 0:
                self.CB_port   .setEnabled(False)
                self.PB_connect.setEnabled(False)
            else:
                self.CB_port   .addItems(list(map(str,self.portlist)))
                self.PB_connect.setEnabled(True)
            self.PB_portrefersh.setEnabled(True)
            self.PB_connect.setText("Connect")
            self.PB_portrefersh.setEnabled(True)
        else:
            #----------------------
            self.CB_port   .setEnabled(False)
            self.CB_port   .clear()
            self.PB_connect.setEnabled(True)
            self.PB_connect.setText("Disconnect")
            self.PB_portrefersh.setEnabled(False)        
    def on_PB_portrefersh_released(self):
        self.__PortState()       
    def on_PB_connect_released(self):
        if not self.S_Port_connect:
            CBPortIndex   = self.CB_port.currentIndex()
            CBPortName    = list(self.portlist[CBPortIndex])[0]
            self.C        = Core_cls(self.Q)
            Flag          = self.C.connect(CBPortName)
            self.C.preview()
            if Flag == True:
                self.S_Port_connect = True
        else:
            self.__PerviewClear()
            self.S_Port_connect = False
            self.S_Ctrl_connect = False
            self.C.stop()
        self.__PortState()
    #---------------------    
    def __CtrlState(self):
        if not self.S_Ctrl_connect:
            self.PB_play.setEnabled(False)
            self.PB_stop.setEnabled(False)   
            self.L_connectstate.setText("Disconnection")
            self.L_connectstate.setStyleSheet("color:black;")
            self.L_recordstate.setText("--------")
            self.L_recordstate.setStyleSheet("color:black;")
            self.L_leftsensor.setStyleSheet("color:black;")
            self.L_rightsensor.setStyleSheet("color:black;")
            self.PB_play.setText("▶")
            self.__PerviewClear()
            self.__TimeClear()
        else:
            self.PB_play.setEnabled(True)
            self.PB_stop.setEnabled(True)
            self.L_connectstate.setText("Connection")
            self.L_connectstate.setStyleSheet("color:green;")
            if    not self.C.STA_REC  :
                State = "Waiting" 
                self.PB_play.setText("▶")
                self.L_recordstate.setStyleSheet("color:black;")
                self.PB_recordclear.setEnabled(True)
            elif  not self.C.STA_PAUSE:
                State = "Recording"
                self.PB_play.setText("||")
                self.L_recordstate.setStyleSheet("color:green;")
                self.PB_recordclear.setEnabled(False)
            else:                      
                State = "Pause"
                self.PB_play.setText("▶")
                self.L_recordstate.setStyleSheet("color:red;")
                self.PB_recordclear.setEnabled(False)
            self.L_recordstate.setText(State)
            if self.C.AVA_DEV_0:
                self.L_leftsensor.setStyleSheet("color:green;")
            else:
                self.L_leftsensor.setStyleSheet("color:red;")
            if self.C.AVA_DEV_1:
                self.L_rightsensor.setStyleSheet("color:green;")
            else: 
                self.L_rightsensor.setStyleSheet("color:red;")
    def on_PB_play_released(self):
        self.C.ctrl("PLAY")
    def on_PB_stop_released(self):
        self.C.ctrl("STOP")
    def __TimeDisplay(self,Time):
        self.L_time.setText(strftime("%H:%M:%S", gmtime(Time)))
    def __TimeClear(self):
        self.L_time.setText("00:00:00")
    #--------------------- 
    def __PerviewPush(self,Data):
        X0,Y0  = Data[0::4],Data[1::4]
        X1,Y1  = Data[2::4],Data[3::4]
        self.data_rx += X0
        self.data_ry += Y0
        self.data_lx += X1
        self.data_ly += Y1
        if len(self.data_rx) > 120*3:
            self.data_rx = self.data_rx[3:]
            self.data_ry = self.data_ry[3:]
            self.data_lx = self.data_lx[3:]
            self.data_ly = self.data_ly[3:]
        self.Figure_RX.clear()
        self.Figure_LX.clear()
        self.Figure_RY.clear()
        self.Figure_LY.clear()
        self.Figure_RX.plot(self.data_rx)
        self.Figure_LX.plot(self.data_lx)
        self.Figure_RY.plot(self.data_ry)
        self.Figure_LY.plot(self.data_ly)
    def __PerviewClear(self):
        self.data_rx = self.data_ry = []
        self.data_lx = self.data_ly = []
        self.Figure_RX.clear()
        self.Figure_LX.clear()
        self.Figure_RY.clear()
        self.Figure_RY.clear()
    #---------------------
    def on_PB_recordrefersh_released(self):
        self.DataBase = self.C.exportcache()
        self.LW_rec.clear()
        for i,item in enumerate(self.DataBase):
            info = item[0]
            self.LW_rec.addItem("%d> "%(i)+info)
        self.LW_rec.itemClicked.connect(self.__clickItem)
        self.DataTarget = -1
        self.L_recordnum.setText("No:-")
    def on_PB_recordexport_released(self):
        if self.DataTarget != -1:
            name = self.DataBase[self.DataTarget][0].replace(" ","_").replace("/","-").replace(":","-")
            file_path, _ = QFileDialog.getSaveFileName(self,\
                            "Select the export directory","%s.txt"%(name),"*.*")
            if file_path:
                ExportToFile(file_path,self.DataBase[self.DataTarget])
    def on_PB_recordclear_released(self):
        reply = QMessageBox.question(self, 'Warnning', 'You sure to clear cache?\nAll data without export will be deleted!',
                                     QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
        if reply == QMessageBox.Yes:
            reply = QMessageBox.question(self, 'Warnning', 'Are you sure to delete all the data in cache?\nThis is the last time asked!',
                                QMessageBox.Yes | QMessageBox.No, QMessageBox.No)
            if reply == QMessageBox.Yes:
                self.C.clearcache()
                self.on_PB_recordrefersh_released()
    def __clickItem(self,item):
        self.DataTarget  = int(str(item.text()).split(">")[0])
        self.L_recordnum.setText("No:%d"%(self.DataTarget))
    #---------------------
    def on_PB_logclear_released(self):
        self.log            = ""
        self.TB_log.setText(self.log) 
    #---------------------
    
import sys
from PyQt5.QtWidgets import QApplication
app          = QApplication(sys.argv)
Window       = Main_Dialog()
Window.show()
exit(app.exec_())