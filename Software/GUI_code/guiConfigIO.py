import copy
import math
import struct
import sys
import tempfile
from PyQt5 import QtWidgets
from collections import OrderedDict
import ast
import guiSequence as seq

#Thermistor properties
PCB_THERMISTOR_NOMINAL = 4700 #Value of thermistor on PCB at nominal temp (25°C)
PCB_B_COEFFICIENT = 3545 #Beta value for the PCB thermistor
EXT_THERMISTOR_NOMINAL = 4700 #Value of external thermistor at nominal temp (25°C)
EXT_B_COEFFICIENT = 3545 #Beta value for the PCB thermistor
SERIES_RESISTOR = 4700 #Resistor value in series with thermistor on PCB board
DEFAULT_CLOCK_SPEED = 180 #Clock speed of the Teensy in MHz - used to convert confocal delay times to clock cycles for sub-microsecond precision

def saveConfiguration(gui, model, file=None):
    def writeLines(prefix, dictionary):
        nonlocal gui
        nonlocal file
        for key, value in dictionary.items():
            if isinstance(value, OrderedDict):
                writeLines(prefix + key + "::", value)
            else:
                file.write(prefix + key + "::" + str(gui.getValue(value)) + "\n")
            pass

    if file is None:
        path = QtWidgets.QFileDialog.getSaveFileName(gui, 'Save File', '', 'TXT(*.txt)')
        if path[0] != "":
            with open(str(path[0]), 'w', newline='') as file:
                writeLines("", model)

    else:
        writeLines("", model)

def loadConfiguration(gui, model, file=None):
    lines = []
    if file is None:
        path = QtWidgets.QFileDialog.getOpenFileName(gui, 'Open File', '', 'TXT(*.txt)')
        if path[0] != "":
            with open(str(path[0]), 'r', newline='') as file:
                lines = file.readlines()
    else:
        lines = file.readlines()

    for line in lines:
        line = line.strip('\n')
        line_list = line.split("::")
        dictionary = model
        key_path = []
        for index, key in enumerate(line_list):
            if index < len(line_list)-1:
                try:
                    key_path.append(key)
                    dictionary = dictionary[key]
                except (KeyError, TypeError):
                    showMessage(gui, "Error: \"" + key +"\" is not a valid key in \"" + line + "\". Load aborted at this step.")
                    return
            else:
                entry = gui.getValue(dictionary)
                if isinstance(entry, str): #Block values that are meant to be strings from getting evaluated
                    value = key
                else:
                    try:
                        value = ast.literal_eval(key)
                    except ValueError:
                        value = ast.literal_eval("\'" + key+ "\'") #Fix literal_eval ValueError - https://stackoverflow.com/questions/14611352/malformed-string-valueerror-ast-literal-eval-with-string-representation-of-tup
                    except SyntaxError:
                        showMessage(gui, "Error: \"" + line + "\" could not be parsed. Load aborted at this step.")
                        return
                if key_path[-1] == "Sequence": #Process sequence file loads separately
                    widget = eval("gui.sync_" + key_path[0].lower() + "_" + key_path[1].lower() + "_sequence_table") #Assign to proper widget table
                    seq.setSequencePath(gui, widget, value)  # Save path to dictionary
                    if value != "None":
                        seq.loadSequence(gui, widget, True) #Load sequence file to table
                elif not gui.setValue(dictionary, value):
                    showMessage(gui, "Error: \"" + key +"\" is not a valid value in \"" + line + "\". Load aborted at this step.")
                    return

    #Check if external fan and sync out are on the same channel and disable used channels
    for channel in range(1,4):
        if gui.getValue(gui.config_model["Fan"]["Channel"][channel]) and gui.getValue(gui.sync_model["Output"][channel]):
            showMessage(gui, "Warning: The external fan output and sync output are both using output #" + str(channel) + ". Change one of these channels for the driver to function properly.")
        else:
            if gui.getValue(gui.config_model["Fan"]["Channel"][channel]):
                gui.disableUsedOutputs(channel, "config")
            else:
                gui.disableUsedOutputs(channel, "sync")
    checkCurrentLimits(gui)

