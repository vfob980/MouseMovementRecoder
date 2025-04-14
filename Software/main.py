from GUI import Main_Dialog as Main_Windows
import sys
from PyQt5.QtWidgets import QApplication
app          = QApplication(sys.argv)
Window       = Main_Windows()
Window.show()
exit(app.exec_())