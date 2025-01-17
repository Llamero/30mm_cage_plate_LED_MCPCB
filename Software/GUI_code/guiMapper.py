from collections import OrderedDict
import guiSequence as seq
import guiConfigIO as fileIO
import calibrationPlot as plot
from PyQt5 import QtGui, QtCore

def initializeConfigModel(gui):
    config_model = OrderedDict()
    config_model["Driver name"] = gui.configure_name_driver_line_edit
    # LEDs
    for led_number in range(1, 5):
        config_model["LED" + str(led_number)] = OrderedDict(
            [("ID", eval("gui.configure_name_LED" + str(led_number) + "_line_edit")),
             ("Active", eval("gui.configure_name_LED" + str(led_number) + "_box")),
             ("Current Limit", eval("gui.configure_current_limit_LED" + str(led_number) + "_spin_box"))])

    #Channels
    for channel in range(1,5):
        config_model["Channel" + str(channel)] = []
        for led_number in range(1,5):
            config_model["Channel" + str(channel)].append(eval("gui.configure_LED_merge_channel" + str(channel) + "_button" + str(led_number)))

    # Current sense resistors
    for resistor in range(1, 5):
        config_model["Resistor" + str(resistor)] = OrderedDict(
            [("Value", eval("gui.configure_resistor" + str(resistor) + "_spin_box")),
             ("Active", eval("gui.configure_resistor" + str(resistor) + "_box"))])

    # Temperature cutoffs
    config_model["Temperature"] = OrderedDict()
    for source in ["Transistor", "Resistor", "External"]:
        config_model["Temperature"][source] = OrderedDict(
            [("Warn", eval("gui.configure_temperature_" + source.lower() + "_warn_box")),
             ("Fault", eval("gui.configure_temperature_" + source.lower() + "_fault_box"))])

    # Fan settings
    config_model["Fan"] = OrderedDict()
    for source in ["Driver", "External"]:
        config_model["Fan"][source] = OrderedDict(
            [("Min", eval("gui.configure_fan_" + source.lower() + "_min_box")),
             ("Max", eval("gui.configure_fan_" + source.lower() + "_max_box"))])
    config_model["Fan"]["Channel"] = []
    for channel in range(4):
        config_model["Fan"]["Channel"].append(eval("gui.configure_fan_external_output" + str(channel) + "_box"))

    # External Thermistor
    config_model["Thermistor"] = OrderedDict(
        [("Resistance", gui.configure_thermistor_resistance_box), ("Beta", gui.configure_thermistor_beta_box)])

    # Audio settings
    config_model["Audio"] = OrderedDict(
        [("Status", gui.configure_audio_status_slider), ("Alarm", gui.configure_audio_alarm_slider)])

    # Pushbuttons
    config_model["Pushbutton"] = OrderedDict([("Indication", [gui.configure_pushbutton_intensity_off_button, gui.configure_pushbutton_intensity_on_button]),
                                                   ("Alarm", [gui.configure_pushbutton_alarm_disable_button,
                                                              gui.configure_pushbutton_alarm_flash_button,
                                                              gui.configure_pushbutton_alarm_chase_button,
                                                              gui.configure_pushbutton_alarm_solid_button])])

    return config_model

