import csv
import math
import tempfile
from PyQt5 import QtGui, QtCore, QtWidgets
from PyQt5.QtWidgets import QMessageBox
import guiConfigIO as FileIO
import struct
from collections import OrderedDict
import os

maximum_rows = 10000 # Maximum number or rows allowed
row_subsample = maximum_rows  #Only load the last segment of the table for very large tables to prevent lag
n_sequence_files = 4 #Total number of sequence tables in Sync configuration

def loadSequence(gui, widget, get_path=False):  # derived from - https://stackoverflow.com/questions/12608835/writing-a-qtablewidget-to-a-csv-or-xls
    if get_path:
        path = getSequencePath(gui, widget)
    else:
        path = None
    if path is None:
        path = QtWidgets.QFileDialog.getOpenFileName(gui, 'Open File', '', 'CSV(*.csv)')[0] #If no path is specified, ask for valid path

    gui.waitCursor(True)
    if path: #If path is specified, load file
        try: #Try to open file at path
            # Count number of rows in CSV file
            with open(str(path), 'rU') as stream: #Verify that the seq table is valid
                widget_headers = verifySequence(gui, stream, widget)

            # Import csv file
            if widget_headers: #Load valid seq table into widget
                with open(str(path), 'rU') as stream:
                    reader = csv.reader(stream)

                    # Remove header from stream
                    next(reader, None)

                    # Clear table
                    widget.setRowCount(0)
                    widget.itemChanged.disconnect() #Speed up CSV loading by preventing widget from validating every cell as data is loaded
                    row = widget.rowCount() #Initialize row index with value for when empty file is loaded
                    for row_data in reader:
                        row = widget.rowCount()
                        if row < maximum_rows:
                            widget.insertRow(row)
                            for column, data in enumerate(row_data):
                                if column < len(widget_headers):
                                    item = QtWidgets.QTableWidgetItem(str(data))
                                    widget.setItem(row, column, item)
                        else:
                            gui.waitCursor(False)
                            showMessage(gui, "The maximum number of rows is capped at " + str(maximum_rows) + ".")
                            break

                    if row < maximum_rows-1:
                        widget.insertRow(row+1) #Add one extra row to allow for editing
                    if widget.rowCount() < 4:
                        widget.setRowCount(4) #Ensure there are at least 4 rows
                    widget.itemChanged.connect(verifyCell)

        except FileNotFoundError:
            showMessage(gui, "\"" + str(path) + "\" is not a valid path. Please find the sequence file manually.")
            setSequencePath(gui, widget, None) #Clear invalid path from model
            loadSequence(gui, widget)

        else:
            setSequencePath(gui, widget, str(path))
    gui.waitCursor(False)

def saveSequence(gui, widget, get_path=None):  # derived from - https://stackoverflow.com/questions/12608835/writing-a-qtablewidget-to-a-csv-or-xls
    if get_path:
        path = getSequencePath(gui, widget)
    else:
        path = None
    if not path: #If no path is specified, ask for valid path
        path = QtWidgets.QFileDialog.getSaveFileName(gui, 'Save File', '', 'CSV(*.csv)')[0]

    gui.waitCursor(True)
    if path: #If path is valid, save
        widget_header_obj = [widget.horizontalHeaderItem(c) for c in range(widget.columnCount())]
        widget_headers = [x.text() for x in widget_header_obj if x is not None]
        with tempfile.TemporaryFile(mode="w+", suffix=".csv", newline='') as stream:  # "newline=''" removes extra newline from windows - https://stackoverflow.com/questions/3191528/csv-in-python-adding-an-extra-carriage-return-on-windows
            writer = csv.writer(stream)
            writer.writerow(widget_headers)
            for row in range(widget.rowCount()):
                row_data = [None] * len(widget_headers)
                for column in range(widget.columnCount()):
                    item = widget.item(row, column)
                    if item is not None:
                        row_data[column] = str(item.text())

                if row_data != [None] * len(widget_headers): #Exclude empty rows
                    writer.writerow(row_data)

            widget_headers = verifySequence(gui, stream, widget)

            if widget_headers:
                try: #Try to open specific path
                    with open(str(path), 'w', newline='') as output_file:
                        stream.seek(0)
                        file_writer = csv.writer(output_file)
                        for row_data in csv.reader(stream):
                            file_writer.writerow(row_data)

                except FileNotFoundError:
                    gui.waitCursor(False)
                    showMessage(gui, str(path) + " is not a valid path. Please find the sequence file manually.")
                    setSequencePath(gui, widget, None)  # Clear invalid path from model
                    saveSequence(gui, widget, None)

                else:
                    setSequencePath(gui, widget, str(path))
    gui.waitCursor(False)

def findUnsavedSeqThenSave(gui, model):
    seq_list = [["Digital", "Low"], ["Digital", "High"], ["Confocal", "Standby"], ["Confocal", "Scanning"]]
    for dictionary in seq_list:
        if model[dictionary[0]][dictionary[1]]["Sequence"] is None:
            showMessage(gui, "Sequence table \"" + dictionary[0] + "::" + dictionary[1] + "\" has not yet been saved. Please save sequence files first if you want them included with the configuration.")
    FileIO.saveConfiguration(gui, model)