def checkCurrentLimits(gui):
    total_resistance = 0
    for resistor in range(1,5):
        if gui.getValue(gui.config_model["Resistor" + str(resistor)]["Active"]):
            total_resistance += 1/gui.getValue(gui.config_model["Resistor" + str(resistor)]["Value"])
    total_resistance = 1/total_resistance
    maximum_current = 3.3/total_resistance
    maximum_current = round(maximum_current, -int(math.floor(math.log10(abs(maximum_current))))+1) #Report max current to 2 significant figures - https://stackoverflow.com/questions/3410976/how-to-round-a-number-to-significant-figures-in-python

    gui.configure_current_limit_box.setTitle("LED Current Limit (" + str(maximum_current) + "A Max)")
    gui.configure_current_limit_box.setWhatsThis(str(total_resistance)) #Store total resistance in whats this so other functions can retrieve it without needing to recalculate the value
    for led_number in range(1,5):
        gui.config_model["LED" + str(led_number)]["Current Limit"].setMaximum(maximum_current)
        gui.config_model["LED" + str(led_number)]["Current Limit"].setToolTip("Set the current limit (in amps) for LED #1"
                                                                              " - " + str(maximum_current) + " amps maximum.")

def checkTemperatures(gui, key_list):
    if key_list[0] == "Temperature":
        labels = ["Warn", "Fault"]
    else:
        labels = ["Min", "Max"]

    if key_list[2] in ["Warn", "Min"]:
        gui.config_model[key_list[0]][key_list[1]][labels[1]].setMinimum(gui.getValue(gui.config_model[key_list[0]][key_list[1]][labels[0]]) + 1)
    else:
        gui.config_model[key_list[0]][key_list[1]][labels[0]].setMaximum(gui.getValue(gui.config_model[key_list[0]][key_list[1]][labels[1]]) - 1)

