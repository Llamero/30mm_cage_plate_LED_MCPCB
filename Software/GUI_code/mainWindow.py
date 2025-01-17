import math

from PyQt5 import QtGui, QtCore, QtWidgets, uic
from PyQt5.QtGui import QFont
import qdarkstyle  # This awesome style sheet was made by Colin Duquesnoy and Daniel Cosmo Pizetta - https://github.com/ColinDuquesnoy/QDarkStyleSheet
from collections import OrderedDict
import os
import pyqtgraph as pg
import guiMapper
import guiSequence as seq
import guiConfigIO as fileIO
import calibrationPlot as plot
import driverUSB
import statusWindow
import sys
from timeit import default_timer as timer
import pickle
import syncPlotWindow

DIAL_UPDATE_RATE = 0.05 #Time in s between updates from dial when in manual control - prevents dial from locking GUI with continuous updates when dial is swept
ANALOG_SYNC_SAMPLE_RATE = 10 #Time interval in microseconds for the analog sync to take a single sample
if hasattr(QtCore.Qt, 'AA_EnableHighDpiScaling'):
    QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_EnableHighDpiScaling, True)

if hasattr(QtCore.Qt, 'AA_UseHighDpiPixmaps'):
    QtWidgets.QApplication.setAttribute(QtCore.Qt.AA_UseHighDpiPixmaps, True)

