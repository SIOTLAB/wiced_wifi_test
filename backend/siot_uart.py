#http://pyserial.readthedocs.io/en/latest/shortintro.html
import serial
from threading import Thread, Lock

class Siot_Serial:
    #TODO: Put mutex lock
    mutex = None
    #TODO: Keep track of num exp from login
    numexp = None
    count_numexp = None
    logged_in = None

    def __init__(self):
        self.ser = None
        self.running_experiments = False
        self.logged_in = False
        self.numexp = -1 #Ensures that login must occur before sending packets
        self.count_numexp = 0
        self.mutex = Lock() #Starts as released

    #TODO: Use mutex lock to check, return true if in use, vice versa
    def is_running_exp(self):
        return self.mutex.locked()

    #Login set and get
    def set_login(self, val):
        self.logged_in = val
        return
    def get_login(self):
        return self.logged_in

    #Functions for modifying exp count to be checked when experiments are about to start
    def increment_exp_count(self):
        self.count_numexp+=1
        return
    def clear_exp_count(self):
        self.count_numexp = 0
        return
    def get_exp_count(self):
        return self.count_numexp
    def check_exp_count(self):
        return self.count_numexp == self.numexp
    def get_exp_total(self):
        return self.numexp

    def open_serial(self, serial_location, baud):
        #should be as argument
        #Timeout set to 1 day
        self.ser = serial.Serial(serial_location,int(baud),timeout=86400)
        return 0

    def pad_str_encode(self, my_string, msg_type):
        pad_size = 4096
        my_string += str(msg_type)
        for _ in range(len(my_string), pad_size):
            my_string += '*'

        ret = my_string.encode('utf-8')
        # print("Final length", len(ret))
        return ret


    def uart_start(self, my_input, msg_type):
        extended_str = str(my_input)
        extended_str = extended_str.replace("'", "\"");

        extended_str = self.pad_str_encode(extended_str, msg_type)

        if(msg_type == 0):
            print("Sending login info to board")
            # print("Num exp: ", str(int(my_input["num_exp"])))
            self.numexp = int(my_input["num_exp"])

        elif(msg_type == 1):
            print("Starting experiments")

        elif(msg_type == 2):
            self.ser.write(extended_str)
            print("Restarting board")
            return 0

        else:
            print("There was an error in parsing message type")
            return -1
        #Send message to board

        #TODO: Lock mutex
        self.mutex.acquire()
        self.ser.write(extended_str)

        last_line = ""
        line = "" #read up to \n
        while "STOP" not in str(line):
            last_line = line
            line = self.ser.readline(5000)
            # self.ser.flushInput()
            if(len(line) == 0):
                self.mutex.release()
                return "Timeout, please ensure board is ready, and not currently running experiments."
            print(line)

        print("Received", last_line)
        self.mutex.release()

        return str(last_line)

    def uart_echo(self):
        print(self.ser.name)

        #prompt user for upd/tcp
        print("Sent: hello")
        self.ser.write(b'echo!\r')

        line = self.ser.readline(1000) #read up to \n
        print("Received",line)

        return line

    def close_serial(self):
        self.ser.close()