def bytesToConfig(byte_array, gui, prefix):
    global EXT_THERMISTOR_NOMINAL
    global EXT_B_COEFFICIENT
    start_index = 0
    index = 0

    #Verify checksum of config file
    checksum = (sum(byte_array) + prefix) & 0xFF #https://stackoverflow.com/questions/44611057/checksum-generation-from-sum-of-bits-in-python
    if checksum == 0:
        while int(byte_array[index]) != 0:
            index += 1
        gui.setValue(gui.config_model["Driver name"], byte_array[start_index:index].decode().rstrip())

        for led_number in range (1,5):
            index += 1
            start_index = index
            while int(byte_array[index]) != 0:
                index += 1
            gui.setValue(gui.config_model["LED" + str(led_number)]["ID"], byte_array[start_index:index].decode().rstrip())
        index += 1
        config_values = struct.unpack("<????HHHHBBBBffff????HHHHHHHHHHBiiBB?BB", byte_array[index:]) #Parse byte array values: https://docs.python.org/3/library/struct.html#struct-alignment

        #Calculate total resistance to be able to convert current limit values
        total_resistance = 0
        for resistor in range (1,5):
            resistance = config_values[resistor + 11]
            active = config_values[resistor + 15]
            if active:
                total_resistance += 1/resistance
            gui.setValue(gui.config_model["Resistor" + str(resistor)]["Value"], resistance)
            gui.setValue(gui.config_model["Resistor" + str(resistor)]["Active"], active)
        total_resistance = 1 / total_resistance
        checkCurrentLimits(gui) #Set new current limits in GUI

        for led_number in range(1, 5):
            current_limit = config_values[led_number + 3]
            current_limit = (3.3*(current_limit/65535))/total_resistance
            gui.setValue(gui.config_model["LED" + str(led_number)]["Active"], config_values[led_number - 1])
            gui.setValue(gui.config_model["LED" + str(led_number)]["Current Limit"], current_limit)
            channel_id = gui.getValue(gui.config_model["LED" + str(config_values[led_number + 7]+1)]["ID"])
            gui.setValue(gui.config_model["Channel" + str(led_number)], channel_id)


        #Get external thermistor properties to convert ADC values to temperatures
        EXT_THERMISTOR_NOMINAL = config_values[31]  # Value of external thermistor at nominal temp (25°C)
        gui.setValue(gui.config_model["Thermistor"]["Resistance"], EXT_THERMISTOR_NOMINAL)
        EXT_B_COEFFICIENT = config_values[32]  # Beta value for the PCB thermistor
        gui.setValue(gui.config_model["Thermistor"]["Beta"], EXT_B_COEFFICIENT)

        for index, source in enumerate(["Transistor", "Resistor", "External"]):
            external = False
            if source == "External":
                external = True
            gui.setValue(gui.config_model["Temperature"][source]["Warn"], round(adcToTemp(config_values[20 + index], external)))
            gui.setValue(gui.config_model["Temperature"][source]["Fault"], round(adcToTemp(config_values[23 + index], external)))

        for index, source in enumerate(["Driver", "External"]):
            external = False
            if source == "External":
                external = True
            gui.setValue(gui.config_model["Fan"][source]["Min"], round(adcToTemp(config_values[26 + 2*index], external)))
            gui.setValue(gui.config_model["Fan"][source]["Max"], round(adcToTemp(config_values[27 + 2*index], external)))

        channel_id = gui.config_model["Fan"]["Channel"][config_values[30]].text()
        gui.setValue(gui.config_model["Fan"]["Channel"], channel_id)

        gui.setValue(gui.config_model["Audio"]["Status"], config_values[33])
        gui.setValue(gui.config_model["Audio"]["Alarm"], config_values[34])
        gui.config_model["Pushbutton"]["Indication"][int(config_values[35])].setChecked(True)
        channel_id = gui.config_model["Pushbutton"]["Alarm"][config_values[36]].text()
        gui.setValue(gui.config_model["Pushbutton"]["Alarm"], channel_id)

        gui.setAdcCurrentLimit(config_values[4:8])  # Save current limit ADC values in current limit "what's this"
        if not gui.ser.initializing_connection:
            showMessage(gui, "Configuration file was successfully downloaded.")

    else:
        showMessage(gui, "Error: Driver config file had invalid checksum: " + str(checksum) + ". Upload aborted.")

