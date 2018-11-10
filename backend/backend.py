import os
import sys
import threading
    #usage: t = threading.Thread(target=get_url, args = (q,u))
import siot_uart as su
from flask import Flask,redirect,jsonify,render_template, request
from jinja2 import Environment, FileSystemLoader

app = Flask(__name__)
uart_obj = None

@app.route('/login')
def render_login():
    return render_template('login.html')

@app.route('/index')
def render_index():
    return render_template('index.html')

@app.route('/uart/login', methods=['POST'])
def uart_login():
    #TODO: Check if running, if so render please wait

    temp = request.form
    jsonStr = request.form.to_dict(flat=True)

    print(jsonStr)
    response = uart_obj.uart_start(jsonStr, 0)
    #Everytime we login,
    del configs[:]
    uart_obj.clear_exp_count()
    uart_obj.set_login(True)

    if("Failed to join:" in str(response)):
        res = render_template('index.html', response="Failed to associate to Access point")
    else:
        res = render_template('index.html')
    return res

@app.route('/uart/append_config', methods=['POST'])
def hello_start():
    temp = request.form
    jsonStr = request.form.to_dict(flat=True)

    #TODO: Check that correct format has been entered
    if(uart_obj.get_login() == False):
        res = render_template('index.html',
                           response="Not yet logged in, please go to login.html")
    elif(uart_obj.is_running_exp() == True):
            res = render_template('index.html',
                               response="Configurations not added, board is currently running experiments. Please try again later.")
    elif(uart_obj.check_exp_count() == True):
        res = render_template('index.html',
                           response="Max number of experiments entered")
    else:
        #TODO: Assert that jsonStr has all values filled
        configs.append(jsonStr)
        print(configs)
        uart_obj.increment_exp_count()

        res = render_template('index.html',
                           response="Configuration Added: (" + str(uart_obj.get_exp_count()) + "/" + str(uart_obj.get_exp_total()) + ")")
    return res

@app.route('/uart/restart_board', methods=['POST'])
def restart_board():
    del configs[:]
    uart_obj.clear_exp_count()
    uart_obj.set_login(False)

    uart_obj.uart_start("", 2)
    res = render_template('index.html',
                       response="Board has been power cycled. Please return to login page to reconfigure experiments")
    return res

@app.route('/uart/start', methods=['POST'])
def uart_echo():
    #TODO: Assert that #configs == numexp
    if(uart_obj.get_login() == False):
        res = render_template('index.html',response="Not yet logged in, please go to login.html")
    elif(not configs):
        res = render_template('index.html',response="NO CONFIGURATIONS SAVED")
    elif(uart_obj.is_running_exp() == True):
        res = render_template('index.html',
                           response="Configuration not added, board is currently running experiments. Please try again later.")
    elif(uart_obj.check_exp_count() == False):
        #TODO: Check syntax on string concat
        res = render_template('index.html',response="Incorrect number of experiments entered. Count: " + str(uart_obj.get_exp_count()) + "/" + str(uart_obj.get_exp_total()))
    else:
        #TODO: Implement waiting for exp to complete
        #if uart_obj.running(), render "please wait"
        #if not running, run start in new thread, and render "please wait"
        #Shouldn't run new thread if mutex is locked, don't need to worry about multiple
        #Could use semaphore to make sure that only one thread is running

        #TODO: Run uart_start() in new thread, and return index.html with "Experiments currently running, refresh page to check progress"
        # response = uart_obj.uart_start(configs, 1)
        t = threading.Thread(target=uart_obj.uart_start, args = (configs,1))
        t.start()

        res = render_template('index.html',response="Experiments currently running, refresh page to check progress")

    return res

@app.route('/uart/board_state', methods=['POST'])
def board_state():
    if(uart_obj.is_running_exp() == True):
        res = render_template('index.html',
                           response="Experiments in progress")
    else:
        res = render_template('index.html',
                           response="No experiments currently in progress")
    return res

@app.route('/uart/clear_exp', methods=['POST'])
def clear_exp():
    if(uart_obj.is_running_exp() == True):
        res = render_template('index.html',
                           response="Experiments in progress")
    else:
        del configs[:]
        uart_obj.clear_exp_count()

        #TODO: Check if running, if so render please wait
        res = render_template('index.html')

    return res

if __name__ == "__main__":
    if len(sys.argv) > 1:
        uart_obj = su.Siot_Serial()
        #Pass arguments here for location and baud
        if (uart_obj.open_serial(sys.argv[1], sys.argv[2]) == 0):
            print("opened serial!")
        else:
            print("Failed to open serial!")
            sys.exit()

        configs = []
        app.run(debug=False)
    else:
        print("Usage:\n\tbackend.py <serial_location> <baud_rate>")
