import math
import struct
from PyQt5 import QtWidgets
from collections import OrderedDict
import ast
import guiSequence as seq

#Thermistor properties
PCB_THERMISTOR_NOMINAL = 4700 #Value of thermistor on PCB at nominal temp (25°C)
PCB_B_COEFFICIENT = 3500 #Beta value for the PCB thermistor
SERIES_RESISTOR = 3600 #Resistor value in series with thermistor on PCB board
DEFAULT_CLOCK_SPEED = 600 #Clock speed of the Teensy in MHz - used to convert confocal delay times to clock cycles for sub-microsecond precision
N_BOARDS = 3 #Number of boards connected to the driver
N_LEDS = 4 #Number of LEDs on each boards


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

def checkTemperatures(gui, key_list):
    if key_list[0] == "Temperature":
        labels = ["Warn", "Fault"]
    else:
        labels = ["Min", "Max"]

    if key_list[1] in ["Fault", "Max"]:
        gui.config_model[key_list[0]][key_list[1]].setMinimum(gui.getValue(gui.config_model[key_list[0]][labels[0]]) + 1)
    else:
        gui.config_model[key_list[0]][key_list[1]].setMaximum(gui.getValue(gui.config_model[key_list[0]][labels[1]]) - 1)

def bytesToConfig(byte_array, gui, prefix):
    global EXT_THERMISTOR_NOMINAL
    global EXT_B_COEFFICIENT
    start_index = 0
    index = 0

    #Verify checksum of config file
    checksum = (sum(byte_array) + prefix) & 0xFF #https://stackoverflow.com/questions/44611057/checksum-generation-from-sum-of-bits-in-python
    if checksum == 0:
        #Get driver name - ends with NULL
        while int(byte_array[index]) != 0:
            index += 1
        gui.setValue(gui.config_model["Driver name"], byte_array[start_index:index].decode().rstrip())

        # Get led names - end with NULL
        for board_number in range (1, gui.nBoards()+1):
            for led_number in range (1, gui.nLeds()+1):
                index += 1
                start_index = index
                while int(byte_array[index]) != 0:
                    index += 1
                gui.setValue(gui.config_model["LED" + str(board_number) + str(led_number)]["ID"], byte_array[start_index:index].decode().rstrip())
        index += 1
        unpack_string = "<"
        #Add a boolean for each LED (LED active)
        for board_number in range (1, gui.nBoards()+1):
            for led_number in range (1, gui.nLeds()+1):
                unpack_string += "?"

        #Add a uint16_t for each LED (current limit)
        for board_number in range(1, gui.nBoards() + 1):
            for led_number in range(1, gui.nLeds() + 1):
                unpack_string += "H"

        unpack_string += "?HHHHBB?BB"
        config_values = struct.unpack(unpack_string, byte_array[index:]) #Parse byte array values: https://docs.python.org/3/library/struct.html#struct-alignment
        config_values_index = 0

        #Get LED active state
        for board_number in range(1, gui.nBoards() + 1):
            for led_number in range(1, gui.nLeds() + 1):
                gui.setValue(gui.config_model["LED" + str(board_number) + str(led_number)]["Active"], config_values[config_values_index])
                config_values_index += 1

        #Get LED current limits
        for board_number in range(1, gui.nBoards() + 1):
            for led_number in range(1, gui.nLeds() + 1):
                current_limit = config_values[config_values_index]
                current_limit = 100*(current_limit/65535)
                gui.setValue(gui.config_model["LED" + str(board_number) + str(led_number)]["Current Limit"], current_limit)
                gui.setAdcCurrentLimit(board_number, led_number, current_limit)
                config_values_index += 1

        #Get simultaneous state if in use
        if "Simultaneous LED" in gui.config_model:
            pass
        config_values_index += 1

        #Get warn and fault temperatures
        gui.setValue(gui.config_model["Temperature"]["Warn"], round(adcToTemp(config_values[config_values_index])))
        gui.setValue(gui.config_model["Temperature"]["Fault"], round(adcToTemp(config_values[config_values_index+1])))
        config_values_index += 2

        #Get fan temperatures
        gui.setValue(gui.config_model["Fan"]["Min"], round(adcToTemp(config_values[config_values_index])))
        gui.setValue(gui.config_model["Fan"]["Max"], round(adcToTemp(config_values[config_values_index + 1])))
        config_values_index += 2

        gui.setValue(gui.config_model["Audio"]["Status"], config_values[config_values_index])
        gui.setValue(gui.config_model["Audio"]["Alarm"], config_values[config_values_index+1])
        gui.config_model["Pushbutton"]["Indication"][int(config_values[config_values_index+2])].setChecked(True)
        channel_id = gui.config_model["Pushbutton"]["Alarm"][config_values[config_values_index+3]].text()
        gui.setValue(gui.config_model["Pushbutton"]["Alarm"], channel_id)

        if not gui.ser.initializing_connection:
            showMessage(gui, "Configuration file was successfully downloaded.")

    else:
        showMessage(gui, "Error: Driver config file had invalid checksum: " + str(checksum) + ". Upload aborted.")

