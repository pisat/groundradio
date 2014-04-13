import time
import sys
import glob
import serial
port = glob.glob("/dev/tty.usbserial*")[0]
print("Opening port {}".format(port))
ser = serial.Serial(port, 300)

if len(sys.argv) != 2:
    print "Usage: tx.py <filename>"
    sys.exit()

with open(sys.argv[1]) as f:
    data = f.read()

print("Transmitting...")
#ser.write("UUUU");
ser.write(data)
#time.sleep(30)
time.sleep(3)
ser.close()