def bytesToSync(byte_array, gui, prefix):
    sync_values = [None]* 39
    def setWidget(widgets, index):
        try:
            widget_string = widgets[sync_values[index]].text()
            gui.setValue(widgets, widget_string)
        except:
            showMessage(gui, "Error: Widget index not found for " + str(widgets))
            print(str(widgets[0].objectName())  + " " + str(index) + " " + str(sync_values[index]))
            sys.exit()
            return None

    checksum = (sum(byte_array) + prefix) & 0xFF  # https://stackoverflow.com/questions/44611057/checksum-generation-from-sum-of-bits-in-python
    if checksum == 0:
        sync_values = struct.unpack("<BBBBBBBHHHHLLBBBHH?B???H?LLLLBBBBHHHHLLB", byte_array)

        #Calculate total resistance for current conversions
        total_resistance = float(gui.configure_current_limit_box.whatsThis())

        #Digital
        gui.sync_model["Mode"].setCurrentIndex(sync_values[0])
        gui.sync_model["Mode"].setWhatsThis(gui.getValue(gui.sync_model["Mode"])) #Store driver mode name in whats this
        setWidget(gui.sync_model["Output"], 1)
        setWidget(gui.sync_model["Digital"]["Channel"], 2)

        current_limit = [0]*2
        for index3, key3 in enumerate(["Mode", "LED", "PWM", "Current", "Duration"]):
            for index2, key2 in enumerate(["Low", "High"]):
                if key3 == "Mode":
                    gui.sync_model["Digital"][key2][key3].setCurrentIndex(sync_values[(2 * index3) + index2 + 3])
                if key3 == "LED":
                    setWidget(gui.sync_model["Digital"][key2][key3], (2 * index3) + index2 + 3)
                    current_limit[index2] = gui.getValue(gui.config_model["LED" + str(sync_values[(2 * index3) + index2 + 3]+1)]["Current Limit"])
                elif key3 == "PWM":
                    gui.setValue(gui.sync_model["Digital"][key2][key3], sync_values[(2 * index3) + index2 + 3]/65535*100)
                elif key3 == "Current":
                    gui.setValue(gui.sync_model["Digital"][key2][key3], (((sync_values[(2 * index3) + index2 + 3]/65535)*3.3/total_resistance)/current_limit[index2])*100)
                elif key3 == "Duration":
                    gui.setValue(gui.sync_model["Digital"][key2][key3], sync_values[(2 * index3) + index2 + 3]/1e6)

        #Analog
        for index2, key2 in enumerate(["Channel", "LED"]):
            setWidget(gui.sync_model["Analog"][key2], 13+index2)
        gui.sync_model["Analog"]["Mode"].setCurrentIndex(sync_values[15])
        gui.setValue(gui.sync_model["Analog"]["PWM"], sync_values[16])
        gui.setValue(gui.sync_model["Analog"]["Current"], sync_values[17])

        #Confocal
        for index2, key2 in enumerate(["Shutter", "Channel", "Line", "Digital", "Polarity"]):
            if key2 == "Line":
                gui.sync_model["Confocal"][key2].setCurrentIndex(sync_values[18 + index2])
            else:
                setWidget(gui.sync_model["Confocal"][key2], 18+index2)
        gui.setValue(gui.sync_model["Confocal"]["Threshold"], sync_values[23]/65535*3.3)
        setWidget(gui.sync_model["Confocal"]["Delay"]["Mode"], 24)
        gui.setValue(gui.sync_model["Confocal"]["Period"], sync_values[25]/DEFAULT_CLOCK_SPEED)
        for index3 in range(1,4):
            gui.setValue(gui.sync_model["Confocal"]["Delay"][str(index3)], sync_values[25+index3]/DEFAULT_CLOCK_SPEED)
        for index3, key3 in enumerate(["Mode", "LED", "PWM", "Current", "Duration"]):
            for index2, key2 in enumerate(["Standby", "Scanning"]):
                if key3 == "Mode":
                    gui.sync_model["Confocal"][key2][key3].setCurrentIndex(sync_values[(2 * index3) + index2 + 29])
                if key3 == "LED":
                    setWidget(gui.sync_model["Confocal"][key2][key3], (2 * index3) + index2 + 29)
                    current_limit[index2] = gui.getValue(gui.config_model["LED" + str(sync_values[(2 * index3) + index2 + 3]+1)]["Current Limit"])
                elif key3 == "PWM":
                    gui.setValue(gui.sync_model["Confocal"][key2][key3], (sync_values[(2 * index3) + index2 + 29]/65535)*100)
                elif key3 == "Current":
                    gui.setValue(gui.sync_model["Confocal"][key2][key3], (((sync_values[(2 * index3) + index2 + 29]/65535)*3.3/total_resistance)/current_limit[index2])*100)
                elif key3 == "Duration":
                    gui.setValue(gui.sync_model["Confocal"][key2][key3], sync_values[(2 * index3) + index2 + 29]/1e6)

        updateModelWhatsThis(gui, gui.sync_model)
        return True

    else:
        showMessage(gui, "Error: Driver config file had invalid checksum: " + str(checksum) + ". Upload aborted.")
        return False