def verifySequence(gui, stream, widget):
    stream.seek(0)
    row_count = sum(1 for row in csv.reader(stream))

    # Verify file
    stream.seek(0)
    reader = csv.reader(stream)

    # Verify header
    csv_headers = next(reader, None)
    widget_header_obj = [widget.horizontalHeaderItem(c) for c in range(widget.columnCount())]
    widget_headers = [x.text() for x in widget_header_obj if x is not None]
    if csv_headers is not None and row_count > 1 and len(csv_headers) >= len(widget_headers):
        for column, data in enumerate(widget_headers):
            if csv_headers[column] != data and column < 4:
                showMessage(gui, 
                    "Error: CSV header \"" + data + "\" does not match table header \"" + widget_headers[
                        column] + "\". Process aborted.")
                return False

    # Verify data
    for row, row_data in enumerate(reader):
        if len(widget_headers) > len(row_data):
            showMessage(gui, 
                "Error: Row #" + str(row + 1) + " has only " + str(len(row_data)) + " cells. At least " + str(
                    len(widget_headers)) + " cells are required.  Import aborted.")
            return False
        for column, data in enumerate(row_data):
            if column < len(widget_headers):
                if not verifyCell(gui, column, row, data):
                    return False

    return widget_headers

def verifyCell(gui, column=None, row=None, data=None, widget=None):
    item = None
    if isinstance(gui, QtWidgets.QTableWidgetItem):
        item = gui
        column = item.column()
        row = item.row()
        data = item.text()

        #Find GUI parent class as root of widget
        parent = gui.tableWidget()
        child = None
        while parent is not None:
            child = parent
            parent = parent.parent()
        gui = child

    try:
        data = float(data)
        if column == 0:
            if data not in [1, 2, 3, 4]:
                showMessage(gui, "Error: \"" + str(data) + "\" at row #" + str(
                    row + 1) + " is not a valid LED integer (1-4). Process aborted.")
                if item:
                    item.setText("")
                return False
        elif column in [1, 2]:
            if data < 0 or data > 100 or data is None:
                showMessage(gui, "Error: \"" + str(data) + "\" at row #" + str(
                    row + 1) + " is not a valid percentage (0-100). Process aborted.")
                if item:
                    item.setText("")
                return False
        elif column == 3:
            if (data < 1e-5 and data != 0) or data > 4294 or data is None:
                showMessage(gui, "Error: \"" + str(data) + "\" at row #" + str(
                    row + 1) + " is not a valid duration: 0 (hold) or 10 µs to 4294 seconds. Process aborted.")
                if item:
                    item.setText("")
                return False
        if item is not None: #Check whole table if last row was edited - this will also add row if table is valid
            dynamicallyCheckTable(gui, item.tableWidget(), item.tableWidget().rowCount(), item.tableWidget().columnCount())


    except:
        if data != "" or not item: #If cell is not empty and table called the error (i.e. not on load) then report that entry as invalid
            showMessage(gui, 
                "Error: \"" + str(data) + "\" at row #" + str(row + 1) + " column #" + str(column + 1) + " is not a number. Process aborted.")
            if item:
                item.setText("")
        return False


    if item is not None:
        print()
    return True

def dynamicallyCheckTable(gui, widget, end_row, end_column):
    widget_header_obj = [widget.horizontalHeaderItem(c) for c in range(widget.columnCount())]
    widget_headers = [x.text() for x in widget_header_obj if x is not None]
    print("222222222222")
    with tempfile.TemporaryFile(mode="w+", suffix=".csv", newline='') as stream:  # "newline=''" removes extra newline from windows - https://stackoverflow.com/questions/3191528/csv-in-python-adding-an-extra-carriage-return-on-windows
        writer = csv.writer(stream)
        writer.writerow(widget_headers)
        valid_row_count = 0
        start_row = end_row-row_subsample
        if start_row < 0:
            start_row = 0
        for row in range(start_row, end_row):
            row_data = [None] * len(widget_headers)
            for column in range(widget.columnCount()):
                item = widget.item(row, column)
                if item is not None:
                    row_data[column] = str(item.text())
                    if row_data[column] == "" or not verifyCell(gui, column, row, row_data[column]):
                        widget.setItem(row, column, None)
                        row_data[column] = None
                        break
                    if row == end_row-1 and column == end_column:
                        setSequencePath(gui, widget, None)  # Flag the sequence table is changed but not yet saved
                        break

            if None not in row_data:
                writer.writerow(row_data)
                valid_row_count += 1

        if valid_row_count > 0:
            if verifySequence(gui, stream, widget):
                if valid_row_count >= end_row-start_row:
                    if widget.rowCount() < maximum_rows:
                        widget.insertRow(widget.rowCount())

def getSequencePath(gui, widget):
    path = widget.objectName().split("_")
    path = [x.capitalize() for x in path[1:4]]
    dictionary = gui.sync_model
    for key in path:
        dictionary = dictionary[key]
    return dictionary

