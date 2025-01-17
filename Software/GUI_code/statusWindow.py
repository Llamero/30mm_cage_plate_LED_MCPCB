import math
import re
import struct
import sys

from PyQt5 import QtGui, QtCore, QtWidgets, uic
from PyQt5.QtGui import QFont
import qdarkstyle  # This awesome style sheet was made by Colin Duquesnoy and Daniel Cosmo Pizetta - https://github.com/ColinDuquesnoy/QDarkStyleSheet
from collections import OrderedDict, deque
import os
import pyqtgraph as pg
import guiMapper
import guiSequence as seq
import guiConfigIO as fileIO
import calibrationPlot as plot
import driverUSB
import time
import copy

N_MEASUREMENTS = 50 #Number of measurements per plot
MIN_TEMP_RANGE = 6 #Number of degrees at maximum zoom on the temperature plot
PLOT_PADDING = 1.1 #Factor of dark space above and below plot line so that plot line doesn't touch top of widget
debug = False

class statusWindow(QtWidgets.QWidget):
    status_signal = QtCore.pyqtSignal(object)  # Need to initialize outside of init() https://stackoverflow.com/questions/2970312/pyqt4-qtcore-pyqtsignal-object-has-no-attribute-connect

    def __init__(self, app, main_window):
        self.app = app
        self.gui = main_window
        super(statusWindow, self).__init__()
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        self.window_closed = False

        # Set look and feel
        uic.loadUi(self.gui.resourcePath('Status_GUI.ui'), self)
        if self.gui.menu_view_skins_dark.isChecked(): #Set dark skin if in dark mode since skin is reverted when window is opened.
            self.app.setStyleSheet(qdarkstyle.load_stylesheet_pyqt5())
        else:
            self.app.setStyleSheet("")
        self.app.setFont(QFont("MS Shell Dlg 2", 12))
        self.x_axis_offset = 0 #Current x-axis offset to use for rastering the plots in time

        #Set signals
        self.status_emit = self.status_signal.emit #Initialize instance of function so it can be explicitly disconnected later
        self.gui.status_signal.connect(self.status_emit)  #Connect mainWindow status signal to dialog status signal
        self.status_signal.connect(self.updateStatus) #Update status when new status signal is received
        self.plot_timeline = guiMapper.TimeLine(loopCount=0, interval=100) #Animation object for animating plots

        #Initialize plot data
        self.status_dict = copy.deepcopy(self.gui.status_dict)
        self.status_dict["Count"] = 0 #Add count element to dictionary
        self.plots = OrderedDict([("PWM", self.graph_intensity_pwm), ("Current", self.graph_intensity_current),
                                  ("Transistor", self.graph_temperature_transistor), ("Resistor", self.graph_temperature_resistor), ("External", self.graph_temperature_external)])

        self.y_values = OrderedDict([("PWM", deque([0]*N_MEASUREMENTS)), ("Current", deque([0]*N_MEASUREMENTS)),
                                  ("Transistor", deque([-1000]*N_MEASUREMENTS)), ("Resistor", deque([-1000]*N_MEASUREMENTS)), ("External", deque([-1000]*N_MEASUREMENTS))])

        self.state_dict = self.gui.state_dict
        self.speed_model, self.custom_spinbox = self.initializeSpeedModel()
        for key, value in self.plots.items():
            self.initializePlot(value, key)
        self.startAnimation()
        self.changeSpeed() #initialize update speed to default value

    def initializePlot(self, status_plot, key):
        # Set look and feel
        default_font = QtGui.QFontInfo(self.gui.app.font())
        pg.setConfigOption('background', 'k')
        pg.setConfigOption('foreground', '#999')
        pg.setConfigOptions(antialias=True)
        label_style = {"color": "#999", "font-size": str(default_font.pointSize()) + "pt",
                       "font-style": str(default_font.family())}

        if key == "PWM":
            status_plot.setLabel('left', "(% Duty Cycle)", **label_style)
        elif key == "Current":
            status_plot.setLabel('left', "(% LED Current Limit)", **label_style)
        else:
            status_plot.setLabel('left', "", "°C", **label_style)
            status_plot.setYRange(21 - MIN_TEMP_RANGE / 2, 21 + MIN_TEMP_RANGE / 2, padding=0)
        status_plot.setLabel('bottom', "", **label_style)

        # Use invisible axes to add margins to plot
        status_plot.showAxis("top")
        status_plot.getAxis('top').setPen('k', width=2)
        status_plot.getAxis('top').setTextPen('k', width=2)
        status_plot.showAxis("right")
        status_plot.getAxis('right').setPen('k', width=2)
        status_plot.getAxis('right').setTextPen('k', width=2)

        # Set
        status_plot.setXRange(0, N_MEASUREMENTS, padding=0)
        status_plot.getAxis('bottom').setTickSpacing(N_MEASUREMENTS/10, N_MEASUREMENTS/10)
        status_plot.getAxis('bottom').setStyle(showValues = False)
        status_plot.getAxis('bottom').setGrid(150)
        status_plot.getAxis('left').setGrid(150)

    def initializeSpeedModel(self):
        speed_model = OrderedDict()
        for speed in ["fast", "normal", "slow", "custom"]:
            speed_model[speed] = eval("self.graph_" + speed + "_update_button")
            speed_model[speed].clicked.connect(self.changeSpeed)

        custom_spinbox = self.graph_custom_update_spinbox
        custom_spinbox.valueChanged.connect(self.changeSpeed)

        return speed_model, custom_spinbox

    def activeCurrent(self):
        # Get current limit of active LED
        for led_number in range(1, 5):
            widget = eval("self.gui.main_channel_LED" + str(led_number) + "_button")
            if self.gui.getValue(widget):
                return self.gui.getValue(self.gui.config_model["LED" + str(led_number)]["Current Limit"]) * self.gui.getValue(
                    self.gui.calibration_current_box) / 100  # Return current limit * scale
        else:
            return None

    def setCalibrationScale(self):
        current = self.activeCurrent()
        self.gui.calibration_plot_window.setYRange(0, current * 1.2, padding=0)
        xlim = self.gui.calibration_plot_window.getViewBox().state["targetRange"][0][1]
        x = [0, xlim]
        y = [current, current]

    def startAnimation(self):
        self.plot_timeline.setFrameRange(0, 100)
        self.plot_timeline.frameChanged.connect(lambda: self.updateStatusWindow())
        self.plot_timeline.start()

    def stopAnimation(self):
        self.plot_timeline.stop()

    @QtCore.pyqtSlot()
    def changeSpeed(self):
        source_widget = self.sender()
        if not (isinstance(source_widget, QtWidgets.QDoubleSpinBox) or  isinstance(source_widget, QtWidgets.QRadioButton)): #If the GUI has just been initialized, find currently selected radiobutton
            for button in self.speed_model.values():
                if button.isChecked():
                    source_widget = button

        # If trigger was spinbox and "custom" radiobutton is checked, update speed from spinbox
        if isinstance(source_widget, QtWidgets.QDoubleSpinBox) and self.speed_model["custom"].isChecked():
            hertz = self.gui.getValue(self.custom_spinbox)

        elif isinstance(source_widget, QtWidgets.QRadioButton):
            if "custom" in str(source_widget.objectName()): #get speed from spinbox if custom radiobutton is checked
                hertz = self.gui.getValue(self.custom_spinbox)
            else: #Get the speed listed in the button text
                numeric_const_pattern = '[-+]? (?: (?: \d* \. \d+ ) | (?: \d+ \.? ) )(?: [Ee] [+-]? \d+ ) ?' #https://stackoverflow.com/questions/4703390/how-to-extract-a-floating-number-from-a-string
                rx = re.compile(numeric_const_pattern, re.VERBOSE)
                hertz = float(rx.findall(source_widget.text())[0])

        period = 1/hertz
        self.plot_timeline.setInterval(period*1000)

    def updateStatus(self, status):
        if debug:
            print("Recv: " + str(status))
        count = self.status_dict["Count"]
        for index, key in enumerate(status):
            if key in ["Channel", "Mode", "Control", "State"] or count == 0:
                self.status_dict[key] = status[key]
            else: #Calculate running average of measured values per update
                self.status_dict[key] += status[key]
        self.status_dict["Count"] += 1

    def updateStatusWindow(self):
        round_to_n = lambda x, n: x if x == 0 else round(x, -int(math.floor(math.log10(abs(x)))) + (n - 1)) #Roudn to sig fig - https://stackoverflow.com/questions/3410976/how-to-round-a-number-to-significant-figures-in-python
        count = self.status_dict["Count"]
        if count > 0: #Update values if at least one new update was received
            for key, value in self.status_dict.items():
                unit = ""
                if key == "Channel":
                    value += 1
                    self.updateLabel(key, value)
                    key = "Channel Name"
                    value = self.gui.getValue(self.gui.config_model["LED" + str(value)]["ID"])
                elif key in ["Transistor", "Resistor", "External"]:
                    if key == "External":
                        self.status_dict[key] = fileIO.adcToTemp(value/count, True) #Use external thermistor coefficients
                    else:
                        self.status_dict[key] = fileIO.adcToTemp(value / count, False) #Use internal thermistor coefficients
                    if self.status_dict[key] > -30:
                        value = round_to_n(self.status_dict[key], 3)
                        unit = " °C"
                    else:
                        self.status_dict[key] = -1000
                        value = "Not Connected"
                elif key in ["Driver Fan", "External Fan", "PWM"]:
                    self.status_dict[key] = ((value / count)/65535)*100
                    value = round_to_n(self.status_dict[key], 3)
                    unit = " %"
                elif key == "Current":
                    self.status_dict[key] = ((value / count) / self.gui.getAdcCurrentLimit(self.status_dict["Channel"])) * 100
                    value = round_to_n(self.status_dict[key], 3)
                    unit = " %"
                elif key == "Control":
                    value = self.gui.main_model["Control"][int(value)].text()
                elif key == "Mode":
                    if self.status_dict[key] == 0:
                        value = "Sync - " + self.gui.sync_model["Mode"].whatsThis()
                    elif self.status_dict[key] in [1, 2]:
                        value = "Manual"
                    else:
                        value = "Off"
                elif key == "State":
                    try:
                        if self.status_dict["Mode"] == 0:
                            value = self.state_dict[self.gui.sync_model["Mode"].whatsThis()][self.status_dict[key]]
                        elif self.status_dict["Mode"] == 1:
                            value = "PWM"
                        elif self.status_dict["Mode"] == 2:
                            value = "Current"
                        else:
                            value = "Off"
                    except KeyError:
                        value = "Loading..."

                if key not in ["Count"]:
                    self.updateLabel(key, value, unit)
            self.status_dict["Count"] = 0 #Reset the averaging counter

        #Update plots
        show_plot = self.isVisible() and self.gui.getValue(self.main_tab) in ["Intensity Plots", "Temperature Plots"]
        self.x_axis_offset += 1
        for key, status_plot in self.plots.items():
            if self.x_axis_offset == N_MEASUREMENTS:
                self.x_axis_offset = 0
            x_values = list(range(self.x_axis_offset, self.x_axis_offset + N_MEASUREMENTS))
            if show_plot:
                status_plot.setXRange(self.x_axis_offset, N_MEASUREMENTS+self.x_axis_offset, padding=0)

            if key in ["Transistor", "Resistor", "External"]:
                self.y_values[key][0] = self.status_dict[key]
                self.y_values[key].rotate(-1)
                if show_plot:
                    y_list = list(self.y_values[key])
                    try:
                        list_max = max(y for y in y_list if y > -273.15)  # Exclude None: https://stackoverflow.com/questions/2295461/list-minimum-in-python-with-none
                        list_min = min(y for y in y_list if y > -273.15)
                        y_mean = (list_max + list_min) / 2
                        y_range = list_max - list_min
                        if y_range < MIN_TEMP_RANGE * PLOT_PADDING:
                            y_range = MIN_TEMP_RANGE * PLOT_PADDING
                        status_plot.setYRange(y_mean - y_range / 2, y_mean + y_range / 2, padding=0)
                    except ValueError:
                        pass
                    status_plot.plot(x_values, y_list, pen=pg.mkPen('g', width=1), connect="finite", clear=True)

            else:
                self.y_values[key][0] = self.status_dict[key]
                self.y_values[key].rotate(-1)
                if show_plot:
                    y_list = list(self.y_values[key])
                    status_plot.setYRange(0, max(y_list)*PLOT_PADDING, padding=0)
                    status_plot.plot(x_values, y_list, pen=pg.mkPen('g', width=1), connect="finite", clear=True)

    def updateLabel(self, key, value, unit = ""):
        widget = key.lower()
        widget = eval("self.text_" + widget.replace(" ", "_") + "_label")
        widget.setText(key + ": " + str(value) + unit)

    def closeEvent(self, event):
        self.stopAnimation()

        #Disconnect class instance from MainWindow signals
        self.gui.status_signal.disconnect(self.status_emit)

        #Disconnect internal signals
        self.status_signal.disconnect()  # Connect mainWindow status signal to dialog status signal

        #Explicity delete timeline
        self.plot_timeline.deleteLater()
        # Change window closed flag
        self.window_closed = True

    def windowClosed(self):
        return self.window_closed

    def showMessage(self, text):
        self.gui.waitCursor(False)
        self.gui.stopSplash()
        self.gui.message_box.setText(text)
        self.gui.message_box.exec()