def configToBytes(gui, prefix, update_model=True):
    global EXT_THERMISTOR_NOMINAL
    global EXT_B_COEFFICIENT
    config_values = [None] * 37

    byte_array = bytearray() #Initialize empty byte array

    byte_array.extend(gui.getValue(gui.config_model["Driver name"]).ljust(gui.config_model["Driver name"].maxLength(), " ").encode()) #Add string with right padding of spaces for max length of QLineEdit
    byte_array.append(0)

    for led_number in range(1, 5):
        byte_array.extend(gui.getValue(gui.config_model["LED" + str(led_number)]["ID"]).ljust(gui.config_model["LED" + str(led_number)]["ID"].maxLength()," ").encode())  # Add string with right padding of spaces for max length of QLineEdit
        byte_array.append(0)

    total_resistance = 0
    for resistor in range (1,5):
        config_values[resistor + 11] = gui.getValue(gui.config_model["Resistor" + str(resistor)]["Value"])
        config_values[resistor + 15] = gui.getValue(gui.config_model["Resistor" + str(resistor)]["Active"])
        if config_values[resistor + 15]:
            total_resistance += 1/config_values[resistor + 11]
    total_resistance = 1 / total_resistance

    for led_number in range(1, 5):
        current_limit = gui.getValue(gui.config_model["LED" + str(led_number)]["Current Limit"])
        config_values[led_number + 3] = round(((current_limit*total_resistance)/3.3)*65535) #Convert current limit to ADC reading (voltage)
        config_values[led_number - 1] = gui.getValue(gui.config_model["LED" + str(led_number)]["Active"])
        for index, widget in enumerate(gui.config_model["Channel" + str(led_number)]):
            if gui.getValue(widget):
                config_values[led_number + 7] = index
                break

    EXT_THERMISTOR_NOMINAL = gui.getValue(gui.config_model["Thermistor"]["Resistance"])
    EXT_B_COEFFICIENT = gui.getValue(gui.config_model["Thermistor"]["Beta"])
    config_values[31] = EXT_THERMISTOR_NOMINAL  # Value of external thermistor at nominal temp (25°C)
    config_values[32] = EXT_B_COEFFICIENT  # Beta value for the PCB thermistor

    for index, source in enumerate(["Transistor", "Resistor", "External"]):
        external = False
        if source == "External":
            external = True
        config_values[20 + index] = tempToAdc(gui.getValue(gui.config_model["Temperature"][source]["Warn"]), external)
        config_values[23 + index] = tempToAdc(gui.getValue(gui.config_model["Temperature"][source]["Fault"]), external)

    for index, source in enumerate(["Driver", "External"]):
        external = False
        if source == "External":
            external = True
        config_values[26 + 2*index] = tempToAdc(gui.getValue(gui.config_model["Fan"][source]["Min"]), external)
        config_values[27 + 2*index] = tempToAdc(gui.getValue(gui.config_model["Fan"][source]["Max"]), external)

    for index, widget in enumerate(gui.config_model["Fan"]["Channel"]):
        if gui.getValue(widget):
            config_values[30] = index
            break

    config_values[33] = gui.getValue(gui.config_model["Audio"]["Status"])
    config_values[34] = gui.getValue(gui.config_model["Audio"]["Alarm"])
    config_values[35] = gui.config_model["Pushbutton"]["Indication"][1].isChecked()

    for index, widget in enumerate(gui.config_model["Pushbutton"]["Alarm"]):
        if gui.getValue(widget):
            config_values[36] = index
            break

    byte_array.extend(struct.pack("<????HHHHBBBBffff????HHHHHHHHHHBiiBB?B", *config_values))

    checksum = (sum(byte_array) + prefix) & 0xFF  # https://stackoverflow.com/questions/44611057/checksum-generation-from-sum-of-bits-in-python
    checksum = 256 - checksum
    byte_array.append(checksum)

    if update_model:
        gui.setAdcCurrentLimit(config_values[4:8]) #Save current limit ADC values in current limit "what's this"

    return byte_array

