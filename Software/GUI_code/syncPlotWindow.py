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
from timeit import default_timer as timer
import datetime

PLOT_PADDING = 1.1 #Factor of dark space above and below plot line so that plot line doesn't touch top of widget
SLEW_TIME = 1e-6 #Time for LED to switch between intensities
N_SAMPLES = 1000 #When plots exceed 2x this length, they will be binned back to this length
STARTING_PLOT_RATE = 50 #Time between plot updates at start of plot
debug = False

class syncPlotWindow(QtWidgets.QWidget):
    sync_update_signal = QtCore.pyqtSignal(object)  # Need to initialize outside of init() https://stackoverflow.com/questions/2970312/pyqt4-qtcore-pyqtsignal-object-has-no-attribute-connect
    status_signal = QtCore.pyqtSignal(object)  # Need to initialize outside of init() https://stackoverflow.com/questions/2970312/pyqt4-qtcore-pyqtsignal-object-has-no-attribute-connect

    def __init__(self, app, main_window):
        self.app = app
        self.gui = main_window
        super(syncPlotWindow, self).__init__()
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        self.window_closed = False

        # Set look and feel
        uic.loadUi(self.gui.resourcePath('Sync_Plot_GUI.ui'), self)

        #Connect signals
        self.status_emit = self.status_signal.emit #Function instance saved so it can later be disconnected explicitly
        self.update_emit = self.sync_update_signal.emit #Function instance saved so it can later be disconnected explicitly
        self.gui.sync_update_signal.connect(self.update_emit)
        self.sync_update_signal.connect(self.updateWindow)  # Update status when new status signal is received
        self.gui.status_signal.connect(self.status_emit)  # Connect mainWindow status signal to dialog status signal
        self.status_signal.connect(self.updateStatus) #Update status when new status signal is received
        self.plot_timeline = guiMapper.TimeLine(loopCount=0, interval=STARTING_PLOT_RATE)  # Animation object for animating plots

        # Set dark skin if in dark mode since skin is reverted when window is opened.
        if self.gui.menu_view_skins_dark.isChecked():
            self.app.setStyleSheet(qdarkstyle.load_stylesheet_pyqt5())
        else:
            self.app.setStyleSheet("")
        self.app.setFont(QFont("MS Shell Dlg 2", 12))

        #Initialize lists for storing plot data
        self.y_ref = [OrderedDict([("PWM", []), ("Current", []), ("Channel", [])]),
                         OrderedDict([("PWM", []), ("Current", []), ("Channel", [])])] #Y values of reference sequence line
        self.x_ref = [[], []] #X values of reference sequence line
        self.y_values = copy.deepcopy(self.y_ref)
        self.x_values = copy.deepcopy(self.x_ref)  # X values of reference sequence line
        self.status_dict = copy.deepcopy(self.gui.status_dict)
        self.resetStatus()
        self.seq_list = guiMapper.initializeSeqList(self.gui)
        self.seq_dict = self.gui.seq_dict
        self.mode = None #Save active sync mode string

        #Plot widget data
        self.plots = [OrderedDict([("PWM", self.graph_intensity_pwm0), ("Current", self.graph_intensity_current0), ("Channel", self.graph_channel0)]),
                      OrderedDict([("PWM", self.graph_intensity_pwm1), ("Current", self.graph_intensity_current1), ("Channel", self.graph_channel1)])]
        self.state_dict = OrderedDict([("Digital", ["LOW", "HIGH"]), ("Analog", ["Active", "Active"]), ("Confocal", ["Standby", "Scanning"]),
                                       ("Serial", ["Active", "Active"]), ("Custom", ["Active", "Active"])])
        self.hold_label = [self.hold_label0, self.hold_label1]

        #Timers for x-axis
        self.plot_interval = STARTING_PLOT_RATE #Time between plot updates in milliseconds
        self.plot_start_time = timer()  # Timer for tracking time points for x_axis
        self.hold_start_time = timer() # Timer for tracking duration of hold at end of sequence
        self.hold_status = [None, None] # State of plot hold (None = no hold, False = hold not yet reached, True = in hold)

        for plot_dict in self.plots:
            for key, value in plot_dict.items():
                self.initializePlot(value, key)
        self.updateWindow()

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
            status_plot.setLabel('left', "(LED Channel #)", **label_style)
            status_plot.setYRange(0, 4.2, padding=0)
            status_plot.getAxis('left').setTickSpacing(1, 1)

        # Use invisible axes to add margins to plot
        status_plot.showAxis("top")
        status_plot.getAxis('top').setPen('k', width=2)
        status_plot.getAxis('top').setTextPen('k', width=2)
        status_plot.showAxis("right")
        status_plot.getAxis('right').setStyle(showValues=False)

        #Set default axis labels
        status_plot.setLabel('bottom', "Time", "s", **label_style)
        status_plot.setLabel('right', "Hold", **label_style)

        # Set grid
        status_plot.getAxis('bottom').setGrid(150)
        status_plot.getAxis('left').setGrid(150)

    def updateWindow(self):
        def widgetIndex(widget_list):
            for w_index, n_widget in enumerate(widget_list):
                if self.gui.getValue(n_widget):
                    return w_index
            else:
                self.showMessage(self.gui, "Error: Widget index not found!")
                return None

        sync_model = self.gui.sync_model

        #Get the active sync mode
        self.mode = sync_model["Mode"].whatsThis()
        if not self.mode:
            self.mode = self.gui.getValue(sync_model["Mode"])
        if debug:
            print("Active mode: " + self.mode)
        self.main_tab.setTabText(0, self.mode + ": " + self.gui.state_dict[self.mode][0])
        self.main_tab.setTabText(1, self.mode + ": " + self.gui.state_dict[self.mode][1])

        if self.mode == "Digital":
            for index, trigger in enumerate(["Low", "High"]):
                self.main_tab.setTabEnabled(index, True)
                if sync_model["Digital"][trigger]["Mode"].whatsThis() == "LED Off":
                    self.x_ref[index] = [0]  # Set duration to hold
                    for key in self.y_ref[index]:
                        self.y_ref[index][key] = [0] # Set all intensities to 0, and channel is not changed (0)
                elif sync_model["Digital"][trigger]["Mode"].whatsThis() == "Constant Value":
                    duration = self.gui.getValue(sync_model["Digital"][trigger]["Duration"])
                    if duration == 0:
                        self.x_ref[index] = [0]
                    else:
                        self.x_ref[index] = [0, duration, duration + SLEW_TIME]
                    for key in self.y_ref[index]:
                        if key == "Channel":
                            model_key = "LED"
                        else:
                            model_key = key
                        if isinstance(sync_model["Digital"][trigger][model_key], list):
                            self.y_ref[index][key] = [widgetIndex(sync_model["Digital"][trigger][model_key])]
                            if duration > 0:
                                self.y_ref[index][key].extend([widgetIndex(sync_model["Digital"][trigger][model_key]), 0]) #Extend to hold at off to intensity profile if intensity is pulsed for a fixed duration
                        else:
                            self.y_ref[index][key] = [self.gui.getValue(sync_model["Digital"][trigger][model_key])]
                            if duration > 0:
                                self.y_ref[index][key].extend([self.gui.getValue(sync_model["Digital"][trigger][model_key]), 0]) #Extend to hold at off to intensity profile if intensity is pulsed for a fixed duration
                else:
                    seq_widget = self.seq_list[index]
                    n_rows = len(self.seq_dict[seq_widget]["LED #"])
                    elapsed_time = 0
                    if n_rows == 0: #If there isn't a sequence specified, default to holding the LED off
                        self.x_ref[index] = [0]  # Set duration to hold
                        for key in self.y_ref[index]:
                            self.y_ref[index][key] = [0]  # Set all intensities to 0, and channel is not changed (0)
                    else:
                        for key in self.y_ref[index]:
                            self.y_ref[index][key] = []  # Clear all lists
                        self.x_ref[index] = []

                        #If a hold isn't specified at the end of the sequence add a hold at off
                        if self.seq_dict[seq_widget]["Duration (s)"][n_rows-1] != 0:
                            for key in self.seq_dict[seq_widget]:
                                self.seq_dict[seq_widget][key].append(0)
                            n_rows += 1

                        for row in range(n_rows):
                            duration = float(self.seq_dict[seq_widget]["Duration (s)"][row])
                            self.x_ref[index].append(elapsed_time+SLEW_TIME)
                            list_length = 1 #If hold is reached, only add one more time point
                            if duration > 0: #If hold is not reached add two time points as sequence is a step function
                                self.x_ref[index].append(elapsed_time+duration)
                                list_length = 2
                            self.y_ref[index]["Channel"].extend([int(self.seq_dict[seq_widget]["LED #"][row])]*list_length)
                            self.y_ref[index]["PWM"].extend([float(self.seq_dict[seq_widget]["LED PWM (%)"][row])]*list_length)
                            self.y_ref[index]["Current"].extend([float(self.seq_dict[seq_widget]["LED current (%)"][row])]*list_length)
                            elapsed_time += duration
                            if duration == 0:
                                break
                        else:
                            self.showMessage("ERROR: Something went very wrong - a hold was not reached at the end of a sequence and this should be impossible.")

        elif self.mode == "Analog":
            #Disable second tab as there is only one analog state
            self.main_tab.setTabEnabled(0, True)
            self.main_tab.setTabEnabled(1, False)

            #Initialize plots to 0
            for index in range(2):
                self.x_ref[index] = [0]
                for key in self.y_ref[index]:
                    self.y_ref[index][key] = [0]

        elif self.mode == "Confocal":
            for index, trigger in enumerate(["Standby", "Scanning"]):
                self.main_tab.setTabEnabled(index, True)
                if sync_model["Confocal"][trigger]["Mode"].whatsThis() in ["LED Off", "External Analog"]:
                    self.x_ref[index] = [0]  # Set duration to hold
                    for key in self.y_ref[index]:
                        self.y_ref[index][key] = [0] # Set all intensities to 0, and channel is not changed (0)
                elif sync_model["Confocal"][trigger]["Mode"].whatsThis() == "Constant Value":
                    duration = self.gui.getValue(sync_model["Confocal"][trigger]["Duration"])
                    if duration == 0:
                        self.x_ref[index] = [0]
                    else:
                        self.x_ref[index] = [0, duration, duration + SLEW_TIME]
                    for key in self.y_ref[index]:
                        if key == "Channel":
                            key2 = "LED"
                        else:
                            key2 = key
                        if isinstance(sync_model["Confocal"][trigger][key2], list):
                            self.y_ref[index][key] = [widgetIndex(sync_model["Confocal"][trigger][key2])]
                            if duration > 0:
                                self.y_ref[index][key].extend([widgetIndex(sync_model["Confocal"][trigger][key2]), 0]) #Extend to hold at off to intensity profile if intensity is pulsed for a fixed duration
                        else:
                            self.y_ref[index][key] = [self.gui.getValue(sync_model["Confocal"][trigger][key2])]
                            if duration > 0:
                                self.y_ref[index][key].extend([self.gui.getValue(sync_model["Confocal"][trigger][key2]), 0]) #Extend to hold at off to intensity profile if intensity is pulsed for a fixed duration
                else:
                    seq_widget = self.seq_list[index+2]
                    n_rows = len(self.seq_dict[seq_widget]["LED #"])
                    elapsed_time = 0
                    if n_rows == 0: #If there isn't a sequence specified, default to holding the LED off
                        self.x_ref[index] = [0]  # Set duration to hold
                        for key in self.y_ref[index]:
                            self.y_ref[index][key] = [0]  # Set all intensities to 0, and channel is not changed (0)
                    else:
                        for key in self.y_ref[index]:
                            self.y_ref[index][key] = []  # Clear all lists
                        self.x_ref[index] = []

                        #If a hold isn't specified at the end of the sequence add a hold at off
                        if self.seq_dict[seq_widget]["Duration (s)"][n_rows-1] != 0:
                            for key in self.seq_dict[seq_widget]:
                                self.seq_dict[seq_widget][key].append(0)
                            n_rows += 1

                        for row in range(n_rows):
                            duration = float(self.seq_dict[seq_widget]["Duration (s)"][row])
                            self.x_ref[index].append(elapsed_time+SLEW_TIME)
                            list_length = 1 #If hold is reached, only add one more time point
                            if duration > 0: #If hold is not reached add two time points as sequence is a step function
                                self.x_ref[index].append(elapsed_time+duration)
                                list_length = 2
                            self.y_ref[index]["Channel"].extend([int(self.seq_dict[seq_widget]["LED #"][row])]*list_length)
                            self.y_ref[index]["PWM"].extend([float(self.seq_dict[seq_widget]["LED PWM (%)"][row])]*list_length)
                            self.y_ref[index]["Current"].extend([float(self.seq_dict[seq_widget]["LED current (%)"][row])]*list_length)
                            elapsed_time += duration
                            if duration == 0:
                                break
                        else:
                            self.showMessage("ERROR: Something went very wrong - a hold was not reached at the end of a sequence and this should be impossible.")

        elif self.mode == "Serial":
            #Disable second tab as there is only one analog state
            self.main_tab.setTabEnabled(0, True)
            self.main_tab.setTabEnabled(1, False)

            #Initialize plots to 0
            for index in range(2):
                self.x_ref[index] = [0]
                for key in self.y_ref[index]:
                    self.y_ref[index][key] = [0]

        elif self.mode == "Custom":
            #Disable second tab as there is only one analog state
            self.main_tab.setTabEnabled(0, True)
            self.main_tab.setTabEnabled(1, False)

            #Initialize plots to 0
            for index in range(2):
                self.x_ref[index] = [0]
                for key in self.y_ref[index]:
                    self.y_ref[index][key] = [0]

        else:
            print("Not implemented")

        #Update hold labels:
        self.resetHoldLabel(0)
        self.resetHoldLabel(1)
        self.resetHold()
        if debug:
            print("Hold status initialized: " + str(self.hold_status))

        #Plot reference lines
        for index in range(2):
            if self.main_tab.isTabEnabled(index):
                for key, seq_plot in self.plots[index].items():
                    seq_plot.plot(self.x_ref[index], self.y_ref[index][key], pen=pg.mkPen('m', width=1), clear=True)

    def binPlots(self):
        index = self.status_dict["State"]
        array_length = len(self.x_values[index])
        if array_length >= N_SAMPLES*2:
            for key in self.y_values[index]:
                self.y_values[index][key] = self.y_values[index][key][::2]
            self.x_values[index] = self.x_values[index][::2]

            #Halve the plotting rate to match new bin widths
            self.plot_interval *= 2
            self.plot_timeline.setInterval(self.plot_interval)
            if debug:
                print("sync interval: " + str(self.plot_interval))

    def startAnimation(self):
        self.plot_start_time = timer()  # Start timer for tracking time points for x_axis
        self.plot_interval = STARTING_PLOT_RATE
        self.plot_timeline.setInterval(self.plot_interval)
        self.plot_timeline.setFrameRange(0, 100)
        self.plot_timeline.frameChanged.connect(lambda: self.updateSyncPlot())
        self.plot_timeline.start()
        if debug:
            print("sync plot started")

    def stopAnimation(self):
        self.plot_timeline.stop()
        if debug:
            print("sync plot stopped")

    def updateStatus(self, status):
        count = self.status_dict["Count"]
        if (status["Mode"] == 0 and self.status_dict["Mode"] != 0) or status["State"] != self.status_dict["State"]: #If there was a change to sync active or sync state change: switch tab, reset plots, and reset animation
            self.main_tab.setCurrentIndex(status["State"]) #Switch to tab for active state
            self.stopAnimation()
            self.status_signal.disconnect()  #Stop status updates - the prevents extraneous updates being saved while plots are reset
            self.resetHold()
            self.resetHoldLabel(status["State"])  # Clear the plot arrays for the active state
            self.clearPlot(status["State"])  # Clear the plot arrays for the active state
            self.resetStatus()
            self.startAnimation()
            self.status_signal.connect(self.updateStatus)  # Update status when new status signal is received

        elif status["Mode"] != 0 and self.status_dict["Mode"] == 0: #If there was a change to sync inactive, stop plotting
            self.stopAnimation()
            self.resetHold()

        self.status_dict["Mode"] = status["Mode"] #Update mode

        if status["Mode"] == 0: #Update status if sync is active
            #Update y-values
            for index, key in enumerate(status):
                if key in ["Mode", "Control", "State"] or count == 0:
                    self.status_dict[key] = status[key]
                else:  # Calculate running average of measured values per update
                    self.status_dict[key] += status[key]

            # Update x-values
            current_time = timer() - self.plot_start_time
            if count == 0:
                self.status_dict["Time"] = current_time
            else:
                self.status_dict["Time"] += current_time

            #Check for hold time if hold is expected
            if self.hold_status[self.status_dict["State"]] is False: #Check if hold time reached if hold is expected (hold status is False)
                if current_time >= self.x_ref[status["State"]][-1]:  # If hold time has been reached, start hold timer
                    self.plot_timeline.setInterval(500) #Set interval to update hold label twice per second (double the highest granularity in hold label timer)
                    self.hold_start_time = current_time
                    self.hold_status[self.status_dict["State"]] = True

            self.status_dict["Count"] += 1

    def clearPlot(self, index):
        for key in self.y_values[index]:
            self.y_values[index][key] = []
        self.x_values[index] = []
    def resetStatus(self):
        self.status_dict["Count"] = 0  # Add count element to dictionary
        self.status_dict["Time"] = 0  # Add time element to dictionary

    def resetHold(self):
        for index in range(2):
            if self.mode in ["Analog", "Serial", "Custom"]:
                self.hold_status[index] = None
            else:
                self.hold_status[index] = False

    def resetHoldLabel(self, index):
        self.hold_label[index].setText("Hold at end: PWM = " + str(self.y_ref[index]["PWM"][-1]) + "%, Current = " + str(self.y_ref[index]["Current"][-1]) + "%, Channel = " + str(self.y_ref[index]["Channel"][-1]) + ".  Elapsed hold time: 00:00:00")

    def updateSyncPlot(self):
        show_plot = self.isVisible()
        state = self.status_dict["State"]
        count = self.status_dict["Count"]

        if count > 0: #Update values if at least one new update was received
            #Convert values
            self.status_dict["Channel"] /= count
            self.status_dict["Current"] = ((self.status_dict["Current"] / count) / self.gui.getAdcCurrentLimit(round(self.status_dict["Channel"]))) * 100
            self.status_dict["PWM"] = ((self.status_dict["PWM"] / count) / 65535) * 100
            self.status_dict["Channel"] += 1

            if self.hold_status[state] is not True: #Update plots if timer has not reached hold
                self.x_values[state].append(self.status_dict["Time"] / count) #Add current time to x values
                for key, status_plot in self.plots[state].items():
                    self.y_values[state][key].append(self.status_dict[key])
                    if show_plot:
                        if self.mode in ["Analog", "Serial", "Custom"]:
                            status_plot.plot(self.x_values[state][1:], self.y_values[state][key][1:], pen=pg.mkPen('g', width=1), connect="finite", clear=True)
                        else:
                            status_plot.plot(self.x_ref[state], self.y_ref[state][key], pen=pg.mkPen('m', width=1), connect="finite", clear=True)
                            status_plot.plot(self.x_values[state][1:], self.y_values[state][key][1:], pen=pg.mkPen('g', width=1), connect="finite", clear=False)
                self.binPlots()  # Check if plots need to be down sampled

            else: #If timer has reached hold, update hold label instead
                round_to_n = lambda x, n: x if x == 0 else round(x, -int(math.floor(math.log10(abs(x)))) + (n - 1))  # Round to sig fig - https://stackoverflow.com/questions/3410976/how-to-round-a-number-to-significant-figures-in-python
                hold_seconds = round((self.status_dict["Time"]/count)-self.hold_start_time)
                for key, status_plot in self.plots[state].items():
                    self.y_values[state][key].append(self.status_dict[key]/count)
                self.hold_label[state].setText("Hold at end: PWM = " + str(round_to_n(self.status_dict["PWM"], 3)) +
                                           "%, Current = " + str(round_to_n(self.status_dict["Current"], 3)) +
                                           "%, Channel = " + str(round(self.status_dict["Channel"])) +
                                           ".  Elapsed hold time: " + str(datetime.timedelta(seconds = hold_seconds)))

            self.status_dict["Count"] = 0  # Reset the averaging counter

    def closeEvent(self, event):
        self.stopAnimation()

        #Disconnect class instance from MainWindow signals
        self.gui.sync_update_signal.disconnect(self.update_emit)
        self.gui.status_signal.disconnect(self.status_emit)

        #Disconnect internal signals
        self.sync_update_signal.disconnect()  # Update status when new status signal is received
        self.status_signal.disconnect()  # Connect mainWindow status signal to dialog status signal

        #Explicity delete timeline
        self.plot_timeline.deleteLater()

        #Change window closed flag
        self.window_closed = True

    def windowClosed(self):
        return self.window_closed

    def showMessage(self, text):
        self.gui.waitCursor(False)
        self.gui.stopSplash()
        self.gui.message_box.setText(text)
        self.gui.message_box.exec()