def initializeSyncModel(gui):
    def initializeDigital():
        nonlocal gui
        sync_model["Digital"] = OrderedDict()
        sync_model["Digital"]["Channel"] = []
        for channel_number in range(1, 5):
            sync_model["Digital"]["Channel"].append(eval("gui.sync_digital_input" + str(channel_number) + "_button"))
        for trigger in ["Low", "High"]:
            sync_model["Digital"][trigger] = OrderedDict()
            sync_model["Digital"][trigger]["Mode"] = eval("gui.sync_digital_trigger_" + trigger.lower() + "_tab")
            sync_model["Digital"][trigger]["LED"] = []
            for led_number in range(5):
                sync_model["Digital"][trigger]["LED"].append(eval("gui.sync_digital_trigger_" + trigger.lower() + "_constant_LED" + str(led_number) + "_button"))
            sync_model["Digital"][trigger]["PWM"] = eval("gui.sync_digital_trigger_" + trigger.lower() + "_constant_config_PWM_box")
            sync_model["Digital"][trigger]["Current"] = eval("gui.sync_digital_trigger_" + trigger.lower() + "_constant_config_current_box")
            sync_model["Digital"][trigger]["Duration"] = eval("gui.sync_digital_trigger_" + trigger.lower() + "_constant_config_duration_box")
            sync_model["Digital"][trigger]["Sequence"] = ""

    def initializeAnalog():
        nonlocal gui
        sync_model["Analog"] = OrderedDict()
        sync_model["Analog"]["LED"] = []
        for led_number in range(5):
            sync_model["Analog"]["LED"].append(eval("gui.sync_analog_LED" + str(led_number) + "_button"))
        sync_model["Analog"]["Channel"] = []
        for channel_number in range(1, 5):
            sync_model["Analog"]["Channel"].append(eval("gui.sync_analog_input" + str(channel_number) + "_button"))
        sync_model["Analog"]["Mode"] = gui.sync_analog_output_tab
        sync_model["Analog"]["PWM"] = gui.sync_analog_output_PWM_avg_slider
        sync_model["Analog"]["Current"] = gui.sync_analog_output_current_avg_slider

    def initializeConfocal():
        nonlocal gui
        sync_model["Confocal"] = OrderedDict()
        sync_model["Confocal"]["Shutter"] = [gui.sync_confocal_shutter_low_button, gui.sync_confocal_shutter_high_button]
        sync_model["Confocal"]["Channel"] = []
        for channel_number in range(1, 5):
            sync_model["Confocal"]["Channel"].append(eval("gui.sync_confocal_line_input" + str(channel_number) + "_button"))
        sync_model["Confocal"]["Line"] = gui.sync_confocal_line_tab
        sync_model["Confocal"]["Digital"] = [gui.sync_confocal_line_digital_low_button, gui.sync_confocal_line_digital_high_button]
        sync_model["Confocal"]["Threshold"] = gui.sync_confocal_line_analog_threshold_box
        sync_model["Confocal"]["Polarity"] = [gui.sync_confocal_line_analog_polarity_below_button, gui.sync_confocal_line_analog_polarity_above_button]
        sync_model["Confocal"]["Delay"] = OrderedDict()
        sync_model["Confocal"]["Delay"]["Mode"] = [gui.sync_confocal_scan_unidirectional_button, gui.sync_confocal_scan_bidirectional_button]
        sync_model["Confocal"]["Period"] = gui.sync_confocal_scan_period_box
        for delay in range(1,4):
            sync_model["Confocal"]["Delay"][str(delay)] = eval("gui.sync_confocal_delay" + str(delay) + "_box")
        for event in ["Standby", "Scanning"]:
            sync_model["Confocal"][event] = OrderedDict()
            sync_model["Confocal"][event]["Mode"] = eval("gui.sync_confocal_" + event.lower() + "_tab")
            sync_model["Confocal"][event]["LED"] = []
            for led_number in range(5):
                sync_model["Confocal"][event]["LED"].append(eval("gui.sync_confocal_" + event.lower() + "_constant_LED" + str(led_number) + "_button"))
            sync_model["Confocal"][event]["PWM"] = eval("gui.sync_confocal_" + event.lower() + "_constant_config_PWM_box")
            sync_model["Confocal"][event]["Current"] = eval("gui.sync_confocal_" + event.lower() + "_constant_config_current_box")
            sync_model["Confocal"][event]["Duration"] = eval("gui.sync_confocal_" + event.lower() + "_constant_config_duration_box")
            sync_model["Confocal"][event]["Sequence"] = ""

    sync_model = OrderedDict()
    sync_model["Mode"] = gui.sync_toolbox
    initializeDigital()
    initializeAnalog()
    initializeConfocal()
    sync_model["Output"] = []
    for output in range(4):
        sync_model["Output"].append(eval("gui.sync_output" + str(output) + "_button"))

    return sync_model