def bytesToSync(byte_array, gui, prefix):
    sync_values = [None] * (15 + 2*11 + 3)
    def setWidget(widgets, index):
        try:
            widget_string = widgets[index].text()
            gui.setValue(widgets, widget_string)
        except:
            showMessage(gui, "Error: Widget index not found at index " + str(index) + " for " + str(widgets))
            return None

    checksum = (sum(byte_array) + prefix) & 0xFF  # https://stackoverflow.com/questions/44611057/checksum-generation-from-sum-of-bits-in-python
    if checksum == 0:
        sync_values = struct.unpack("<BBBBBBHHHHLLBBB?B???H?LLLLBBBBHHHHLLB", byte_array)
        index = 0

        #Digital
        gui.sync_model["Mode"].setCurrentIndex(sync_values[index])
        gui.sync_model["Mode"].setWhatsThis(gui.getValue(gui.sync_model["Mode"])) #Store driver mode name in whats this
        setWidget(gui.sync_model["Digital"]["Channel"], sync_values[index+1])
        index += 2

        current_limit = [0]*2
        for index3, key3 in enumerate(["Mode", "LED", "PWM", "Current", "Duration"]):
            for index2, key2 in enumerate(["Low", "High"]):
                if key3 == "Mode":
                    gui.sync_model["Digital"][key2][key3].setCurrentIndex(sync_values[(2 * index3) + index2 + index])
                if key3 == "LED":
                    board_number = math.floor(sync_values[(2 * index3) + index2 + index]/gui.nLeds())+1
                    led_number = sync_values[(2 * index3) + index2 + index]%gui.nLeds()
                    setWidget(gui.sync_model["Digital"][key2][key3]["Board" + str(board_number)], led_number)
                    current_limit[index2] = gui.getAdcCurrentLimit(board_number, led_number+1)
                elif key3 == "PWM":
                    gui.setValue(gui.sync_model["Digital"][key2][key3], sync_values[(2 * index3) + index2 + index]/65535*100)
                elif key3 == "Current":
                    gui.setValue(gui.sync_model["Digital"][key2][key3], sync_values[(2 * index3) + index2 + index]/65535*100)
                elif key3 == "Duration":
                    gui.setValue(gui.sync_model["Digital"][key2][key3], sync_values[(2 * index3) + index2 + index]/1e6)
        index += 10

        #Analog
        for board in range(1, gui.nBoards()+1):
            setWidget(gui.sync_model["Analog"]["Board" + str(board)], sync_values[index+board-1])
        index += gui.nBoards()

        #Confocal
        for index2, key2 in enumerate(["Shutter", "Channel", "Line", "Digital", "Polarity"]):
            if key2 == "Line":
                gui.sync_model["Confocal"][key2].setCurrentIndex(sync_values[index + index2])
            else:
                setWidget(gui.sync_model["Confocal"][key2], sync_values[index+index2])
        index += 5

        gui.setValue(gui.sync_model["Confocal"]["Threshold"], sync_values[index]/65535*3.3)
        setWidget(gui.sync_model["Confocal"]["Delay"]["Mode"], sync_values[index+1])
        gui.setValue(gui.sync_model["Confocal"]["Period"], sync_values[index+2]/DEFAULT_CLOCK_SPEED)
        index += 3

        for index3 in range(3):
            gui.setValue(gui.sync_model["Confocal"]["Delay"][str(index3+1)], sync_values[index+index3]/DEFAULT_CLOCK_SPEED)
        index += 3

        for index3, key3 in enumerate(["Mode", "LED", "PWM", "Current", "Duration"]):
            for index2, key2 in enumerate(["Standby", "Scanning"]):
                if key3 == "Mode":
                    gui.sync_model["Confocal"][key2][key3].setCurrentIndex(sync_values[(2 * index3) + index2 + index])
                if key3 == "LED":
                    board_number = math.floor(sync_values[(2 * index3) + index2 + index]/gui.nLeds())+1
                    led_number = sync_values[(2 * index3) + index2 + index]%gui.nLeds()
                    setWidget(gui.sync_model["Confocal"][key2][key3]["Board" + str(board_number)], led_number)
                    current_limit[index2] = gui.getAdcCurrentLimit(board_number, led_number+1)
                elif key3 == "PWM":
                    gui.setValue(gui.sync_model["Confocal"][key2][key3], sync_values[(2 * index3) + index2 + index]/65535*100)
                elif key3 == "Current":
                    gui.setValue(gui.sync_model["Confocal"][key2][key3], sync_values[(2 * index3) + index2 + index]/65535*100)
                elif key3 == "Duration":
                    gui.setValue(gui.sync_model["Confocal"][key2][key3], sync_values[(2 * index3) + index2 + index]/1e6)

        updateModelWhatsThis(gui, gui.sync_model)
        return True

    else:
        showMessage(gui, "Error: Sync config file had invalid checksum: " + str(checksum) + ". Upload aborted.")
        return False

