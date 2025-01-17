from PyQt5 import QtWidgets
import sys
import mainWindow
import guiResources #https://stackoverflow.com/questions/37888581/pyinstaller-ui-files-filenotfounderror-errno-2-no-such-file-or-directory/37920111#37920111

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    window = mainWindow.Ui(app)
    app.exec_()
    window.ser.disconnectSerial() #Cleanly disconnect serial on close
    sys.exit()