def initializeSeqList(gui):
    seq_table_list = [gui.sync_digital_low_sequence_table,
                      gui.sync_digital_high_sequence_table,
                      gui.sync_confocal_standby_sequence_table,
                      gui.sync_confocal_scanning_sequence_table]  # List of sequence table widgets
    return seq_table_list

def initializeSeqDictionary(gui):
    seq_dict = OrderedDict()
    seq_table_list = initializeSeqList(gui)
    for widget in seq_table_list:
        widget_header_obj = [widget.horizontalHeaderItem(c) for c in range(widget.columnCount())]  # Get headers from table
        widget_headers = [x.text() for x in widget_header_obj if x is not None]
        seq_dict[widget] = OrderedDict()
        for header in widget_headers:
            seq_dict[widget][header] = []
    return seq_dict

def initializeMainModel(gui):
    main_model = OrderedDict()
    main_model["Name"] = gui.main_driver_name_label2
    main_model["Serial"] = gui.main_driver_serial_label2
    main_model["Channel"] = [gui.main_channel_LED1_button,
                             gui.main_channel_LED2_button,
                             gui.main_channel_LED3_button,
                             gui.main_channel_LED4_button]
    main_model["Intensity"] = gui.main_intensity_dial
    main_model["Mode"] = [gui.main_toggle_slider, gui.main_intensity_PWM_button, gui.main_intensity_current_button, gui.main_intensity_off_button]
    main_model["Control"] = [gui.main_control_software_button, gui.main_control_physical_button]
    return main_model

def initializeEvents(gui):
    def menuEvents():
        nonlocal gui
