from collections import OrderedDict
import guiSequence as seq
import guiConfigIO as fileIO
from PyQt5 import QtGui, QtCore

def initializeConfigModel(gui):
    config_model = OrderedDict()
    config_model["Driver name"] = gui.configure_name_driver_line_edit
    # LEDs
    for board_number in range(1, gui.nBoards() + 1):
        for led_number in range(1, gui.nLeds() + 1):
            config_model["LED" + str(board_number) + str(led_number)] = OrderedDict(
                [("ID", eval("gui.configure_name_LED" + str(board_number) + str(led_number) + "_line_edit")),
                 ("Active", eval("gui.configure_name_LED" + str(board_number) + str(led_number) + "_box")),
                 ("Current Limit", eval("gui.configure_current_limit_LED" + str(board_number) + str(led_number) + "_spin_box"))])

    #Simultaneous LED operation
#    config_model["Simultaneous LED"] = [gui.configure_pushbutton_simul_off_button, gui.configure_pushbutton_simul_on_button]

    # Temperature cutoffs
    config_model["Temperature"] = OrderedDict([("Warn", eval("gui.configure_temperature_warn_box")),
             ("Fault", eval("gui.configure_temperature_fault_box"))])

    # Fan settings
    config_model["Fan"] = OrderedDict([("Min", eval("gui.configure_fan_min_box")),
             ("Max", eval("gui.configure_fan_max_box"))])

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
            sync_model["Digital"][trigger]["LED"] = OrderedDict()
            for board_number in range(1, gui.nBoards() + 1):
                sync_model["Digital"][trigger]["LED"]["Board" + str(board_number)] = []
                for led_number in range(1, gui.nLeds() + 1):
                    sync_model["Digital"][trigger]["LED"]["Board" + str(board_number)].append(eval("gui.sync_digital_trigger_" + trigger.lower() + "_constant_LED" + str(board_number) + str(led_number) + "_button"))
            sync_model["Digital"][trigger]["PWM"] = eval("gui.sync_digital_trigger_" + trigger.lower() + "_constant_config_PWM_box")
            sync_model["Digital"][trigger]["Current"] = eval("gui.sync_digital_trigger_" + trigger.lower() + "_constant_config_current_box")
            sync_model["Digital"][trigger]["Duration"] = eval("gui.sync_digital_trigger_" + trigger.lower() + "_constant_config_duration_box")
            sync_model["Digital"][trigger]["Sequence"] = ""

    def initializeAnalog():
        nonlocal gui
        sync_model["Analog"] = OrderedDict()
        for board_number in range(1, gui.nBoards() + 1):
            sync_model["Analog"]["Board" + str(board_number)] = []
            for led_number in range(1, gui.nLeds() + 2): #+2 to include "None" radio button
                sync_model["Analog"]["Board" + str(board_number)].append(eval("gui.sync_analog_LED" + str(board_number) + str(led_number) + "_button"))

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
            sync_model["Confocal"][event]["LED"] = OrderedDict()
            for board_number in range(1, gui.nBoards() + 1):
                sync_model["Confocal"][event]["LED"]["Board" + str(board_number)] = []
                for led_number in range(1, gui.nLeds() + 1):
                    sync_model["Confocal"][event]["LED"]["Board" + str(board_number)].append(eval("gui.sync_confocal_" + event.lower() + "_constant_LED" + str(board_number) + str(led_number) + "_button"))
            sync_model["Confocal"][event]["PWM"] = eval("gui.sync_confocal_" + event.lower() + "_constant_config_PWM_box")
            sync_model["Confocal"][event]["Current"] = eval("gui.sync_confocal_" + event.lower() + "_constant_config_current_box")
            sync_model["Confocal"][event]["Duration"] = eval("gui.sync_confocal_" + event.lower() + "_constant_config_duration_box")
            sync_model["Confocal"][event]["Sequence"] = ""

    sync_model = OrderedDict()
    sync_model["Mode"] = gui.sync_toolbox
    initializeDigital()
    initializeAnalog()
    initializeConfocal()

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
    main_model["Channel"] = OrderedDict()
    for board_number in range(1, gui.nBoards() + 1):
        main_model["Channel"]["Board" + str(board_number)] = []
        for led_number in range(1, gui.nLeds() + 1):
            main_model["Channel"]["Board" + str(board_number)].append(eval("gui.main_channel_LED" + str(board_number) + str(led_number) + "_button"))
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
        for board_number in range(1, gui.nBoards() + 1):
            for led_number in range(1, gui.nLeds() + 1):
                event = eval("gui.main_channel_LED" + str(board_number) + str(led_number) + "_button.clicked")
                event.connect(lambda: gui.ser.updateStatus())

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
            nonlocal gui
            gui.config_model["Driver name"].textChanged.connect(lambda: gui.changeDriverName(gui.configure_name_driver_line_edit))

        def ledCheckBoxEvents():
            nonlocal gui
            # Changes to LED check boxes - toggle whether LED is active
            for board_number in range(1, gui.nBoards() + 1):
                for led_number in range(1, gui.nLeds() + 1):
                    gui.config_model["LED" + str(board_number) + str(led_number)]["Active"].stateChanged.connect(lambda: gui.toggleLedActive())

        def ledNameEvents():
            nonlocal gui
            # Changes to LED names - updates GUI LED references with new name
            for board_number in range(1, gui.nBoards() + 1):
                for led_number in range(1, gui.nLeds() + 1):
                    gui.config_model["LED" + str(board_number) + str(led_number)]["ID"].textChanged.connect(lambda: gui.changeLedName())

        def temperatureValueEvents():
            nonlocal gui
            gui.config_model["Temperature"]["Warn"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Temperature", "Warn"]))
            gui.config_model["Temperature"]["Fault"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Temperature", "Fault"]))

            gui.config_model["Fan"]["Min"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Fan", "Min"]))
            gui.config_model["Fan"]["Max"].valueChanged.connect(lambda: fileIO.checkTemperatures(gui, ["Fan", "Max"]))

        driverNameEvents()
        ledCheckBoxEvents()
        ledNameEvents()
        temperatureValueEvents()

        gui.configure_audio_status_button.clicked.connect(lambda: gui.ser.testVolume(None, 0))
        gui.configure_audio_alarm_button.clicked.connect(lambda: gui.ser.testVolume(None, 1))
        gui.configure_pushbutton_alarm_test_button.clicked.connect(lambda: gui.ser.testVolume(None, 2))

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


        gui.sync_confocal_scan_unidirectional_button.toggled.connect(lambda: gui.toggleScanMode())
        gui.sync_confocal_scan_period_button.clicked.connect(lambda: gui.ser.measurePeriod())

        gui.sync_download_button.clicked.connect(lambda: gui.ser.downloadSyncConfiguration())
        gui.sync_upload_button.clicked.connect(lambda: gui.ser.uploadSyncConfiguration())
        gui.sync_save_button.clicked.connect(lambda: seq.findUnsavedSeqThenSave(gui, gui.sync_model))
        gui.sync_load_button.clicked.connect(lambda: fileIO.loadConfiguration(gui, gui.sync_model))

        sequenceEvents()

    menuEvents()
    mainEvents()
    syncEvents()
    configureEvents()

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