def setSequencePath(gui, widget, value):
    path = widget.objectName().split("_")
    path = [x.capitalize() for x in path[1:4]]
    dictionary = gui.sync_model
    for key in path[:-1]:
        dictionary = dictionary[key]
    dictionary[path[-1]] = value

def sequenceToBytes(gui, widget):
    byte_array = bytearray()
    widget_header_obj = [widget.horizontalHeaderItem(c) for c in range(widget.columnCount())]
    widget_headers = [x.text() for x in widget_header_obj if x is not None]
    with tempfile.TemporaryFile(mode="w+", suffix=".csv", newline='') as stream:  # "newline=''" removes extra newline from windows - https://stackoverflow.com/questions/3191528/csv-in-python-adding-an-extra-carriage-return-on-windows
        writer = csv.writer(stream)
        writer.writerow(widget_headers)
        for row in range(widget.rowCount()):
            row_data = [None] * len(widget_headers)
            for column in range(widget.columnCount()):
                item = widget.item(row, column)
                if item is not None:
                    row_data[column] = str(item.text())

            if row_data != [None] * len(widget_headers):  # Exclude empty rows
                writer.writerow(row_data)

        widget_headers = verifySequence(gui, stream, widget)

        if widget_headers:
            stream.seek(0)
            converted_row=[None]*4
            total_resistance = float(gui.configure_current_limit_box.whatsThis())
            for key, value in gui.seq_dict[widget].items(): #Clear seq dict entry
                gui.seq_dict[widget][key] = []
            for index, row_data in enumerate(csv.reader(stream)):
                if index > 0: #Don't send headers to LED driver
                    converted_row[0] = int(row_data[0])
                    converted_row[1] = round((float(row_data[1])*65535)/100) #Convert percent to ADC value
                    converted_row[3] = round(float(row_data[3])*1e6) #convert seconds to microseconds
                    led_current = (float(row_data[2])/100)*gui.getValue(gui.config_model["LED" + str(converted_row[0])]["Current Limit"])
                    led_voltage = led_current*total_resistance
                    converted_row[2] = round((led_voltage/3.3)*65535)
                    byte_array.extend(struct.pack("<BHHI", *converted_row))

                    # Save data to sequence dictionary
                    for header_index, header in enumerate(widget_headers):
                        gui.seq_dict[widget][header].append(row_data[header_index])

            return byte_array

        else:
            return None

def bytesToSequence(byte_array, gui, widget):
    def sigFigLimit(x, n):
        if x != 0:
            return round(x, -int(math.floor(math.log10(abs(x)))) + (n - 1))
        else:
            return 0

    widget_header_obj = [widget.horizontalHeaderItem(c) for c in range(widget.columnCount())] #Get headers from table
    widget_headers = [x.text() for x in widget_header_obj if x is not None]
    if len(byte_array)%9 == 0:
        row_list = [struct.unpack("<BHHI", byte_array[i:i+9]) for i in range(0, len(byte_array), 9)] #Unpack byte_array to list of row data - https://stackoverflow.com/questions/6614891/turning-a-list-into-nested-lists-in-python
        with tempfile.NamedTemporaryFile(mode="w+", suffix=".csv", newline='', delete=False) as stream:  #NOTE: On windows a tempfile cannot be read from another source while open, so don't use "with" to open: https://stackoverflow.com/questions/23212435/permission-denied-to-write-to-my-temporary-file
            writer = csv.writer(stream)
            writer.writerow(widget_headers) #Add header to temp file
            converted_row = [None] * 4
            total_resistance = float(gui.configure_current_limit_box.whatsThis())
            for key, value in gui.seq_dict[widget].items(): #Clear seq dict entry
                gui.seq_dict[widget][key] = []
            for row_data in row_list:
                converted_row[0] = int(row_data[0])
                converted_row[1] = (float(row_data[1]) / 65535) * 100  # Convert ADC to percent value
                converted_row[3] = float(row_data[3]) / 1e6  # convert microseconds to seconds
                led_voltage = (float(row_data[2]) / 65535) * 3.3
                led_current = led_voltage / total_resistance
                converted_row[2] = (led_current / gui.getValue(gui.config_model["LED" + str(converted_row[0])]["Current Limit"])) * 100
                converted_row[1:] = [sigFigLimit(x, 3) for x in converted_row[1:]]
                writer.writerow(converted_row) #Write rows to temp file

                #Save data to sequence dictionary
                for index, header in enumerate(widget_headers):
                    gui.seq_dict[widget][header].append(converted_row[index])
            setSequencePath(gui, widget, stream.name) #Save temp file path to sync model

        loadSequence(gui, widget, True) #Load temp file to widget
        os.remove(stream.name) #Close and remove tempfile when done
        setSequencePath(gui, widget, None)


    else:
        showMessage(gui, "Error: Downloaded sequence stream length % 9 = " + str(len(byte_array)%9) + ". It should be = 0. Sequence stream not loaded")

def showMessage(gui, text):
    gui.waitCursor(False)
    gui.stopSplash()
    gui.message_box.setText(text)
    gui.message_box.exec()