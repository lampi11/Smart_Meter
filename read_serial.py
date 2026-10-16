import serial, time
s = serial.Serial('/dev/ttyUSB0', 115200)
s.setDTR(False)
s.setRTS(True)
time.sleep(0.1)
s.setDTR(True)
s.setRTS(False)
time.sleep(0.1)
s.setDTR(False)
for _ in range(10):
    print(s.readline().decode('utf-8', 'ignore').strip())
