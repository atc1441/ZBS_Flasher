import os
import time

database_file = None
status_file = None
assoc_displays = []
status_displays = []

def init(filenameList, filenameStatus):
    global database_file
    global status_file
    global assoc_displays
    database_file = filenameList    
    status_file = filenameStatus    
    print("Loading database files: " + database_file + " and " + status_file)
    if os.path.isfile(database_file):
        with open(database_file) as f:
            assoc_displays = f.readlines()
    else:
        print("No database available")
    if os.path.isfile(status_file):
        with open(status_file) as f:
            status_displays = f.readlines()
    else:
        print("No status file available")
        
def addAssoc(mac, data):
    if(database_file==None):
        print("Err no database file available")
        return
    exists = False
    for i, asc in enumerate(assoc_displays):
        if mac in asc[0:16]:
            assoc_displays[i] = mac + data + "\n"
            exists = True
            break
    if exists == False:
        assoc_displays.append(mac + data + "\n")
    file_writing = open(database_file, "w")
    for asc in assoc_displays:
        file_writing.write(asc)
    file_writing.close()
    
def addStatus(mac, data):
    if(status_file==None):
        print("Err no database file available")
        return
    exists = False
    for i, asc in enumerate(status_displays):
        if mac in asc[0:16]:
            status_displays[i] = mac + data + "\n"
            exists = True
            break
    if exists == False:
        status_displays.append(mac + data + "\n")
    file_writing = open(status_file, "w")
    for asc in status_displays:
        file_writing.write(asc)
    file_writing.close()
    
def share(cb_send, websocket):
    if(cb_send == None):
        return
    for asc in assoc_displays:
        cb_send(websocket, asc.strip())
    for asc in status_displays:
        cb_send(websocket, asc.strip())