def syncToBytes(gui, prefix, update_model=True):
    if update_model:
        updateModelWhatsThis(gui, gui.sync_model)

    sync_values = [None]* 39
    byte_array = bytearray() #Initialize empty byte array

    def widgetIndex(widget_list):
        for w_index, n_widget in enumerate(widget_list):
            if gui.getValue(n_widget):
                return w_index
        else:
            showMessage(gui, "Error: Widget index not found!")
            return None

    #Calculate total resistance for current conversions
    total_resistance = float(gui.configure_current_limit_box.whatsThis())

    #Digital
    sync_values[0] = gui.sync_model["Mode"].currentIndex()
    sync_values[1] = widgetIndex(gui.sync_model["Output"])
    sync_values[2] = widgetIndex(gui.sync_model["Digital"]["Channel"])
    current_limit = [0]*2
    for index3, key3 in enumerate(["Mode", "LED", "PWM", "Current", "Duration"]):
        for index2, key2 in enumerate(["Low", "High"]):
            if key3 == "Mode":
                sync_values[(2 * index3) + index2 + 3] = gui.sync_model["Digital"][key2][key3].currentIndex()
            if key3 == "LED":
                sync_values[(2 * index3) + index2 + 3] = widgetIndex(gui.sync_model["Digital"][key2][key3])
                if sync_values[(2 * index3) + index2 + 29] == 0: #If current LED is selected - get active LED channel from main window
                    sync_values[(2 * index3) + index2 + 29] = widgetIndex(gui.main_model["Channel"])+1
                current_limit[index2] = gui.getValue(gui.config_model["LED" + str(sync_values[(2 * index3) + index2 + 3]+1)]["Current Limit"])
            elif key3 == "PWM":
                sync_values[(2 * index3) + index2 + 3] = round((gui.getValue(gui.sync_model["Digital"][key2][key3])/100)*65535)
            elif key3 == "Current":
                sync_values[(2 * index3) + index2 + 3] = round((((gui.getValue(gui.sync_model["Digital"][key2][key3])/100)*current_limit[index2] * total_resistance) / 3.3) * 65535)  # Convert current limit to ADC reading (voltage)
                print("Digital Input: " + str(gui.getValue(gui.sync_model["Digital"][key2][key3])) + ", Limit: " + str(current_limit[index2]) + ", Res: " + str(total_resistance))
            elif key3 == "Duration":
                sync_values[(2 * index3) + index2 + 3] = round(gui.getValue(gui.sync_model["Digital"][key2][key3])*1e6)  # Convert duration to microseconds

    #Analog
    for index2, key2 in enumerate(["Channel", "LED"]):
        sync_values[13+index2] = widgetIndex(gui.sync_model["Analog"][key2])
    sync_values[15] = gui.sync_model["Analog"]["Mode"].currentIndex()
    sync_values[16] = gui.getValue(gui.sync_model["Analog"]["PWM"])
    sync_values[17] = gui.getValue(gui.sync_model["Analog"]["Current"])

    #Confocal
    for index2, key2 in enumerate(["Shutter", "Channel", "Line", "Digital", "Polarity"]):
        if key2 == "Line":
            sync_values[18 + index2] = gui.sync_model["Confocal"][key2].currentIndex()
        else:
            sync_values[18+index2] = widgetIndex(gui.sync_model["Confocal"][key2])
    sync_values[23] = round(gui.getValue(gui.sync_model["Confocal"]["Threshold"])/3.3*65535)
    sync_values[24] = widgetIndex(gui.sync_model["Confocal"]["Delay"]["Mode"])
    sync_values[25] = round(gui.getValue(gui.sync_model["Confocal"]["Period"]) * DEFAULT_CLOCK_SPEED)

    for index3 in range(1,4):
        sync_values[25+index3] = round(gui.getValue(gui.sync_model["Confocal"]["Delay"][str(index3)])*DEFAULT_CLOCK_SPEED) #Convert the delay times to clock cycles at default Teensy speed
    for index3, key3 in enumerate(["Mode", "LED", "PWM", "Current", "Duration"]):
        for index2, key2 in enumerate(["Standby", "Scanning"]):
            if key3 == "Mode":
                sync_values[(2 * index3) + index2 + 29] = gui.sync_model["Confocal"][key2][key3].currentIndex()
            if key3 == "LED":
                sync_values[(2 * index3) + index2 + 29] = widgetIndex(gui.sync_model["Confocal"][key2][key3])
                if sync_values[(2 * index3) + index2 + 29] == 0: #If current LED is selected - get active LED channel from main window
                    sync_values[(2 * index3) + index2 + 29] = widgetIndex(gui.main_model["Channel"])+1
                current_limit[index2] = gui.getValue(gui.config_model["LED" + str(sync_values[(2 * index3) + index2 + 29])]["Current Limit"])
            elif key3 == "PWM":
                sync_values[(2 * index3) + index2 + 29]  = round((gui.getValue(gui.sync_model["Confocal"][key2][key3]) / 100) * 65535) #Convert to clock-cycles, where 100% = # of clock cycles in delay #2
            elif key3 == "Current":
                sync_values[(2 * index3) + index2 + 29] = round((((gui.getValue(gui.sync_model["Confocal"][key2][key3])/100)*current_limit[index2] * total_resistance) / 3.3) * 65535)  # Convert current to ADC reading (voltage) as percent of current limit
                print("Confocal Input: " + str(gui.getValue(gui.sync_model["Confocal"][key2][key3])) + ", Limit: " + str(current_limit[index2]) + ", Res: " + str(total_resistance))
            elif key3 == "Duration":
                sync_values[(2 * index3) + index2 + 29] = round(gui.getValue(gui.sync_model["Confocal"][key2][key3])*1e6)

    byte_array.extend(struct.pack("<BBBBBBBHHHHLLBBBHH?B???H?LLLLBBBBHHHHLL", *sync_values))
    checksum = (sum(byte_array) + prefix) & 0xFF  # https://stackoverflow.com/questions/44611057/checksum-generation-from-sum-of-bits-in-python
    checksum = 256 - checksum
    byte_array.append(checksum)
    return byte_array