#        gui.menu_connection.aboutToShow.connect(lambda: gui.ser.getDriverPort()) #Search for all available LED drivers on USB ports
        gui.menu_view_windows_status.triggered.connect(gui.createStatusWindow)
        gui.menu_view_windows_sync_plot.triggered.connect(gui.createSyncPlotWindow)


        # Dark/light mode view
        gui.menu_view_skins_dark.triggered.connect(lambda: gui.toggleSkin("dark"))
        gui.menu_view_skins_light.triggered.connect(lambda: gui.toggleSkin("light"))

        #Toggle gui locks
        gui.menu_view_lock_gui.triggered.connect(lambda: gui.lockInterface("gui"))
        gui.menu_view_lock_sync.triggered.connect(lambda: gui.lockInterface("sync"))
        gui.menu_view_lock_config.triggered.connect(lambda: gui.lockInterface("config"))

    def mainEvents():
        nonlocal gui

        #Connect dial and spinbox values - https://www.youtube.com/watch?v=BSP9sB0JoaE
        gui.main_intensity_dial.valueChanged.connect(
            lambda: gui.syncDialAndSpinbox(gui.main_intensity_dial, gui.main_intensity_spinbox))
        gui.main_intensity_dial.sliderReleased.connect(
            lambda: gui.syncDialAndSpinbox(gui.main_intensity_dial, gui.main_intensity_spinbox, True)) #Force update on mouse release
        gui.main_intensity_spinbox.valueChanged.connect(
            lambda: gui.syncDialAndSpinbox(gui.main_intensity_spinbox, gui.main_intensity_dial))

        gui.main_control_software_button.toggled.connect(lambda: gui.toggleSoftwareControl(gui.getValue(gui.main_control_software_button)))

        #Update configure plot current limits when active LED is changed
        gui.main_channel_LED1_button.clicked.connect(lambda: gui.ser.updateStatus())
        gui.main_channel_LED2_button.clicked.connect(lambda: gui.ser.updateStatus())
        gui.main_channel_LED3_button.clicked.connect(lambda: gui.ser.updateStatus())
        gui.main_channel_LED4_button.clicked.connect(lambda: gui.ser.updateStatus())

        gui.main_channel_LED1_button.clicked.connect(lambda: plot.setCalibrationScale(gui))
        gui.main_channel_LED2_button.clicked.connect(lambda: plot.setCalibrationScale(gui))
        gui.main_channel_LED3_button.clicked.connect(lambda: plot.setCalibrationScale(gui))
        gui.main_channel_LED4_button.clicked.connect(lambda: plot.setCalibrationScale(gui))

        #Update status if mode or control change
        gui.main_intensity_PWM_button.clicked.connect(lambda: gui.ser.updateStatus())
        gui.main_intensity_current_button.clicked.connect(lambda: gui.ser.updateStatus())
        gui.main_intensity_off_button.clicked.connect(lambda: gui.ser.updateStatus())

        gui.main_control_software_button.toggled.connect(lambda: gui.ser.updateStatus())

        #Disable manual control widgets when in sync mode
        gui.main_toggle_slider.valueChanged.connect(gui.syncDisableMain)

    def configureEvents():
        nonlocal gui
        def driverNameEvents():
            gui.config_model["Driver name"].textChanged.connect(lambda: gui.changeDriverName(gui.configure_name_driver_line_edit))

        def ledCheckBoxEvents():
            nonlocal gui
            # Changes to LED check boxes - toggle whether LED is active
            gui.config_model["LED1"]["Active"].stateChanged.connect(lambda: gui.toggleLedActive(1))
            gui.config_model["LED2"]["Active"].stateChanged.connect(lambda: gui.toggleLedActive(2))
            gui.config_model["LED3"]["Active"].stateChanged.connect(lambda: gui.toggleLedActive(3))
            gui.config_model["LED4"]["Active"].stateChanged.connect(lambda: gui.toggleLedActive(4))

        def ledNameEvents():
            nonlocal gui
            # Changes to LED names - updates GUI LED references with new name
            gui.config_model["LED1"]["ID"].textChanged.connect(lambda: gui.changeLedName(1, gui.config_model["LED1"]["ID"]))
            gui.config_model["LED2"]["ID"].textChanged.connect(lambda: gui.changeLedName(2, gui.config_model["LED2"]["ID"]))
            gui.config_model["LED3"]["ID"].textChanged.connect(lambda: gui.changeLedName(3, gui.config_model["LED3"]["ID"]))
            gui.config_model["LED4"]["ID"].textChanged.connect(lambda: gui.changeLedName(4, gui.config_model["LED4"]["ID"]))

        def resistorCheckBoxEvents():
            nonlocal gui
            # Changes to resistor check boxes - toggle whether resistor is active
            gui.config_model["Resistor2"]["Active"].stateChanged.connect(lambda: gui.toggleResistorActive(2))
            gui.config_model["Resistor3"]["Active"].stateChanged.connect(lambda: gui.toggleResistorActive(3))
            gui.config_model["Resistor4"]["Active"].stateChanged.connect(lambda: gui.toggleResistorActive(4))

        def resistorValueEvents():
            for resistor_number in range(1,5):
                gui.config_model["Resistor" + str(resistor_number)]["Value"].valueChanged.connect(lambda: fileIO.checkCurrentLimits(gui))


        def temperatureValueEvents():
            gui.config_model["Temperature"]["Transistor"]["Warn"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Temperature", "Transistor", "Warn"]))
            gui.config_model["Temperature"]["Transistor"]["Fault"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Temperature", "Transistor", "Fault"]))
            gui.config_model["Temperature"]["Resistor"]["Warn"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Temperature", "Resistor", "Warn"]))
            gui.config_model["Temperature"]["Resistor"]["Fault"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Temperature", "Resistor", "Fault"]))
            gui.config_model["Temperature"]["External"]["Warn"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Temperature", "External", "Warn"]))
            gui.config_model["Temperature"]["External"]["Fault"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Temperature", "External", "Fault"]))

            gui.config_model["Fan"]["Driver"]["Min"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Fan", "Driver", "Min"]))
            gui.config_model["Fan"]["Driver"]["Max"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Fan", "Driver", "Max"]))
            gui.config_model["Fan"]["External"]["Min"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Fan", "External", "Min"]))
            gui.config_model["Fan"]["External"]["Max"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Fan", "External", "Max"]))

        def fanChannelEvents():
            gui.config_model["Fan"]["Channel"][0].clicked.connect(lambda: gui.disableUsedOutputs(0, "config"))
            gui.config_model["Fan"]["Channel"][1].clicked.connect(lambda: gui.disableUsedOutputs(1, "config"))
            gui.config_model["Fan"]["Channel"][2].clicked.connect(lambda: gui.disableUsedOutputs(2, "config"))
            gui.config_model["Fan"]["Channel"][3].clicked.connect(lambda: gui.disableUsedOutputs(3, "config"))

        driverNameEvents()
        ledCheckBoxEvents()
        ledNameEvents()
        resistorCheckBoxEvents()
        resistorValueEvents()
        temperatureValueEvents()
        fanChannelEvents()

        gui.configure_audio_status_button.clicked.connect(lambda: gui.ser.testVolume(None, 0))
        gui.configure_audio_alarm_button.clicked.connect(lambda: gui.ser.testVolume(None, 1))
        gui.configure_pushbutton_alarm_test_button.clicked.connect(lambda: gui.ser.testVolume(None, 2))
        gui.configure_current_limit_test_button.clicked.connect(lambda: gui.ser.testCurrent())

        gui.configure_save_button.clicked.connect(lambda: fileIO.saveConfiguration(gui, gui.config_model))
        gui.configure_load_button.clicked.connect(lambda: fileIO.loadConfiguration(gui, gui.config_model))
        gui.configure_download_button.clicked.connect(lambda: gui.ser.downloadDriverConfiguration())
        gui.configure_upload_button.clicked.connect(lambda: gui.ser.uploadDriverConfiguration())

    def syncEvents():
        nonlocal gui

        def sequenceEvents():
            nonlocal gui
            # Save and load sequence files
            gui.sync_digital_trigger_low_sequence_save_button.clicked.connect(
                lambda: seq.saveSequence(gui, gui.sync_digital_low_sequence_table))
            gui.sync_digital_trigger_low_sequence_load_button.clicked.connect(
                lambda: seq.loadSequence(gui, gui.sync_digital_low_sequence_table))
            gui.sync_digital_trigger_high_sequence_save_button.clicked.connect(
                lambda: seq.saveSequence(gui, gui.sync_digital_high_sequence_table))
            gui.sync_digital_trigger_high_sequence_load_button.clicked.connect(
                lambda: seq.loadSequence(gui, gui.sync_digital_high_sequence_table))
            gui.sync_confocal_scanning_sequence_save_button.clicked.connect(
                lambda: seq.saveSequence(gui, gui.sync_confocal_scanning_sequence_table))
            gui.sync_confocal_scanning_sequence_load_button.clicked.connect(
                lambda: seq.loadSequence(gui, gui.sync_confocal_scanning_sequence_table))
            gui.sync_confocal_standby_sequence_save_button.clicked.connect(
                lambda: seq.saveSequence(gui, gui.sync_confocal_standby_sequence_table))
            gui.sync_confocal_standby_sequence_load_button.clicked.connect(
                lambda: seq.loadSequence(gui, gui.sync_confocal_standby_sequence_table))

            # Changes to sequence table
            gui.sync_digital_low_sequence_table.itemChanged.connect(gui.verifyCell)
            gui.sync_digital_high_sequence_table.itemChanged.connect(gui.verifyCell)
            gui.sync_confocal_scanning_sequence_table.itemChanged.connect(gui.verifyCell)
            gui.sync_confocal_standby_sequence_table.itemChanged.connect(gui.verifyCell)

        def outputChannelEvents():
            gui.sync_model["Output"][0].clicked.connect(lambda: gui.disableUsedOutputs(0, "sync"))
            gui.sync_model["Output"][1].clicked.connect(lambda: gui.disableUsedOutputs(1, "sync"))
            gui.sync_model["Output"][2].clicked.connect(lambda: gui.disableUsedOutputs(2, "sync"))
            gui.sync_model["Output"][3].clicked.connect(lambda: gui.disableUsedOutputs(3, "sync"))

        gui.sync_analog_output_tab.currentChanged.connect(lambda: gui.toggleAnalogChannel(gui.sync_analog_output_tab))
        gui.sync_confocal_scan_unidirectional_button.toggled.connect(lambda: gui.toggleScanMode())
        gui.sync_confocal_scan_period_button.clicked.connect(lambda: gui.ser.measurePeriod())

        gui.sync_download_button.clicked.connect(lambda: gui.ser.downloadSyncConfiguration())
        gui.sync_upload_button.clicked.connect(lambda: gui.ser.uploadSyncConfiguration())
        gui.sync_save_button.clicked.connect(lambda: seq.findUnsavedSeqThenSave(gui, gui.sync_model))
        gui.sync_load_button.clicked.connect(lambda: fileIO.loadConfiguration(gui, gui.sync_model))

        gui.sync_analog_output_PWM_avg_slider.valueChanged.connect(lambda: gui.updateAnalogSync("PWM"))
        gui.sync_analog_output_current_avg_slider.valueChanged.connect(lambda: gui.updateAnalogSync("current"))

        sequenceEvents()
        outputChannelEvents()

    def calibrationEvents():
        nonlocal gui
        gui.calibration_current_box.valueChanged.connect(lambda: plot.setCalibrationScale(gui))

        gui.calibration_run_button.clicked.connect(lambda: plot.startAnimation(gui, TimeLine(loopCount=0, interval=50)))


    menuEvents()
    mainEvents()
    syncEvents()
    configureEvents()
    calibrationEvents()

#Timer class for animating widgets such as the PyQtgraph
class TimeLine(QtCore.QObject):
    frameChanged = QtCore.pyqtSignal(int)

    def __init__(self, interval=60, loopCount=1, parent=None):
        super(TimeLine, self).__init__(parent)
        self._startFrame = 0
        self._endFrame = 0
        self._loopCount = loopCount
        self._timer = QtCore.QTimer(self, timeout=self.on_timeout)
        self._counter = 0
        self._loop_counter = 0
        self.setInterval(interval)

    def on_timeout(self):
        if self._startFrame <= self._counter < self._endFrame:
            self.frameChanged.emit(self._counter)
            self._counter += 1
        else:
            self._counter = 0
            self._loop_counter += 1

        if self._loopCount > 0:
            if self._loop_counter >= self.loopCount():
                self._timer.stop()
    def stop(self):
        self._timer.stop()

    def setLoopCount(self, loopCount):
        self._loopCount = loopCount

    def loopCount(self):
        return self._loopCount

    def frameCount(self):
        return self._counter

    interval = QtCore.pyqtProperty(int, fget=loopCount, fset=setLoopCount)

    def setInterval(self, interval):
        interval = int(round(interval)) #Ensure interval is an int
        self._timer.setInterval(interval)

    def interval(self):
        return self._timer.interval()

    interval = QtCore.pyqtProperty(int, fget=interval, fset=setInterval)

    def setFrameRange(self, startFrame, endFrame):
        self._startFrame = startFrame
        self._endFrame = endFrame

    @QtCore.pyqtSlot()
    def start(self):
        self._counter = 0
        self._loop_counter = 0
        self._timer.start()