def configToBytes(gui, prefix, update_model=True):
    global EXT_THERMISTOR_NOMINAL
    global EXT_B_COEFFICIENT
    config_values = [None] * ((2 * gui.nBoards() * gui.nLeds()) + 9)

    byte_array = bytearray() #Initialize empty byte array

    byte_array.extend(gui.getValue(gui.config_model["Driver name"]).ljust(gui.config_model["Driver name"].maxLength(), " ").encode()) #Add string with right padding of spaces for max length of QLineEdit
    byte_array.append(0)

    #Get LED names
    for board_number in range(1, gui.nBoards() + 1):
        for led_number in range(1, gui.nLeds() + 1):
            byte_array.extend(gui.getValue(gui.config_model["LED" + str(board_number) + str(led_number)]["ID"]).ljust(gui.config_model["LED" + str(board_number) + str(led_number)]["ID"].maxLength()," ").encode())  # Add string with right padding of spaces for max length of QLineEdit
            byte_array.append(0)

    #Get LED active state
    index = 0
    for board_number in range(1, gui.nBoards() + 1):
        for led_number in range(1, gui.nLeds() + 1):
            config_values[index] = gui.getValue(gui.config_model["LED" + str(board_number) + str(led_number)]["Active"])
            index += 1

    #Get LED current limits
    for board_number in range(1, gui.nBoards() + 1):
        for led_number in range(1, gui.nLeds() + 1):
            current_limit = gui.getAdcCurrentLimit(board_number, led_number)
            gui.setAdcCurrentLimit(board_number, led_number, current_limit) #Update What's this to new value
            config_values[index] = round((current_limit/100)*65535) #Convert current limit to ADC reading (voltage)
            index += 1

    #Get simultaneous state if in use
    if "Simultaneous LED" in gui.config_model:
        pass
    else:
        config_values[index] = False
    index += 1

    #Get fault temperatures
    config_values[index] = tempToAdc(gui.getValue(gui.config_model["Temperature"]["Warn"]))
    config_values[index+1] = tempToAdc(gui.getValue(gui.config_model["Temperature"]["Fault"]))
    index += 2

    #Get fan temperatures
    config_values[index] = tempToAdc(gui.getValue(gui.config_model["Fan"]["Min"]))
    config_values[index+1] = tempToAdc(gui.getValue(gui.config_model["Fan"]["Max"]))
    index += 2

    #Get alarm volumes
    config_values[index] = gui.getValue(gui.config_model["Audio"]["Status"])
    config_values[index+1] = gui.getValue(gui.config_model["Audio"]["Alarm"])
    config_values[index+2] = gui.config_model["Pushbutton"]["Indication"][1].isChecked()
    index += 3

    for idx, widget in enumerate(gui.config_model["Pushbutton"]["Alarm"]):
        if gui.getValue(widget):
            config_values[index] = idx
            break

    unpack_string = "<"
    # Add a boolean for each LED (LED active)
    for board_number in range(1, gui.nBoards() + 1):
        for led_number in range(1, gui.nLeds() + 1):
            unpack_string += "?"

    # Add a uint16_t for each LED (current limit)
    for board_number in range(1, gui.nBoards() + 1):
        for led_number in range(1, gui.nLeds() + 1):
            unpack_string += "H"

    unpack_string += "?HHHHBB?B"
    byte_array.extend(struct.pack(unpack_string, *config_values))
    checksum = (sum(byte_array) + prefix) & 0xFF  # https://stackoverflow.com/questions/44611057/checksum-generation-from-sum-of-bits-in-python
    checksum = 256 - checksum
    if(checksum == 256):
        checksum = 0
    byte_array.append(checksum)

    return byte_array

