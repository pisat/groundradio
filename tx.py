import glob
import serial
port = glob.glob("/dev/tty.usbserial*")[0]
print("Opening port {}".format(port))
ser = serial.Serial(port, 300)

print("Transmitting...")
while True:
    ser.write("UUUU Hello World ")

ser.close()