class Ui(QtWidgets.QMainWindow):
    status_signal = QtCore.pyqtSignal(object) #Need to initialize outside of init() https://stackoverflow.com/questions/2970312/pyqt4-qtcore-pyqtsignal-object-has-no-attribute-connect
    sync_update_signal = QtCore.pyqtSignal(object)  # Need to initialize outside of init() https://stackoverflow.com/questions/2970312/pyqt4-qtcore-pyqtsignal-object-has-no-attribute-connect

    def __init__(self, app):
        self.app = app
        super(Ui, self).__init__()

        #Initialize splash screen to show on load
        self.splash_dict = {"main": self.resourcePath("Four Channel MHz LED Driver-main.png"),
                            "upload": self.resourcePath("Four Channel MHz LED Driver-upload.png"),
                            "download": self.resourcePath("Four Channel MHz LED Driver-download.png")}
        self.startup = True #Flag that program is in startup
        self.splash = QtWidgets.QSplashScreen(QtGui.QPixmap(self.splash_dict["main"]))
        self.splash.setFont(QFont('Arial', 15))
        self.splash.show()
        self.splash.showMessage("Loading program...", alignment=QtCore.Qt.AlignBottom, color=QtCore.Qt.white)

        # Set look and feel
        uic.loadUi(self.resourcePath('QtDesigner_GUI.ui'), self)
        self.gui_state_file = self.resourcePath('gui_state.obj')
        self.gui_state_dict = OrderedDict([("skin", "light"), ("lock", OrderedDict([("sync", False), ("config", False), ("gui", False)]))])

        #Initialize message box
        self.message_box = QtWidgets.QMessageBox() # https://pythonbasics.org/pyqt-qmessagebox/
        self.message_box.setIcon(QtWidgets.QMessageBox.Warning)

        #Initialize signal that is emitted when a new status packet is received
        self.status_signal.connect(self.updateMain)

        # Map gui widgets to ordered dictionaries
        self.splash.showMessage("Initializing models...", alignment=QtCore.Qt.AlignBottom, color=QtCore.Qt.white)
        self.config_model = guiMapper.initializeConfigModel(self)
        self.sync_model = guiMapper.initializeSyncModel(self)
        self.main_model = guiMapper.initializeMainModel(self)
        self.intensity_delay_timer = timer() #Timer for delaying updates from intensity dial

        # Initialize status dictionaries
        self.status_dynamic_dict = OrderedDict([("Channel", 0),
                                    ("PWM", 0),
                                    ("Current", 0),
                                    ("Mode", 0),
                                    ("State", 0),
                                    ("Control", 0),
                                    ("Transistor", 0),
                                    ("Resistor", 0),
                                    ("External", 0),
                                    ("Driver Fan", 0),
                                    ("External Fan", 0)])
        self.status_dict = OrderedDict(list(self.status_dynamic_dict.items()) + [("Name", 0),
                                                           ("COM Port", 0),
                                                           ("Serial", 0),
                                                           ("Control", 0)])
        self.status_window_list = []
        self.state_dict = OrderedDict(
            [("Digital", ["LOW", "HIGH"]), ("Analog", ["Active", "Active"]), ("Confocal", ["Standby", "Scanning"]),
             ("Serial", ["Active", "Active"]), ("Custom", ["Active", "Active"])])

        #Initialize seq dict and sync list for sync plot
        self.seq_dict = guiMapper.initializeSeqDictionary(self)
        self.sync_window_list = []

        # Hide dummy widgets
        for channel in range(1, 5):
            for button in range(4):
                if channel <= button:
                    self.config_model["Channel" + str(channel)][button].setVisible(False)

        # Assign events to widgets
        self.ser = driverUSB.usbSerial(self)
        guiMapper.initializeEvents(self)
        fileIO.checkCurrentLimits(self)
        plot.initializeCalibrationPlot(self)
        self.splash.showMessage("Searching for connected drivers...", alignment=QtCore.Qt.AlignBottom, color=QtCore.Qt.white)
        self.ser.getDriverPort(True)
        if self.splash.isVisible():
            self.splash.finish(self)
        self.startup = False #Set flag to exiting startup

        #Initialize the analog sync labels
        self.updateAnalogSync("current")
        self.updateAnalogSync("PWM")

        #Restore look and feel to previous session state
        self.initializeLookAndFeel()
        self.show()

    def initializeLookAndFeel(self):
        def checkDict(d_ref, d_test):
            for k, v in d_ref.items():
                if isinstance(v, OrderedDict):
                    if isinstance(d_test, OrderedDict):
                        checkDict(v, d_test[k])
                    else:
                        return d_ref
                else:
                    if type(v) != type(d_test[k]):
                        return d_ref
            else:
                return d_test

        try:
            with open(self.gui_state_file, "rb") as file:
                self.gui_state_dict = checkDict(self.gui_state_dict, pickle.load(file)) #Make sure file is valid

        except (EOFError, FileNotFoundError) as e: #Default to light skin if no made has been saved yet
            pass #Use default dictionary
        self.toggleSkin(self.gui_state_dict["skin"])
        for key in self.gui_state_dict["lock"]:
            self.lockInterface(key, True)


    def getValue(self, widget):
        if isinstance(widget, QtWidgets.QLineEdit):
            return widget.text()
        elif isinstance(widget, QtWidgets.QRadioButton) or isinstance(widget, QtWidgets.QCheckBox) or isinstance(
                    widget, QtWidgets.QPushButton):
            return widget.isChecked()
        elif isinstance(widget, QtWidgets.QSpinBox) or isinstance(widget, QtWidgets.QDoubleSpinBox) or isinstance(
                widget, QtWidgets.QSlider) or isinstance(widget, QtWidgets.QDial):
            return widget.value()
        elif isinstance(widget, QtWidgets.QToolBox):
            return widget.itemText(widget.currentIndex())
        elif isinstance(widget, list):
            if isinstance(widget[0], QtWidgets.QRadioButton) or isinstance(widget[0], QtWidgets.QPushButton):
                for element in widget:
                    if element.isChecked():
                        return element.text()

        elif isinstance(widget, QtWidgets.QTabWidget):
            return widget.tabText(widget.currentIndex())
        elif isinstance(widget, QtWidgets.QTableWidget):
            pass
        elif isinstance(widget, str) or widget is None:
            return str(widget)

    def setValue(self, widget, value):
        try:
            if isinstance(widget, QtWidgets.QLineEdit):
                widget.setText(str(value))
            elif isinstance(widget, QtWidgets.QRadioButton) or isinstance(widget, QtWidgets.QCheckBox) or isinstance(
                    widget, QtWidgets.QPushButton):
                widget.setChecked(bool(value))
            elif isinstance(widget, QtWidgets.QSpinBox) or isinstance(widget, QtWidgets.QDoubleSpinBox) or isinstance(
                    widget, QtWidgets.QSlider) or isinstance(widget, QtWidgets.QDial):
                widget.setValue(value)
            elif isinstance(widget, QtWidgets.QToolBox):
                for index in range(widget.count()):
                    if str(value) == widget.itemText(index):
                        widget.setCurrentIndex(index)
            elif isinstance(widget, list):
                if isinstance(widget[0], QtWidgets.QRadioButton) or isinstance(widget[0], QtWidgets.QPushButton):
                    for element in widget:
                        if element.text() == str(value):
                            element.setChecked(True)
            elif isinstance(widget, QtWidgets.QTabWidget):
                for index in range(widget.count()):
                    if value == widget.tabText(index):
                        widget.setCurrentIndex(index)
            elif isinstance(widget, QtWidgets.QTableWidget):
                pass
            elif isinstance(widget, str):
                seq.loadSequence()
        except ValueError:
            return False
        else:
            return True

    def createStatusWindow(self):
        #See if there is a closed instance to overwrite.  If not, create a new instance
        for index, instance in enumerate(self.status_window_list):
            if instance.windowClosed():
                self.status_window_list[index] = statusWindow.statusWindow(self.app, self)
                self.status_window_list[index].show()
                break
        else:
            self.status_window_list.append(statusWindow.statusWindow(self.app, self))
            self.status_window_list[-1].show()

    def createSyncPlotWindow(self):
        for index, instance in enumerate(self.sync_window_list):
            if instance.windowClosed():
                self.sync_window_list[index] = syncPlotWindow.syncPlotWindow(self.app, self)
                self.sync_window_list[index].show()
                break
        else:
            self.sync_window_list.append(syncPlotWindow.syncPlotWindow(self.app, self))
            self.sync_window_list[-1].show()

    def updateSerialNumber(self, serial_number):
        self.configure_name_driver_serial_label2.setText(serial_number)
        self.main_model["Serial"].setText(serial_number)
        self.status_dict["Serial"] = serial_number

    def updateMain(self, status_dict):
        if self.getValue(self.main_model["Control"]) == "LED Driver": #If the LED driver is the input source, update GUI with driver status
            self.main_model["Channel"][status_dict["Channel"]].setChecked(True)
            if status_dict["Mode"] > 0:
                self.setValue(self.main_model["Mode"][0], 0) #Set slider to Manual
                status_list = list(status_dict.items())
                self.main_model["Mode"][status_dict["Mode"]].setChecked(True)
                try:
                    if status_dict["Mode"] in [1,2]: #Check if knob is for PWM or current
                        intensity = status_list[status_dict["Mode"]][1]
                    else:
                        intensity = 0
                    self.setValue(self.main_model["Intensity"], (intensity/self.getAdcCurrentLimit(status_dict["Channel"]))*self.main_model["Intensity"].maximum())
                except (OverflowError, ZeroDivisionError): #This can happen when initializing connection - so default intensity to 0
                    self.setValue(self.main_model["Intensity"], 0)

            else:
                self.setValue(self.main_model["Mode"][0], 1) #Set slider to Sync

    def updateAnalogSync(self, sync_type):
        avg_label = eval("self.sync_analog_output_" + sync_type + "_avg_num_label")
        freq_label = eval("self.sync_analog_output_" + sync_type + "_avg_bandwidth_label")

        if sync_type == "current":
            key = sync_type.capitalize()
        else:
            key = sync_type
        slider_value = self.getValue(self.sync_model["Analog"][key])

        n_avg = 2**slider_value;
        freq = 1/(n_avg * ANALOG_SYNC_SAMPLE_RATE * 1e-6)

        round_to_n = lambda x, n: x if x == 0 else round(x, -int(math.floor(math.log10(abs(x)))) + (n - 1))
        round_to_n(freq, 3)

        freq_units = " Hz"
        if freq >= 100:
            freq /= 1000
            freq_units = " kHz"

        avg_label.setText("# of samples per update: " + str(n_avg))
        freq_label.setText("LED update frequency: " + f"{freq:.3}" + freq_units)

    def syncDisableMain(self, sync_active): #Disable manual control widgets if the sync is active
        self.main_model["Intensity"].setEnabled(not sync_active)
        self.main_intensity_spinbox.setReadOnly(sync_active)
        for widget in self.main_model["Mode"][1:4]:
            widget.setEnabled(not sync_active)

        if sync_active:
            for led_widget in self.main_model["Channel"]:
                led_widget.setEnabled(False)

        if not sync_active:
            software_control = self.getValue(self.main_model["Control"]) == "Software"
            self.toggleSoftwareControl(software_control)
            if not self.status_dict["Control"]: #Restore LED channel widgets if software control
                for led_number in range(1,5):
                    self.toggleLedActive(led_number)


        self.ser.updateStatus()

    def toggleSkin(self, mode):
        dark_active = False
        if mode == "dark":
            self.app.setStyleSheet(qdarkstyle.load_stylesheet_pyqt5())
            dark_active = True
        else:
            self.app.setStyleSheet("")
        self.app.setFont(QFont("MS Shell Dlg 2", 8))

        #Toggle menu check marks
        self.menu_view_skins_dark.setChecked(dark_active)
        self.menu_view_skins_light.setChecked(not dark_active)

        #Save new GUI state
        self.gui_state_dict["skin"] = mode
        with open(self.gui_state_file, "wb") as file:
            pickle.dump(self.gui_state_dict, file)

    def lockInterface(self, key, force_toggle = False):
        menu_widget = eval("self.menu_view_lock_" + key)
        if key == "gui":
            widget_list = [self.gui_master_tab]
            index = 0
        elif key == "sync":
            widget_list = [self.sync_scroll_area, self.sync_output_box]
            index = 1
        else:
            widget_list = [self.configure_scroll_area]
            index = 2

        #Override check mark to stored state
        if force_toggle:
            menu_widget.setChecked(self.gui_state_dict["lock"][key])

        #Toggle enabled widgets
        self.gui_master_tab.setCurrentIndex(index)  # Jump to main page before locking interface
        for widget in widget_list:
            widget.setEnabled(not menu_widget.isChecked())
        self.gui_state_dict["lock"][key] = menu_widget.isChecked()

        # Save new GUI state
        with open(self.gui_state_file, "wb") as file:
            pickle.dump(self.gui_state_dict, file, protocol=pickle.HIGHEST_PROTOCOL)

    def changeDriverName(self, widget):
        name = self.getValue(widget)
        self.main_model["Name"].setText(str(name))
        self.status_dict["Name"] = name


    def toggleLedActive(self, led_number):
        led_state = self.getValue(self.config_model["LED" + str(led_number)]["Active"])
        widget_list = [self.config_model["LED" + str(led_number)]["ID"], self.config_model["LED" + str(led_number)]["Current Limit"]]
        for channel in range(1,5):
            widget_list.append(eval("self.configure_LED_merge_channel" + str(channel) + "_button" + str(led_number) + ""))
        widget_list.append(self.sync_model["Digital"]["High"]["LED"][led_number])
        widget_list.append(self.sync_model["Digital"]["Low"]["LED"][led_number])
        widget_list.append(self.sync_model["Analog"]["LED"][led_number])
        widget_list.append(self.sync_model["Confocal"]["Standby"]["LED"][led_number])
        widget_list.append(self.sync_model["Confocal"]["Scanning"]["LED"][led_number])
        widget_list.append(self.main_model["Channel"][led_number-1])
        for widget in widget_list:
            widget.setEnabled(led_state)

    def toggleResistorActive(self, resistor_number):
            resistor_state = self.getValue(self.config_model["Resistor" + str(resistor_number)]["Active"])
            self.config_model["Resistor" + str(resistor_number)]["Value"].setEnabled(resistor_state)
            fileIO.checkCurrentLimits(self)

    def changeLedName(self, led_number, widget):
        name = self.getValue(widget)
        widget_list = [self.main_model["Channel"][led_number-1],
                       self.sync_model["Digital"]["Low"]["LED"][led_number],
                       self.sync_model["Digital"]["High"]["LED"][led_number],
                       self.sync_model["Analog"]["LED"][led_number],
                       self.sync_model["Confocal"]["Standby"]["LED"][led_number],
                       self.sync_model["Confocal"]["Scanning"]["LED"][led_number],
                       eval("self.configure_current_limit_LED" + str(led_number) + "_label")]
        for channel in range(1,5):
            widget_list.append(self.config_model["Channel" + str(channel)][led_number-1])

        for widget in widget_list:
            widget.setText(str(name))

    def toggleScanMode(self):
        scan_mode = self.getValue(self.sync_model["Confocal"]["Delay"]["Mode"])
        if scan_mode == "Unidirectional":
            self.sync_confocal_delay3_label1.setText("3) Time from LED event to trigger reset:")
            self.sync_confocal_delay3_label1.setToolTip("Set the additional time required for line sync trigger to reset after the LED event.")
            self.sync_confocal_delay3_box.setToolTip("Set the additional time required for line sync trigger to reset after the LED event.")
            self.sync_confocal_delay3_label2.setToolTip("Set the additional time required for line sync trigger to reset after the LED event.")
        else:
            self.sync_confocal_delay3_label1.setText("3) Bidirectional: Time between LED events:")
            self.sync_confocal_delay3_label1.setToolTip("Set the time between the forward scan LED event window and the reverse scan LED event window.")
            self.sync_confocal_delay3_box.setToolTip("Set the time between the forward scan LED event window and the reverse scan LED event window.")
            self.sync_confocal_delay3_label2.setToolTip("Set the time between the forward scan LED event window and the reverse scan LED event window.")


    def syncDialAndSpinbox(self, widget_in, widget_out, force = False):
        time = timer()
        if self.intensity_delay_timer < time or force:
            self.intensity_delay_timer = time + DIAL_UPDATE_RATE
            if isinstance(widget_in, QtWidgets.QDial):
                value = float(self.getValue(widget_in)) * 100.0 / float(widget_in.maximum())
                out_value = self.getValue(widget_out)
            else:
                value = round(self.getValue(widget_in) * widget_out.maximum() / 100)
                out_value = self.getValue(widget_out)

            if value != out_value:
                self.setValue(widget_out, value)
                if self.getValue(self.main_model["Control"][0]):
                    self.ser.updateStatus()

    def toggleAnalogChannel(self, widget):
        name = self.getValue(widget)
        if name == "Unbuffered - Current":
            enable = False
        else:
            enable = True
        for channel in range(4):
            self.sync_model["Analog"]["Channel"][channel].setEnabled(enable)

    def disableUsedOutputs(self, channel, source):
        if source == "sync":
            widget_list = self.config_model["Fan"]["Channel"]
        else:
            widget_list = self.sync_model["Output"]

        for index, button in enumerate(widget_list):
            if index == channel and channel != 0:
                button.setEnabled(False)
            else:
                button.setEnabled(True)

    def toggleSoftwareControl(self, software_enable):
        self.main_model["Intensity"].setEnabled(software_enable)
        for widget in self.main_model["Mode"]:
            widget.setEnabled(software_enable)
        if self.status_dict["Mode"] != 0 and software_enable: #Enable channel widgets if software control and in manual mode
            for led_number in range(1, 5):
                self.toggleLedActive(led_number)
        else:
            for led_widget in self.main_model["Channel"]:
                led_widget.setEnabled(False)
        for widget in self.main_model["Channel"]:
            widget.setEnabled(software_enable)
        self.main_intensity_spinbox.setReadOnly(not software_enable)
        if not software_enable:
            self.main_model["Mode"][1].setChecked(True) #Force to PWM for safety when driver is in manual mode

    def verifyCell(self, item):
        seq.verifyCell(self, item.column(), item.row(), item.text(), item.tableWidget())

    def waitCursor(self, override_cursor = True): #https://stackoverflow.com/questions/8218900/how-can-i-change-the-cursor-shape-with-pyqt
        if override_cursor:
            QtWidgets.QApplication.setOverrideCursor(QtCore.Qt.WaitCursor)
        else:
            QtWidgets.QApplication.restoreOverrideCursor()

    def startSplash(self, image):
        if self.startup:
            self.splash.showMessage("Downloading LED driver configuration...", alignment=QtCore.Qt.AlignBottom, color=QtCore.Qt.white)
        else:
            if self.splash.isVisible(): #Close the current splash screen if one is active
                self.splash.close()
            self.splash = QtWidgets.QSplashScreen(QtGui.QPixmap(self.splash_dict[image]))
            self.splash.setFont(QFont('Arial', 10))
            self.splash.show()

    def stopSplash(self):
        if self.splash.isVisible():
            self.splash.close()

    def splashText(self, text):
        if self.splash.isVisible() and not self.startup:
            self.splash.showMessage(text, alignment=QtCore.Qt.AlignBottom, color=QtCore.Qt.white)

    def setAdcCurrentLimit(self, value_list):
        for led_number, value in enumerate(value_list):
            self.config_model["LED" + str(led_number+1)]["Current Limit"].setWhatsThis(str(value))

    def getAdcCurrentLimit(self, led_number):
        try:
            return int(self.config_model["LED" + str(led_number+1)]["Current Limit"].whatsThis())
        except ValueError:
            return 0.01

    # Define function to import external files when using PyInstaller - https://stackoverflow.com/questions/37888581/pyinstaller-ui-files-filenotfounderror-errno-2-no-such-file-or-directory/37920111#37920111
    def resourcePath(self, relative_path):
        #Get absolute path to resource, works for dev and for PyInstaller
        try:
            # PyInstaller creates a temp folder and stores path in _MEIPASS
            base_path = sys._MEIPASS
        except Exception:
            base_path = os.path.abspath(".")

        return os.path.join(base_path, relative_path)