def updateModelWhatsThis(gui, dictionary):
    for key, value in dictionary.items():
        if isinstance(value, OrderedDict):
            updateModelWhatsThis(gui, value)
        elif isinstance(value, list):
            for widget in value:
                if isinstance(value, str):
                    break
                else:
                    widget.setWhatsThis(str(gui.getValue(value)))
        else:
            if not isinstance(value, (str, type(None))):
                value.setWhatsThis(str(gui.getValue(value)))

def adcToTemp(adc, external = False):
    try:
        if external:
            therm_nominal = EXT_THERMISTOR_NOMINAL
            b_coefficient = EXT_B_COEFFICIENT
        else:
            therm_nominal = PCB_THERMISTOR_NOMINAL
            b_coefficient = PCB_B_COEFFICIENT
        if adc > 65500: #If ADC value is arbitrarily high then thermistor is disconnected, so return arbitrarily low value
            return -1000
        raw = adc
        raw = 65535 / raw - 1
        raw = SERIES_RESISTOR / raw
        steinhart = raw / therm_nominal
        steinhart = math.log(steinhart)
        steinhart /= b_coefficient
        steinhart += 1.0 / (25 + 273.15)
        steinhart = 1.0 / steinhart
        steinhart -= 273.15
    except ZeroDivisionError:
        return -1000 #Return impossible temp if invalid ADC value is received
    return steinhart

def tempToAdc(temperature, external = False):
    if external:
        therm_nominal = EXT_THERMISTOR_NOMINAL
        b_coefficient = EXT_B_COEFFICIENT
    else:
        therm_nominal = PCB_THERMISTOR_NOMINAL
        b_coefficient = PCB_B_COEFFICIENT

    steinhart = temperature
    steinhart += 273.15
    steinhart = 1.0 / steinhart
    steinhart -= 1.0 / (25 + 273.15)
    steinhart *= b_coefficient
    steinhart = math.exp(steinhart)
    raw = steinhart * therm_nominal
    raw = SERIES_RESISTOR / raw
    raw = 65535 / (raw + 1)
    return round(raw)

def showMessage(gui, text):
    gui.waitCursor(False)
    gui.stopSplash()
    gui.message_box.setText(text)
    gui.message_box.exec()