def syncToBytes(gui, prefix, update_model=True):
    if update_model:
        updateModelWhatsThis(gui, gui.sync_model)

    sync_values = [None] * (8 + 2*11 + 2*3)
    byte_array = bytearray() #Initialize empty byte array
    index = 0

    unpack_string = "<B"
    #Digital sync
    unpack_string += "BBBBBHHHHLL"
    #Analog sync
    for _ in range(gui.nBoards()):
        unpack_string += "B"
    #Confocal sync
    unpack_string += "?B???H?LLLLBBBBHHHHLL"
    def widgetIndex(widget_list, showerror = True):
        for w_index, n_widget in enumerate(widget_list):
            if gui.getValue(n_widget):
                return w_index
        else:
            if(showerror):
                showMessage(gui, "Error: Widget index not found!")
            return None

    #Digital
    sync_values[0] = gui.sync_model["Mode"].currentIndex()
    sync_values[1] = widgetIndex(gui.sync_model["Digital"]["Channel"])
    current_limit = [0]*2
    index += 2

    for index3, key3 in enumerate(["Mode", "LED", "PWM", "Current", "Duration"]):
        for index2, key2 in enumerate(["Low", "High"]):
            if key3 == "Mode":
                sync_values[(2 * index3) + index2 + index] = gui.sync_model["Digital"][key2][key3].currentIndex()
            if key3 == "LED":
                for board_number in range(1, gui.nBoards()+1):
                    showerror = False #Only show the none error if on the last board and a clicked widget still hasn't been found
                    if board_number == gui.nBoards():
                        showerror = True
                    sync_values[(2 * index3) + index2 + index] = widgetIndex(gui.sync_model["Digital"][key2][key3]["Board" + str(board_number)], showerror)
                    if sync_values[(2 * index3) + index2 + index] is not None:
                        current_limit[index2] = gui.getAdcCurrentLimit(board_number, sync_values[(2 * index3) + index2 + index] + 1)
                        sync_values[(2 * index3) + index2 + index] += gui.nLeds()*(board_number-1) #Add board number offset to final led number
                        break
            elif key3 == "PWM":
                sync_values[(2 * index3) + index2 + index] = round((gui.getValue(gui.sync_model["Digital"][key2][key3])/100)*65535)
            elif key3 == "Current":
                sync_values[(2 * index3) + index2 + index] = round(gui.getValue(gui.sync_model["Digital"][key2][key3])/current_limit[index2]*65535)  # Convert current limit to ADC reading (voltage)
            elif key3 == "Duration":
                sync_values[(2 * index3) + index2 + index] = round(gui.getValue(gui.sync_model["Digital"][key2][key3])*1e6)  # Convert duration to microseconds

    index += 10

    #Analog
    for board_number in range(gui.nBoards()):
        sync_values[index+board_number] = widgetIndex(gui.sync_model["Analog"]["Board" + str(board_number+1)])
    index += gui.nBoards()

    #Confocal
    for index2, key2 in enumerate(["Shutter", "Channel", "Line", "Digital", "Polarity"]):
        if key2 == "Line":
            sync_values[index + index2] = gui.sync_model["Confocal"][key2].currentIndex()
        else:
            sync_values[index+index2] = widgetIndex(gui.sync_model["Confocal"][key2])
    index += 5
    sync_values[index] = round(gui.getValue(gui.sync_model["Confocal"]["Threshold"])/3.3*65535)
    sync_values[index+1] = widgetIndex(gui.sync_model["Confocal"]["Delay"]["Mode"])
    sync_values[index+2] = round(gui.getValue(gui.sync_model["Confocal"]["Period"]) * DEFAULT_CLOCK_SPEED)
    index += 3

    for index3 in range(3):
        sync_values[index+index3] = round(gui.getValue(gui.sync_model["Confocal"]["Delay"][str(index3+1)])*DEFAULT_CLOCK_SPEED) #Convert the delay times to clock cycles at default Teensy speed
    index += 3
    for index3, key3 in enumerate(["Mode", "LED", "PWM", "Current", "Duration"]):
        for index2, key2 in enumerate(["Standby", "Scanning"]):
            if key3 == "Mode":
                sync_values[(2 * index3) + index2 + index] = gui.sync_model["Confocal"][key2][key3].currentIndex()
            if key3 == "LED":
                for board_number in range(1, gui.nBoards() +1):
                    showerror = False #Only show the none error if on the last board and a clicked widget still hasn't been found
                    if board_number == gui.nBoards():
                        showerror = True
                    sync_values[(2 * index3) + index2 + index] = widgetIndex(gui.sync_model["Confocal"][key2][key3]["Board" + str(board_number)], showerror)
                    if sync_values[(2 * index3) + index2 + index] is not None:
                        current_limit[index2] = gui.getAdcCurrentLimit(board_number, sync_values[(2 * index3) + index2 + index] + 1)
                        sync_values[(2 * index3) + index2 + index] += gui.nLeds() * (board_number - 1)  # Add board number offset to final led number
                        break
            elif key3 == "PWM":
                sync_values[(2 * index3) + index2 + index]  = round((gui.getValue(gui.sync_model["Confocal"][key2][key3]) / 100) * 65535) #Convert to clock-cycles, where 100% = # of clock cycles in delay #2
            elif key3 == "Current":
                sync_values[(2 * index3) + index2 + index] = round(gui.getValue(gui.sync_model["Confocal"][key2][key3])/current_limit[index2]*65535) # Convert current to ADC reading (voltage) as percent of current limit
            elif key3 == "Duration":
                sync_values[(2 * index3) + index2 + index] = round(gui.getValue(gui.sync_model["Confocal"][key2][key3])*1e6)

    byte_array.extend(struct.pack(unpack_string, *sync_values))
    checksum = (sum(byte_array) + prefix) & 0xFF  # https://stackoverflow.com/questions/44611057/checksum-generation-from-sum-of-bits-in-python
    checksum = 256 - checksum
    if(checksum == 256):
        checksum = 0
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

