from collections import OrderedDict
import re
from cobs import cobs
from PyQt5 import QtCore, QtWidgets
from PyQt5.QtSerialPort import QSerialPortInfo, QSerialPort
import inspect
from collections import OrderedDict
import guiConfigIO as fileIO
import time
import struct
import guiSequence as seq
import guiMapper
import tempfile
import sys
from timeit import default_timer as timer
import traceback
import pyautogui

# Teensy USB serial microcontroller program id data:
VENDOR_ID = 0x16C0
PRODUCT_ID = 0x0483
SERIAL_NUMBER = "30MMLED"
MAGIC_SEND = "51ERrUAT6ZWlThiltxJK" #Magic number sent to Teensy to verify that they are an LED driver
MAGIC_RECEIVE = "A5DihJ3v5bbXKmAmmhQl" #Magic number received from Teensy verifying it is an LED driver
HEARTBEAT_INTERVAL = 5 #Send a heartbeat signal every 5 seconds after the last packet was transmitted
debug = True #Show all serial debug messages excluding status updates
debug_status = False #Also show status messages


class usbSerial(QtWidgets.QWidget): #Implementation based on: https://stackoverflow.com/questions/55070483/connect-to-serial-from-a-pyqt-gui
    def __init__(self, gui, parent=None):
        super(usbSerial, self).__init__(parent)
        self.gui = gui
        self.debug = True #Prints debug messages
        self.ser_num = None #Serial number of the USB connected device
        self.com_list_teensy = []  # List of USB COM ports that have the same VENDOR_ID and PRODUCT_ID as a Teensy
        self.com_list_custom = [] #List of valid Teensy COM ports with a custom serial number that is "MHZ_LEDXX:
        self.com_list_verified = OrderedDict() #List of verified LED driver ports and their attributes
        self.active_port = None #Active serial connection, None if no port is currently active
        self.serial_buffer = [] #Stores incoming serial stream
        self.command_queue = [] #List of parsed and cobs decoded
        self.prefix_dict = {} #byte prefix identifying data packet type
        self.in_prefix_dict = {}  # byte prefix identifying data packet type
        self.command_dict = {} #Mapping of prefix to function that will process the command
        self.dropped_frame_counter = 0 #Track total number of invalid frames
        self.default_action_number = len(self.gui.menu_connection.actions()) #Get number of actions in connection menu when there are no connected drivers
        self.default_serial_number = self.gui.configure_name_driver_serial_label2.text()
        self.seq_table_list = guiMapper.initializeSeqList(self.gui)
        #Initialize connection menu action group
        self.conn_menu_action_group = QtWidgets.QActionGroup(self.gui.menu_connection) #Connection menu action group to have options act like radio buttons
        self.conn_menu_action_group.setExclusive(True)
        self.conn_menu_action_group.triggered.connect(self.onTriggered)
        self.upload_stream_buffer = [] #Buffer for storing active data upload streams - used to send large files
        self.download_stream_size = None #Expected size of download stream to be received - including prefix byte
        self.expected_callback = None #Expected callback function - used when GUI expects a reply from the driver to verify data is received in order
        self.download_all_seq = False #Whether just one sequence file, or all sequence files are to be downloaded
        self.stream_download_timeout = 0 #Unix time to wait for complete non-COBS stream packet before timing out and clearing the stream flag
        self.initializing_connection = True #Flag to suppress unnecessary notifications if connection is being initialized
        self.stop_receive = False #Blocks receive thread when a packet is being processed
        self.heartbeat_timer = timer() #Timer to track if a heartbeat signal needs to be sent
        self.autoclick_mouse = False #Automatically click the mouse when the sync switches to the set state
        self.autoclick_state = False #State of the sync status that will trigger an auto-mouse click
        self.autoclick_position = (0, 0) #Position to click mouse

        for action in self.gui.menu_connection.actions():
            self.conn_menu_action_group.addAction(action)

        #Initialize prefix routing dicts
        self.initializeRoutingDictionaries()

    #https://forum.pjrc.com/threads/25295-Automatically-find-a-Teensy-Board-with-Python-and-PySerial
    def getDriverPort(self, on_boot = False):
        for self.port in list(QSerialPortInfo.availablePorts()):
            port_info = self.getPortInfo(self.port)
            if port_info["Vendor"] == VENDOR_ID and port_info["Product"] == PRODUCT_ID: #Search for COM ports that have correct vendor and product IDs
                self.com_list_teensy.append(port_info)
                try: #See if the connection has a valid custom serial number
                    ser_num = re.search('MHZ_LED[A-Z0-9_-][A-Z0-9_-]', port_info["Serial"]).group(0)
                    self.com_list_custom.append((self.port[0], ser_num))
                except AttributeError:
                    pass

        if len(self.com_list_teensy) > 0: #If at least one Teensy was found, exchange magic numbers to confirm it is an LED driver
            port_list = self.com_list_teensy
            if len(self.com_list_custom) > 0: #If at least one custom serial number was found, only check devices with custom serial numbers
                port_list = self.com_list_custom
            for port_info in port_list:
                if self.connectSerial(port_info["Port"]):
                    self.magicNumberCheck()
                    self.uploadTime()
                    self.disconnectSerial()
        if on_boot: #On boot, automatically connect to the first driver in the menu
            action = self.gui.menu_connection.actions()[0]
            action.setChecked(True)
            self.onTriggered(action)
        else: #If check was performed through menu, inform of result
            if len(self.gui.menu_connection.actions()) > self.default_action_number:
                self.showMessage("Success: " + str(len(self.gui.menu_connection.actions()) - self.default_action_number) + " LED driver(s) were found.")
            else:
                self.showMessage("No LED drivers were found. Make sure the following:\n1) USB cables are connected properly\n2) No other program is connected to the LED driver\n3) The LED driver software has been uploaded to the Teensy board")

    def getPortInfo(self, port):
        return {"Vendor": QSerialPortInfo(port).vendorIdentifier(),
                "Product": QSerialPortInfo(port).productIdentifier(),
                "Serial": QSerialPortInfo(port).serialNumber(),
                "Port": QSerialPortInfo(port).systemLocation()}

    def connectSerial(self, port):
        #try:
            self.active_port = QSerialPort(port, baudRate=QSerialPort.Baud9600, readyRead=self.receive)
            if not self.active_port.isOpen(): #Close serial port if it is already open
                if self.active_port.open(QtCore.QIODevice.ReadWrite): #Open serial connection
                    self.active_port.readyRead.connect(self.receive)
                    self.active_port.clear() #Clear buffer of any remaining data
                    self.gui.status_dict["COM Port"] = self.getPortInfo(self.active_port)["Port"]
                    self.active_port.errorOccurred.connect(self.disconnectSerial) #Add signal for a connection error -
                    return True
                else:
                    if debug:
                        print("Can't open port1")
            else:
                if debug:
                    print("Can't open port2")

        #except: #Return False if unable to establish connection to serial port
            if debug:
                print("Failed to connect to COM port, with QSerialPort Error #" + str(self.active_port.error()))
            self.disconnectSerial()
            return False

    def disconnectSerial(self, error = None):
        if error in [None, QSerialPort.SerialPortError.ResourceError, QSerialPort.SerialPortError.DeviceNotFoundError]:
            if error == QSerialPort.SerialPortError.ResourceError:
                self.showMessage("Error: Serial port disconnected (Resource error)")
                self.active_port.close() #close connection
            elif error == QSerialPort.SerialPortError.DeviceNotFoundError:
                self.showMessage("Error: Serial port disconnected (Device not found)")
                self.active_port.close()  # close connection
            if self.active_port is not None:
                error = self.active_port.error()
                if self.active_port.isOpen() and error == 12: #Close serial port if it is already open
                    self.sendWithoutReply()  #Infrom the LED driver of disconnect
                self.active_port.clear() #Clear buffer of any remaining data
                self.active_port.close() #close connection
                self.active_port = None

            self.gui.menu_connection_disconnect.setChecked(True)
            self.gui.updateSerialNumber(self.default_serial_number)
            self.gui.status_dict["COM Port"] = "Disconnect"

    @QtCore.pyqtSlot()
    def receive(self):
        if self.stop_receive: #If processing a stream - do not process incoming packets
            pass
        else:
            if self.active_port is not None:  # Should be redundant - better safe than sorry
                temp_buffer = bytearray(self.active_port.readAll().data())
    #            print("Temp buffer: " + str(temp_buffer))
                if self.download_stream_size and self.stream_download_timeout: #If stream is expected and it has timed out, clear the serial buffer before proceeding
                    if time.time() > self.stream_download_timeout:  # Check to make sure that stream has not yet timed out
                        self.showMessage("Error: Stream download timed out with " + str(len(self.serial_buffer)) + " of " + str(self.download_stream_size) + " bytes received. Stream aborted.")
                        self.serial_buffer = []
                        self.download_stream_size = None  # Clear download stream flag
                        self.stream_download_timeout = None #Clear timeout timer

                for i, byte in enumerate(temp_buffer):
                    if self.download_stream_size: #Check if non-COBS encoded data stream is expected
                        self.serial_buffer.append(byte)
                        if self.download_stream_size == len(self.serial_buffer): #If streaming is active and full length stream is received, send it to the command queue
                            self.stop_receive = True
                            self.command_queue.append(self.serial_buffer)
                            self.serial_buffer = []
                            self.serialRouter()
                            self.stop_receive = False

                        elif self.serial_buffer:
                            if self.serial_buffer[0] == 1: #If a message packet is being received in place of the stream - clear stream flag so message can be processed
                                self.download_stream_size = None  # Clear download stream flag
                                self.stream_download_timeout = None  # Clear timeout timer

                    elif byte == 0:
                        try:
                            self.command_queue.append(cobs.decode(bytes(self.serial_buffer)))
                            self.serial_buffer = []
                            self.serialRouter()

                        except cobs.DecodeError:
                            #self.showMessage("Warning: Invalid COBS frame received from driver. Check connection.")
                            if debug:
                                print("Invalid COBS packet")
                                print(temp_buffer[:i])
                                print(self.serial_buffer)
                            self.serial_buffer = []
                            self.dropped_frame_counter += 1

                    else:
                        self.serial_buffer.append(byte)
            return True

    @QtCore.pyqtSlot()
    def send(self, message = None, cobs_encode = True):
        self.heartbeat_timer = timer()  # Reset heartbeat timer
        if self.active_port is None: #If driver is disconnected, then don't try to send packet
            return
        if cobs_encode:
            packet = bytearray(self.prefix_dict[inspect.stack()[2].function].to_bytes(1, "big")) #Add routing prefix based on name of calling function
            if message:
                if isinstance(message, str):
                    message = bytearray(message.encode())
                elif isinstance(message, int):
                    message = message.to_bytes(1, "big")
                packet.extend(bytearray(message))
            self.gui.splashText("Func: " + str(inspect.stack()[2].function) + ", Tx: " + str(packet))
            if debug:
                print("Func: " + str(inspect.stack()[2].function) + ", Tx: " + str(packet))
            self.active_port.write(cobs.encode(bytes(packet)))
            self.active_port.write(bytes(1))  # Send NULL framing byte
        else:
            self.gui.splashText("Func: " + str(inspect.stack()[2].function) + ", Tx: " + str(message))
            if debug:
                print("Func: " + str(inspect.stack()[2].function) + ", Tx: " + str(message[:100]))
                if len(message) > 100:
                    print("↑ Total tx packet length: " + str(len(message)))
            bytes_written = self.active_port.write(message)
            if bytes_written != len(message):
                self.showMessage("Error: Only " + str(bytes_written) + " of " + str(len(message)) + " were sent to LED driver.  Please check connection.")

        wait_time = 200
        if message:
            wait_time += round(len(message) / 10) #adjust the wait time according to the size of the packet to be transmitted
        if self.active_port.waitForBytesWritten(wait_time): #Wait for data to be sent
            pass
        else:
            if not self.initializing_connection:
                self.showMessage("Error: Message buffer failed to be sent to driver, please check driver connection.")
                self.disconnectSerial()

    def onTriggered(self, action):
        if str(action.objectName()) == "menu_connection_disconnect":
            self.disconnectSerial()
        elif str(action.objectName()) == "menu_connection_search":
            self.getDriverPort()
        else:
            port = action.toolTip()
            serial_number = action.whatsThis()
            if self.connectSerial(port):
                self.initializing_connection = True
                self.downloadDriverConfiguration()
                self.updateStatus()
                self.gui.updateSerialNumber(serial_number)
                self.downloadSyncConfiguration()

            else:
                self.conn_menu_action_group.removeAction(action)
                self.gui.menu_connection.removeAction(action)
                self.showMessage("Error: Failed to open LED driver port.  Check USB connection and confirm no other software is connected to the driver.")

    def serialRouter(self):
        while self.command_queue: #Process all commands in the queue
            command = bytearray(self.command_queue.pop(0))
            self.gui.splashText("Rx: " + str(command))
            if debug:
                if command[0] != self.prefix_dict["updateStatus"] or debug_status:
                    print("Rx: " + self.command_dict[command[0]].__name__ + " " +str(command[:100]))
                    if len(command) > 50:
                        print("↑ Total tx packet length: " + str(len(command)))
            try:
                if self.expected_callback:
                    if command[0] not in [self.expected_callback, self.prefix_dict["showDriverMessage"], self.prefix_dict["updateStatus"]]:
                        self.showMessage("Warning: Waiting for reply to \"" + str(self.command_dict[self.expected_callback].__name__) + "\" and received a packet for \"" + str(self.command_dict[command[0]].__name__) + "\" instead.")
                        if debug:
                            print("Warning: Waiting for reply to \"" + str(self.command_dict[self.expected_callback].__name__) + "\" and received a packet for \"" + str(self.command_dict[command[0]].__name__) + "\" instead.")
                    else: #Clear callback if prefix is valid
                        self.expected_callback = None
                self.command_dict[command[0]](command[1:])
                if debug:
                    if command[0] != self.prefix_dict["updateStatus"] or debug_status:
                        print("Frame processed. " + str(self.dropped_frame_counter) + " dropped frames so far.")
            except KeyError:
                if debug:
                    print(traceback.format_exc())
                    print("Invalid prefix: " + str(command[0]))
                self.dropped_frame_counter += 1

    def initializeRoutingDictionaries(self):
        self.prefix_dict = {"showDriverMessage": 0, # byte prefix identifying data packet type
                            "magicNumberCheck": 1,
                            "downloadDriverConfiguration": 2,
                            "uploadDriverConfiguration": 3,
                            "downloadSyncConfiguration": 4,
                            "uploadSyncConfiguration": 5,
                            "downloadSeqFile": 6,
                            "uploadSeqFile": 7,
                            "downloadDriverId": 8,
                            "uploadTime": 9,
                            "uploadStream": 10,
                            "downloadStream": 11,
                            "updateStatus": 12,
                            "driverCalibration": 13,
                            "disconnectSerial": 14,
                            "measurePeriod": 15,
                            "testCurrent": 16,
                            "testVolume": 17}

        self.command_dict = {self.prefix_dict["showDriverMessage"]: self.showDriverMessage, # Mapping of prefix to function that will process the command
                             self.prefix_dict["magicNumberCheck"]: self.magicNumberCheck,
                             self.prefix_dict["downloadDriverConfiguration"]: self.downloadDriverConfiguration,
                             self.prefix_dict["uploadDriverConfiguration"]: self.uploadDriverConfiguration,
                             self.prefix_dict["downloadSyncConfiguration"]: self.downloadSyncConfiguration,
                             self.prefix_dict["uploadSyncConfiguration"]: self.uploadSyncConfiguration,
                             self.prefix_dict["downloadSeqFile"]: self.downloadSeqFile,
                             self.prefix_dict["uploadSeqFile"]: self.uploadSeqFile,
                             self.prefix_dict["downloadDriverId"]: self.downloadDriverId,
                             self.prefix_dict["uploadTime"]: self.uploadTime,
                             self.prefix_dict["uploadStream"]: self.uploadStream,
                             self.prefix_dict["downloadStream"]: self.downloadStream,
                             self.prefix_dict["updateStatus"]: self.updateStatus,
                             self.prefix_dict["disconnectSerial"]: self.disconnectSerial,
                             self.prefix_dict["measurePeriod"]: self.measurePeriod,
                             self.prefix_dict["testCurrent"]: self.testCurrent,
                             self.prefix_dict["testVolume"]: self.testVolume}

    def showDriverMessage(self, reply=None):
        if reply is not None:
            reply = reply.decode()
            if reply == "Sync and sequence files were successfully uploaded.":
                self.gui.sync_update_signal.emit(None)  # Flag that the active sync state has changed
            self.showMessage(reply)
        else:
            if self.portConnected():
                self.sendWithoutReply(None, True, 0) #Send empty heartbeat packet

    def magicNumberCheck(self, reply=None):
        if reply is not None:
            reply = reply.decode()
            if str(reply) == MAGIC_RECEIVE:
                self.downloadDriverId()
        else:
            if self.portConnected():
                self.sendWithoutReply(MAGIC_SEND)

    def downloadDriverId(self, reply=None):
        if reply is not None:
            reply = reply.decode().rstrip()
            menu_item = QtWidgets.QAction(reply, self.gui)
            menu_item.setToolTip(self.getPortInfo(self.active_port)["Port"])  # Add port# to tool tip to distinguish drivers with identical names
            menu_item.setWhatsThis(self.getPortInfo(self.active_port)["Serial"])  # Add port# to tool tip to distinguish drivers with identical names
            menu_item.setCheckable(True)
            menu_item.setChecked(False)
            for action in self.gui.menu_connection.actions():
                if action.toolTip() == menu_item.toolTip():
                    break
            else:
                self.gui.menu_connection.insertAction(self.gui.menu_connection_disconnect, menu_item)
                self.conn_menu_action_group.addAction(menu_item)
        else:
            if self.portConnected():
                self.sendWithReply(self.prefix_dict["downloadDriverId"])

    def uploadTime(self, reply=None):
        if reply is not None:
            pass
        else:
            if self.portConnected():
                time_now = round(time.mktime(time.localtime())) - time.timezone
                time_now = bytearray(struct.pack("<L", int(time_now)))
                self.sendWithoutReply(time_now)

    def downloadDriverConfiguration(self, reply=None):
        if reply is not None:
            fileIO.bytesToConfig(reply, self.gui, self.prefix_dict["downloadDriverConfiguration"])
        else:
            if self.portConnected():
                self.sendWithReply(self.prefix_dict["downloadDriverConfiguration"])

    def uploadDriverConfiguration(self, reply=None):
        if reply is not None:
            pass
        else:
            if self.portConnected():
                self.sendWithoutReply(fileIO.configToBytes(self.gui, self.prefix_dict["uploadDriverConfiguration"]))

    def downloadSyncConfiguration(self, reply=None):
        if reply is not None:
            if fileIO.bytesToSync(reply, self.gui, self.prefix_dict["downloadSyncConfiguration"]):
                self.downloadSeqFile()
            else:
                self.showMessage("Error: Invalid Sync configuration packet was received.")
        else:
            if self.portConnected():
                self.gui.startSplash("download")
                self.sendWithReply(self.prefix_dict["downloadSyncConfiguration"])

    def uploadSyncConfiguration(self, reply=None):
        if reply is not None:
            pass
        else:
            if self.portConnected():
                self.gui.startSplash("upload")
                message = fileIO.syncToBytes(self.gui, self.prefix_dict["uploadSyncConfiguration"])
                self.sendWithReply(self.prefix_dict["uploadSeqFile"], message)

    def downloadSeqFile(self, reply=None, widget=None):
        message = bytearray()
        if reply is not None:
            if self.download_stream_size: #If stream is active, process streamed sequence file data
                self.download_stream_size = None
                self.stream_download_timeout = None
                if self.download_all_seq: #If all sequence are to be downloaded, request next sequence file for download
                    seq_id = reply.pop(0) #Retrieve sequence file ID from list
                    seq.bytesToSequence(reply, self.gui, self.seq_table_list[seq_id])
                    if seq_id < seq.n_sequence_files-1:
                        message.extend(struct.pack("B", seq_id+1))
                        self.sendWithReply(self.prefix_dict["downloadSeqFile"], message)

                    else:
                        self.download_all_seq = False  #If end of sequence file list is reached, clear download all flag
                        self.showDriverMessage()  # Start status update stream
                        self.gui.splash.close()
                        if self.initializing_connection:
                            self.updateStatus() #Send GUI status to driver on successful connection
                            self.initializing_connection = False
                        else:
                            self.gui.sync_update_signal.emit(None)  # Flag that the active sync state has changed
                            self.showMessage("Sync and sequence files were successfully uploaded.")

            elif len(reply) == 4: #If stream is not active, reply is stream initialization showing length of stream to be received
                self.download_stream_size = struct.unpack("<L", reply)[0]
                self.stream_download_timeout = time.time() + 0.5 + self.download_stream_size / 10000
                self.sendWithReply(self.prefix_dict["downloadSeqFile"]) #Reply that ready for stream start
            else:
                self.showMessage("Error: Invalid downloadSeq packet received.")

        else:
            if self.portConnected():
                if widget:
                    for index, ref_widget in enumerate(self.seq_table_list):
                        if widget == ref_widget or widget == index: #Widget could be the calling widget object or a numerical index identifier
                            message.extend(struct.pack("B", index))
                            self.sendWithReply(self.prefix_dict["downloadSeqFile"], message)
                else: #If no widget was specified, download the first sequence file
                    message.extend(struct.pack("B", 0))
                    self.download_all_seq = True #Flag that all sequence files are to be downloaded
                    self.sendWithReply(self.prefix_dict["downloadSeqFile"], message)

    def uploadSeqFile(self, reply=None, widget=None):
        message = bytearray()
        if reply is not None:
            message.extend(struct.pack("B", self.prefix_dict["uploadSeqFile"]))
            message.extend(reply)
            self.upload_stream_buffer = seq.sequenceToBytes(self.gui, self.seq_table_list[ord(reply)])
            message.extend(struct.pack("<L", len(self.upload_stream_buffer)))
            if len(self.upload_stream_buffer) > 0: #If there is a file to stream, expect reply to start stream
                self.sendWithReply(self.prefix_dict["uploadStream"], message, False)
            else: #If no file is to be streamed, expect reply requesting next file
                self.sendWithReply(self.prefix_dict["uploadSeqFile"], message, False)

        else:
            if self.portConnected():
                for index, ref_widget in enumerate(self.seq_table_list):
                    if widget == ref_widget or widget == index: #Widget could be the calling widget object or a numerical index identifier
                        message.extend(struct.pack("B", index))
                        self.sendWithReply(self.prefix_dict["uploadSeqFile"], message)

    def uploadStream(self, message):
        if self.portConnected():
            self.sendWithoutReply(self.upload_stream_buffer, False)
            self.upload_stream_buffer = []  # Clean stream buffer

    def downloadStream(self, message):
        pass

    def updateStatus(self, reply=None, force_tx = False):
        unpack_string = "<"
        # String for LED info
        for byte in ["B", "H", "H"]:
            for board_number in range(1, self.gui.nBoards() + 1):
                unpack_string += byte

        unpack_string += "B??"

        # Temp and fan info
        for byte in ["H", "H"]:
            for _ in range(1, self.gui.nBoards() + 1): #https://www.datacamp.com/tutorial/role-underscore-python
                unpack_string += byte

        if reply:
            #parse status
            status_change = False
            status_list = struct.unpack(unpack_string, reply)

            for index, key in enumerate(self.gui.status_dynamic_dict):
                self.gui.status_dynamic_dict[key] = status_list[index]
                if self.gui.status_dynamic_dict[key] != self.gui.status_dict[key]: #Update master dictionary if a value has changed

                   #Mouse autoclicker for Z-series
                    if (self.autoclick_mouse and key == "Mode" and self.gui.status_dynamic_dict["Control"] and  self.gui.status_dynamic_dict["Mode"] == 0): #If the driver has control, and the mode switched to sync, get the mouse position
                        self.autoclick_position = pyautogui.position()
                        print("Autoclick position set: " + str(self.autoclick_position))
                    if (self.autoclick_mouse and key == "State" and self.gui.status_dynamic_dict[key] == self.autoclick_state and self.gui.status_dynamic_dict["Mode"] == 0 and self.gui.status_dynamic_dict["Control"]): #If the sync status has changed to false, click the mouse
                        print("Mouse autoclick")
                        pyautogui.leftClick(self.autoclick_position)

                    self.gui.status_dict[key] = self.gui.status_dynamic_dict[key]
                    status_change = True

            #If status has changed, emit status change signal with the new status dictionary
            if status_change:
                self.gui.status_signal.emit(self.gui.status_dynamic_dict)

            # Send heartbeat packet in reply
            if (timer() - self.heartbeat_timer) > HEARTBEAT_INTERVAL:
                self.showDriverMessage()
                self.heartbeat_timer = timer()
        else:
            if self.portConnected():
                def widgetIndex(widget_list):
                    nonlocal self
                    for w_index, n_widget in enumerate(widget_list):
                        if self.gui.getValue(n_widget) in [True, 1]:
                            if(n_widget is self.gui.main_model["Mode"][0]): #if the slider has a value of 1 this means mode is 0
                                w_index = 0
                            return w_index
                    else:
                        #self.gui.showMessage("Error: Widget index not found!")
                        return None
                if widgetIndex(self.gui.main_model["Control"]) == 0 or force_tx: #Send a status control command only if GUI has control
                    status_list = [0] * (5*self.gui.nBoards() + 3)
                    led_dict = {"channel": [None]*self.gui.nBoards(), "pwm": [None]*self.gui.nBoards(), "current": [None]*self.gui.nBoards()}
                    mode = widgetIndex(self.gui.main_model["Mode"])
                    dial_max = self.gui.main_model["Intensity"].maximum()
                    for board in range(1, self.gui.nBoards()+1):
                        led_dict["channel"][board-1] = widgetIndex(self.gui.main_model["Channel"]["Board" + str(board)])
                        if led_dict["channel"][board-1] is not None:
                            led_number = led_dict["channel"][board-1]+1
                            if mode == 1: #PWM mode
                                led_dict["pwm"][board-1] = round((self.gui.getValue(self.gui.main_model["Intensity"]) / dial_max) * 65535)
                                led_dict["current"][board-1] = self.gui.getAdcCurrentLimit(board, led_number)
                            elif mode == 2: #Current mode
                                led_dict["pwm"][board - 1] = 65535
                                led_dict["current"][board-1] = round((self.gui.getValue(
                                    self.gui.main_model["Intensity"]) / dial_max) * self.gui.getAdcCurrentLimit(board, led_number))
                            else: #Off mode or sync mode
                                led_dict["current"][board-1] = 0
                                led_dict["pwm"][board-1] = 0
                                led_dict["channel"][board - 1] = self.gui.nLeds()
                        else:
                            led_dict["channel"][board-1] = self.gui.nLeds()
                            led_dict["current"][board-1] = 0
                            led_dict["pwm"][board-1] = 0
                    led_dict["channel"][board - 1]
                    # Send only GUI states - set all driver specific values to 0 since they are just padding
                    for board in range(0, self.gui.nBoards()):
                        status_list[board] = led_dict["channel"][board]
                        status_list[self.gui.nBoards() + board] = led_dict["pwm"][board]
                        status_list[2*self.gui.nBoards() + board] = led_dict["current"][board]
                    status_list[3*self.gui.nBoards()] = mode
                    status_list[3*self.gui.nBoards()+2] = widgetIndex(self.gui.main_model["Control"])
                    status_list = struct.pack("<BBBHHHHHHB??HHHHHH", *status_list)
                    self.sendWithoutReply(status_list, True, 0)

    def measurePeriod(self, reply=None):
        if reply:
            mirror_period = struct.unpack("<f", reply)[0]
            self.gui.setValue(self.gui.sync_model["Confocal"]["Period"], mirror_period)
        else:
            if self.portConnected():
                message = fileIO.syncToBytes(self.gui, self.prefix_dict["measurePeriod"])
                self.sendWithoutReply(message, True, 100)  #Send temporary sync to be used to measure period

    def testCurrent(self, reply=None):
        if reply:
            led_active = struct.unpack("<????", reply)
            for index, led_state in enumerate(led_active):
                self.gui.setValue(self.gui.config_model["LED" + str(index+1)]["Active"], led_state)
        else:
            if self.portConnected():
                message = fileIO.configToBytes(self.gui, self.prefix_dict["testCurrent"])
                self.sendWithoutReply(message, True, 100)

    def testVolume(self, reply=None, indication_id=None):
        def widgetIndex(widget_list):
            nonlocal self
            for w_index, n_widget in enumerate(widget_list):
                if self.gui.getValue(n_widget):
                    return w_index
            else:
                self.gui.showMessage("Error: Widget index not found!")
                return None

        if reply:
            pass
        else:
            if self.portConnected() and indication_id is not None:
                if indication_id == 0:
                    volume = self.gui.getValue(self.gui.config_model["Audio"]["Status"])
                    mode = 0
                else:
                    volume = self.gui.getValue(self.gui.config_model["Audio"]["Alarm"])
                    mode = widgetIndex(self.gui.config_model["Pushbutton"]["Alarm"])
                message = struct.pack("<BBB", *[indication_id, volume, mode])
                self.sendWithoutReply(message, True, 10)  # Sent volume test command

    def portConnected(self):
        if self.active_port is None:
            self.showMessage("Error: LED driver is disconnected.")
            return False  ###################################################################################ADD CODE TO SET MENU TO DISCONNECT AND REMOVE THIS DRIVER FROM MENU LIST#################################################################
        return True

    def sendWithReply(self, callback, message=None, cobs_encode=True, wait_time=500):
        self.expected_callback = callback
        self.send(message, cobs_encode)
        if self.active_port is not None:
            self.active_port.waitForReadyRead(wait_time)


    def sendWithoutReply(self, message=None, cobs_encode=True, wait_time=500):
        self.expected_callback = None
        self.send(message, cobs_encode)
        if self.active_port is not None:
            self.active_port.waitForReadyRead(wait_time)

    def showMessage(self, text):
        self.gui.waitCursor(False)
        self.gui.stopSplash()
        self.gui.message_box.setText(text)
        self.gui.message_box.exec()
