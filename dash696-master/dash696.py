import os
import ttk
import Tkinter as tk
import ScrolledText
import multiprocessing
import struct
import time
import urllib
import socket
import StringIO
import pickle
from multiprocessing.reduction import ForkingPickler


import PIL.Image
import PIL.ImageTk
import cv2
import numpy as np

class Tcp_Tag:
    # These constants must match those with the same names in tcp_comms.h
    RASPICAM_QUIT =                   0
    RASPICAM_SATURATION =             1
    RASPICAM_SHARPNESS =              2
    RASPICAM_CONTRAST =               3
    RASPICAM_BRIGHTNESS =             4
    RASPICAM_ISO =                    5
    RASPICAM_METERING_MODE =          6
    RASPICAM_VIDEO_STABILISATION =    7
    RASPICAM_EXPOSURE_COMPENSATION =  8
    RASPICAM_EXPOSURE_MODE =          9
    RASPICAM_AWB_MODE =              10
    RASPICAM_AWB_GAINS =             11
    RASPICAM_IMAGE_FX =              12
    RASPICAM_COLOUR_FX =             13
    RASPICAM_ROTATION =              14
    RASPICAM_FLIPS =                 15
    RASPICAM_ROI =                   16
    RASPICAM_SHUTTER_SPEED =         17
    RASPICAM_DRC =                   18
    RASPICAM_STATS_PASS =            19
    RASPICAM_TEST_IMAGE_ENABLE =     20
    RASPICAM_YUV_WRITE_ENABLE =      21
    RASPICAM_JPG_WRITE_ENABLE =      22
    RASPICAM_DETECT_YUV_ENABLE =     23
    RASPICAM_BLOB_YUV =              24
    RASPICAM_FREEZE_EXPOSURE =       25
    RASPICAM_CROSSHAIRS =            26

class Tcp_Params():
    """Camera Parameters"""
    def __init__(self):
        # RASPICAM_CAMERA_PARAMS

        self.contrast = 0
        self.brightness = 0
        self.saturation = 0
        self.ISO = 0
        self.videoStabilisation = 0
        self.exposureCompensation = 0
        self.exposureMode = 0
        self.exposureMeterMode = 0
        self.awbMode = 0
        self.rotation = 0
        self.hflip = 0
        self.vflip = 0
        self.shutter_speed = 0
        self.awb_gains_r = 0.0
        self.awb_gains_b = 0.0
        self.drc_level = 0

        # Tcp_Params

        self.test_img_enable = False
        self.yuv_write = False
        self.jpg_write = False
        self.detect_yuv = False
        self.blob_y_min = 0
        self.blob_u_min = 0
        self.blob_v_min = 0
        self.blob_y_max = 0
        self.blob_u_max = 0
        self.blob_v_max = 0
        self.analog_gain_target = 0.0
        self.analog_gain_tol = 0.0
        self.digital_gain_target = 0.0
        self.digital_gain_tol = 0.0
        self.crosshairs_x = 0
        self.crosshairs_y = 0

        dir = os.path.dirname(os.path.abspath(__file__))
        self.save_fn = os.path.join(dir, "dash696_config.bin")


    def save(self):
        data = struct.pack('!10i56x3i4x4diffi296x4?6B2x4f2i28x', self.sharpness, self.contrast, self.brightness,
                self.saturation, self.ISO, self.videoStabilisation,
                self.exposureCompensation, self.exposureMode, self.exposureMeterMode, self.awbMode,
                self.rotation, self.hflip, self.vflip,
                self.roi_x, self.roi_y, self.roi_w, self.roi_h,
                self.shutter_speed, self.awb_gains_r, self.awb_gains_b, self.drc_level,
                self.test_img_enable, self.yuv_write, self.jpg_write, self.detect_yuv,
                self.blob_y_min, self.blob_u_min, self.blob_v_min, self.blob_y_max, self.blob_u_max, self.blob_v_max,
                self.analog_gain_target, self.analog_gain_tol, self.digital_gain_target, self.digital_gain_tol,
                self.crosshairs_x, self.crosshairs_y)
        file = open(self.save_fn, "wb")
        file.write(data)
        file.close()

    def restore(self):
        try:
            file = open(self.save_fn, "rb")
            data = file.read()
            file.close()
            self.sharpness, self.contrast, self.brightness, self.saturation, self.ISO, self.videoStabilisation, \
            self.exposureCompensation, self.exposureMode, self.exposureMeterMode, self.awbMode, \
            self.rotation, self.hflip, self.vflip, \
            self.roi_x, self.roi_y, self.roi_w, self.roi_h, \
            self.shutter_speed, self.awb_gains_r, self.awb_gains_b, self.drc_level, \
            self.test_img_enable, self.yuv_write, self.jpg_write, self.detect_yuv, \
            self.blob_y_min, self.blob_u_min, self.blob_v_min, self.blob_y_max, self.blob_u_max, self.blob_v_max, \
            self.analog_gain_target, self.analog_gain_tol, self.digital_gain_target, self.digital_gain_tol, \
            self.crosshairs_x, self.crosshairs_y = struct.unpack('!10i56x3i4x4diffi296x4?6B2x4f2i28x', data)
            return True
        except:
            return False



def connect_tcp_comms(ip_addr, port, conn):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    is_connected = False
    while not is_connected:
        print("connect_tcp_comms")
        try:
            sock.connect((ip_addr, port))
            is_connected = True
        except Exception, e:
            print("can't connect to " + ip_addr + "/" + str(port) + ": " + str(e))
            conn.send(("can't connect to " + ip_addr + "/" + str(port) + ": " + str(e) + "\n", None))
    print("connect done")
    print(str(sock))
    buf = StringIO.StringIO()
    ForkingPickler(buf).dump(sock)
    conn.send((None, buf.getvalue()))


class Tcp_Comms():
    """TCP communications with the input_raspicam_696 process on the Raspberry Pi"""
    def __init__(self, ip_addr, port, text_display, do_quit):
        """Tcp_Comms Constructor
        Parameters:
            ip_addr = IP address of the host running input_raspicam_696
            port = port number used by input_raspicam_696
        """
        is_connected = False
        repeat_count = 0
        while not is_connected and not do_quit.value:
            try:
                self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                orig_timeout = self.sock.gettimeout()
                self.sock.settimeout(0.5)
                self.sock.connect((ip_addr, port))
                is_connected = True
            except socket.timeout as e:
                if repeat_count % 10 == 0:
                    text_display.insert(tk.END, "can't connect to " + ip_addr + "/" + str(port) + ": timeout\n")
            except Exception, e:
                text_display.insert(tk.END, "can't connect to " + ip_addr + "/" + str(port) + ": " + str(e) + "\n")
            text_display.update_idletasks()
            text_display.update()
            repeat_count = repeat_count + 1
        if do_quit.value:
            exit(255)
        self.sock.settimeout(orig_timeout)
        self.text_display = text_display
        text_display.insert(tk.END, "connected to " + ip_addr + "/" + str(port) + "\n")
        text_display.update()
        self.tcp_params = Tcp_Params();


    def recv_tcp_params(self):
        try:
            data = self.sock.recv(520)
        except Exception as e:
            self.text_display.insert(tk.END, "can't recv_tcp_params: %s\n" % str(e))
        else:
            self.tcp_params.sharpness, self.tcp_params.contrast, self.tcp_params.brightness, \
                self.tcp_params.saturation, self.tcp_params.ISO, self.tcp_params.videoStabilisation, \
                self.tcp_params.exposureCompensation, self.tcp_params.exposureMode, \
                self.tcp_params.exposureMeterMode, self.tcp_params.awbMode, self.tcp_params.rotation, \
                self.tcp_params.hflip, self.tcp_params.vflip, self.tcp_params.roi_x, self.tcp_params.roi_y, \
                self.tcp_params.roi_w, self.tcp_params.roi_h, self.tcp_params.shutter_speed, \
                self.tcp_params.awb_gains_r, self.tcp_params.awb_gains_b, self.tcp_params.drc_level, \
                self.tcp_params.test_img_enable, self.tcp_params.yuv_write, self.tcp_params.jpg_write, \
                self.tcp_params.detect_yuv, self.tcp_params.blob_y_min, self.tcp_params.blob_u_min, \
                self.tcp_params.blob_v_min, self.tcp_params.blob_y_max, self.tcp_params.blob_u_max, \
                self.tcp_params.blob_v_max, self.tcp_params.analog_gain_target, self.tcp_params.analog_gain_tol, \
                self.tcp_params.digital_gain_target, self.tcp_params.digital_gain_tol, self.tcp_params.crosshairs_x, \
                self.tcp_params.crosshairs_y = struct.unpack('!10i56x3i4x4diffi296x4?6B2x4f2i28x', data)

    def send_tcp_params(self):
        data = struct.pack('!10i56x3i4x4diffi296x4?6B2x4f2i28x', self.tcp_params.sharpness, self.tcp_params.contrast,
                           self.tcp_params.brightness, self.tcp_params.saturation, self.tcp_params.ISO,
                           self.tcp_params.videoStabilisation, self.tcp_params.exposureCompensation,
                           self.tcp_params.exposureMode, self.tcp_params.exposureMeterMode, self.tcp_params.awbMode,
                           self.tcp_params.rotation, self.tcp_params.hflip, self.tcp_params.vflip,
                           self.tcp_params.roi_x, self.tcp_params.roi_y, self.tcp_params.roi_w, self.tcp_params.roi_h,
                           self.tcp_params.shutter_speed, self.tcp_params.awb_gains_r, self.tcp_params.awb_gains_b,
                           self.tcp_params.drc_level, self.tcp_params.test_img_enable, self.tcp_params.yuv_write,
                           self.tcp_params.jpg_write, self.tcp_params.detect_yuv, self.tcp_params.blob_y_min,
                           self.tcp_params.blob_u_min, self.tcp_params.blob_v_min, self.tcp_params.blob_y_max,
                           self.tcp_params.blob_u_max, self.tcp_params.blob_v_max, self.tcp_params.analog_gain_target,
                           self.tcp_params.analog_gain_tol, self.tcp_params.digital_gain_target,
                           self.tcp_params.digital_gain_tol, self.tcp_params.crosshairs_x, self.tcp_params.crosshairs_y)
        self.sock.send(data)

    def send_tcp_params_null(self):
        data = ""
        self.sock.send(data)

    def recv_text_msg(self):

        timeout = False
        data = ""
        try:
            orig_timeout = self.sock.gettimeout()
            self.sock.settimeout(0.0)
            data = self.sock.recv(81)
        except Exception as e:
            timeout = True
        self.sock.settimeout(orig_timeout)
        if timeout or len(data) != 81:
            return
        msg_tuple = struct.unpack('!81s', data)
        msg = msg_tuple[0]
        color_byte = msg[0:1]
        text_msg = msg[1:]

        if color_byte == 'r':
            color = 'red'
        elif color_byte == 'g':
            color = 'green'
        elif color_byte == 'b':
            color = 'blue'
        elif color_byte == 'm':
            color = 'magenta'
        elif color_byte == 'y':
            color = 'yellow'
        elif color_byte == 'c':
            color = 'cyan'
        else:
            color = 'orange'

        self.text_display.tag_config(color, foreground=color)
        self.text_display.insert(tk.END, text_msg, (color))
        #self.text_display.insert(tk.END, text_msg)

    def _send_int_msg(self, tag, count, int0=0, int1=0, int2=0):
        """Send a message comprised of integer fields to the host
        Parameters:
            tag = identifies the type of this message
            count = the number of integers to follow
            intX = an integer argument
        """
        if count == 0:
            data = struct.pack('!BBBB', tag, 0, 0, 0)
        elif count == 1:
            data = struct.pack('!BBBBI', tag, 0, 0, 0, int0)
        elif count == 2:
            data = struct.pack('!BBBBII', tag, 0, 0, 0, int0, int1)
        elif count == 3:
            data = struct.pack('!BBBBIII', tag, 0, 0, 0, int0, int1, int2)
        else:
            print("_send_int_msg saw bad count " + str(count))
            return
        bytes = self.sock.send(data)
        if bytes != len(data):
            print("_send_int_msg: short sock.send byte count (" + str(bytes) + "); expected " + str(len(data)) + "; ignoring")

    def _send_float_msg(self, tag, count, float0 = 0, float1 = 0, float2 = 0, float3 = 0):
        """Send a message comprised of float fields to the host
        Parameters:
            tag = identifies the type of this message
            count = the number of floats to follow
            floatX = a float argument
        """
        if count == 0:
            data = struct.pack('!BBBB', tag, 0, 0, 0)
        elif count == 1:
            data = struct.pack('!BBBBf', tag, 0, 0, 0, float0)
        elif count == 2:
            data = struct.pack('!BBBBff', tag, 0, 0, 0, float0, float1)
            #print(':'.join(x.encode('hex') for x in data))
        elif count == 3:
            data = struct.pack('!BBBBfff', tag, 0, 0, 0, float0, float1, float2)
        elif count == 4:
            data = struct.pack('!BBBBffff', tag, 0, 0, 0, float0, float1, float2, float3)
        else:
            print("_send_int_msg saw bad count " + str(count))
            return
        bytes = self.sock.send(data)
        if bytes != len(data):
            print("_send_float_msg: short sock.send byte count (" + str(bytes) + "); expected " + str(len(data)) + "; ignoring")


    def send_saturation(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_SATURATION, 1, value)

    def send_sharpness(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_SHARPNESS, 1, value)

    def send_contrast(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_CONTRAST, 1, value)

    def send_brightness(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_BRIGHTNESS, 1, value)

    def send_iso(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_ISO, 1, value)

    def send_metering_mode(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_METERING_MODE, 1, value)

    def send_video_stabilisation(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_VIDEO_STABILISATION, 1, value)

    def send_exposure_compensation(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_EXPOSURE_COMPENSATION, 1, value)

    def send_exposure_mode(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_EXPOSURE_MODE, 1, value)

    def send_awb_mode(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_AWB_MODE, 1, value)

    def send_freeze_exposure(self, analog_target, analog_tol, digital_target, digital_tol):
        self._send_float_msg(Tcp_Tag.RASPICAM_FREEZE_EXPOSURE, 4, analog_target, analog_tol, digital_target, digital_tol)

    def send_awb_gains(self, red_gain, blue_gain):
        self._send_float_msg(Tcp_Tag.RASPICAM_AWB_GAINS, 2, red_gain, blue_gain)

    def send_image_fx(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_IMAGE_FX, 1, value)

    def send_colour_fx(self, red, green, blue):
        self._send_int_msg(Tcp_Tag.RASPICAM_COLOUR_FX, 3, red, green, blue)

    def send_rotation(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_ROTATION, 1, value)

    def send_flips(self, hflip, vflip):
        self._send_int_msg(Tcp_Tag.RASPICAM_FLIPS, 2, hflip, vflip)

    def send_roi(self, x, y, w, h):
        self._send_float_msg(Tcp_Tag.RASPICAM_ROI, 4, x, y, w, h)

    def send_shutter_speed(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_SHUTTER_SPEED, 1, value)

    def send_drc(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_DRC, 1, value)

    def send_stats_pass(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_STATS_PASS, 1, value)

    def send_test_image_enable(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_TEST_IMAGE_ENABLE, 1, value)

    def send_yuv_write_enable(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_YUV_WRITE_ENABLE, 1, value)

    def send_jpg_write_enable(self, value):
        self._send_int_msg(Tcp_Tag.RASPICAM_JPG_WRITE_ENABLE, 1, value)

    def send_blob_yuv(self, y_min, y_max, u_min, u_max, v_min, v_max):
        data = struct.pack('!BBBBBBB', Tcp_Tag.RASPICAM_BLOB_YUV, y_min, y_max, u_min, u_max, v_min, v_max)
        self.sock.send(data)

    def send_crosshairs(self, x, y):
        self._send_int_msg(Tcp_Tag.RASPICAM_CROSSHAIRS, 2, x, y)







#tkinter GUI functions----------------------------------------------------------

awb_mode_names = ("off", "auto", "sun", "cloud", "shade", "tungsten", "fluorescent",
                           "incandescent", "flash", "horizon")
class Awb_Widget():
    """This widget allows the user to set the auto white balance to one of a small set of legal values."""
    def __init__(self, parent, tcp_comms):
        global awb_mode_names
        self.value = tk.StringVar()
        self.value.set(awb_mode_names[1])
        self.widget = tk.OptionMenu(parent, self.value, *awb_mode_names, command=self.command)
        self.tcp_comms = tcp_comms

    def command(self, value):
        """When the value is selected this function is called to send a TCP message to the host to set the specified parameter values."""
        global awb_mode_names
        for ii in range(0, len(awb_mode_names)):
            if value == awb_mode_names[ii]: break
        self.tcp_comms.send_awb_mode(ii)

    def grid(self, row, column):
        self.widget.grid(row = row, column = column)

exposure_mode_names = ("off", "auto", "night", "nightpreview", "backlight", "spotlight", "sports",
                           "snow", "beach", "verylong", "fixedfps", "antishake", "fireworks")

class Exposure_Mode_Widget():
    """This widget allows the user to set the exposure mode to one of a small set of legal values."""
    def __init__(self, parent, tcp_comms):
        global exposure_mode_names
        self.value = tk.StringVar()
        self.value.set(exposure_mode_names[tcp_comms.tcp_params.exposureMode])
        self.widget = tk.OptionMenu(parent, self.value, *exposure_mode_names, command=self.command)
        self.tcp_comms = tcp_comms

    def command(self, value):
        """When the value is selected this function is called to send a TCP message to the host to set the specified parameter values."""
        for ii in range(0, len(exposure_mode_names)):
            if value == exposure_mode_names[ii]: break
        self.tcp_comms.tcp_params.exposureMode = ii
        self.tcp_comms.send_exposure_mode(self.tcp_comms.tcp_params.exposureMode)

    def grid(self, row, column):
        self.widget.grid(row = row, column = column)

class Iso_Widget():
    """This widget allows the user to set the ISO to one of a small set of legal values."""
    def __init__(self, parent, tcp_comms):
        self.value_list = ("100", "200", "300", "400", "500", "640", "800")
        self.value = tk.StringVar()
        for ii in range(0, len(self.value_list) - 1):
            if tcp_comms.tcp_params.ISO <= self.value_list[ii]: break
        self.value.set(self.value_list[ii])
        self.widget = tk.OptionMenu(parent, self.value, *self.value_list, command=self.command)
        self.tcp_comms = tcp_comms

    def command(self, value):
        """When the value is selected this function is called to send a TCP message to the host to set the specified parameter values."""
        self.tcp_comms.tcp_params.ISO = int(value)
        self.tcp_comms.send_iso(self.tcp_comms.tcp_params.ISO)

    def grid(self, row, column):
        self.widget.grid(row = row, column = column)

exposure_meter_mode_names = ("average", "spot", "backlit", "matrix")

class Metering_Mode_Widget():
    """This widget allows the user to set the metering mode to one of a small set of legal values."""
    def __init__(self, parent, tcp_comms):
        global exposure_meter_mode_names
        self.value = tk.StringVar()
        self.value.set(exposure_meter_mode_names[tcp_comms.tcp_params.exposureMeterMode])
        self.widget = tk.OptionMenu(parent, self.value, *exposure_meter_mode_names, command=self.command)
        self.tcp_comms = tcp_comms

    def command(self, value):
        """When the value is selected this function is called to send a TCP message to the host to set the specified parameter values."""
        global exposure_meter_mode_names
        for ii in range(0, len(exposure_meter_mode_names)):
            if value == exposure_meter_mode_names[ii]: break
        self.tcp_comms.tcp_params.exposureMeterMode = ii
        self.tcp_comms.send_metering_mode(self.tcp_comms.tcp_params.exposureMeterMode)

    def grid(self, row, column):
        self.widget.grid(row = row, column = column)

class Freeze_Exposure_Widget():
    """This widget allows the user to set values for Freeze Analog Gain, Freeze Digital Gain, Analog Tolerance and Digital Tolerance"""
    def __init__(self, parent, tcp_comms):
        self.value_analog = tk.StringVar()
        self.value_analog.set(str(tcp_comms.tcp_params.analog_gain_target))
        self.widget_analog = tk.Entry(parent, textvariable=self.value_analog)
        self.value_digital = tk.StringVar()
        self.value_digital.set(str(tcp_comms.tcp_params.digital_gain_target))
        self.widget_digital = tk.Entry(parent, textvariable=self.value_digital)
        self.value_analog_tol = tk.StringVar()
        self.value_analog_tol.set(str(tcp_comms.tcp_params.analog_gain_tol))
        self.widget_analog_tol = tk.Entry(parent, textvariable=self.value_analog_tol)
        self.value_digital_tol = tk.StringVar()
        self.value_digital_tol.set(str(tcp_comms.tcp_params.digital_gain_tol))
        self.widget_digital_tol = tk.Entry(parent, textvariable=self.value_digital_tol)
        self.ok_button = tk.Button(parent, text="Send", command=self.command)
        self.tcp_comms = tcp_comms

    def command(self):
        """When the 'Send' button is pushed this function is called to send a TCP message to the host to set the specified parameter values."""
        saw_error = False
        try:
            analog_gain = float(self.value_analog.get())
        except:
            print("analog must be floating point value")
            self.value_analog.set(str(self.tcp_comms.tcp_params.analog_gain_target))
            saw_error = True
        try:
            digital_gain = float(self.value_digital.get())
        except:
            print("digital must be floating point value")
            self.value_digital.set(str(self.tcp_comms.tcp_params.digital_gain_target))
            saw_error = True
        try:
            analog_tol = float(self.value_analog_tol.get())
        except:
            print("analog tol must be floating point value")
            self.value_analog_tol.set(str(self.tcp_comms.tcp_params.analog_gain_tol))
            saw_error = True
        try:
            digital_tol = float(self.value_digital_tol.get())
        except:
            print("digital tol must be floating point value")
            self.value_digital_tol.set(str(self.tcp_comms.tcp_params.digital_gain_tol))
            saw_error = True
        if not saw_error:
            self.tcp_comms.tcp_params.analog_gain_target = analog_gain
            self.tcp_comms.tcp_params.digital_gain_target = digital_gain
            self.tcp_comms.tcp_params.analog_gain_tol = analog_tol
            self.tcp_comms.tcp_params.digital_gain_tol = digital_tol
            self.tcp_comms.send_freeze_exposure(analog_gain, analog_tol, digital_gain, digital_tol)

    def grid(self, row, column):
        self.widget_analog.grid(row = row, column = column)
        self.widget_digital.grid(row = row + 1, column = column)
        self.widget_analog_tol.grid(row = row + 2, column = column)
        self.widget_digital_tol.grid(row=row + 3, column = column)
        self.ok_button.grid(row = row + 4, column = column)

class Awb_Gains_Widget():
    """This widget allows the user to set values for AWB Gain Red and AWB Gain Blue."""
    def __init__(self, parent, tcp_comms):
        self.red_gain = tcp_comms.tcp_params.awb_gains_r
        self.value_red = tk.StringVar()
        self.value_red.set(str(tcp_comms.tcp_params.awb_gains_r))
        self.widget_red = tk.Entry(parent, textvariable=self.value_red)
        self.blue_gain = tcp_comms.tcp_params.awb_gains_b
        self.value_blue = tk.StringVar()
        self.value_blue.set(str(self.blue_gain))
        self.widget_blue = tk.Entry(parent, textvariable=self.value_blue)
        self.ok_button = tk.Button(parent, text="Send", command=self.command)
        self.tcp_comms = tcp_comms

    def command(self):
        """When the 'Send' button is pushed this function is called to send a TCP message to the host to set the specified parameter values."""
        saw_error = False
        red_gain = 0.0
        blue_gain = 0.0
        try:
            red_gain = float(self.value_red.get())
        except:
            print("red must be floating point value")
            self.value_red.set(str(self.tcp_comms.tcp_params.awb_gains_r))
            saw_error = True
        try:
            blue_gain = float(self.value_blue.get())
        except:
            print("blue must be floating point value")
            self.value_blue.set(str(self.tcp_comms.tcp_params.awb_gains_b))
            saw_error = True
        if not saw_error:
            self.tcp_comms.tcp_params.awb_gains_r = red_gain
            self.tcp_comms.tcp_params.awb_gains_b = blue_gain
            self.tcp_comms.send_awb_gains(red_gain, blue_gain)

    def grid(self, row, column):
        self.widget_red.grid(row = row, column = column)
        self.widget_blue.grid(row = row + 1, column = column)
        self.ok_button.grid(row = row + 2, column = column)




class Cam_Param_Frame(ttk.Frame, object):
    """Defines the 'Camera Params' tab; used to set camera parameters."""
    def __init__(self, parent, tcp_comms, text_display):
        super(Cam_Param_Frame, self).__init__(parent)

        self.text_display = text_display
        self.tcp_comms = tcp_comms

        self.iso_label = ttk.Label(self, text="ISO")
        self.iso_param = Iso_Widget(self, tcp_comms)
        self.iso_label.grid(row=0, column=0)
        self.iso_param.grid(row=0, column=1)
        self.exposure_mode_label = ttk.Label(self, text="Exposure Mode")
        self.exposure_mode_param = Exposure_Mode_Widget(self, tcp_comms)
        self.exposure_mode_label.grid(row=1, column=0)
        self.exposure_mode_param.grid(row=1, column=1)
        self.metering_mode_label = ttk.Label(self, text="Metering Mode")
        self.metering_mode_param = Metering_Mode_Widget(self, tcp_comms)
        self.metering_mode_label.grid(row=2, column=0)
        self.metering_mode_param.grid(row=2, column=1)

        self.analog_gain_label = ttk.Label(self, text="Freeze Analog Gain")
        self.digital_gain_label = ttk.Label(self, text="Freeze Digital Gain")
        self.analog_tol_label = ttk.Label(self, text="Analog Tolerance")
        self.digital_tol_label = ttk.Label(self, text="Digital Tolerance")
        self.gains_param = Freeze_Exposure_Widget(self, tcp_comms)
        self.analog_gain_label.grid(row=3, column=0)
        self.digital_gain_label.grid(row=4, column=0)
        self.analog_tol_label.grid(row=5, column=0)
        self.digital_tol_label.grid(row=6, column=0)
        self.gains_param.grid(row=3, column=1)

        self.awb_label = ttk.Label(self, text="AWB")
        self.awb_param = Awb_Widget(self, tcp_comms)
        self.awb_label.grid(row = 8, column = 0)
        self.awb_param.grid(row = 8, column = 1)

        self.awb_gain_red_label = ttk.Label(self, text="AWB Gain Red")
        self.awb_gain_blue_label = ttk.Label(self, text="AWB Gain Blue")
        self.awb_gains_param = Awb_Gains_Widget(self, tcp_comms)
        self.awb_gain_red_label.grid(row=9, column=0)
        self.awb_gain_blue_label.grid(row=10, column=0)
        self.awb_gains_param.grid(row=9, column=1)

        self.command_line_button = tk.Button(self, text="Cmd Line", command=self.command_line)
        self.command_line_button.grid(row=11,column=0)

        self.save_button = tk.Button(self, text="Save", command=self.save)
        self.save_button.grid(row=12, column=0)

    def update(self, queue_entry):
        pass

    def save(self):
        self.tcp_comms.tcp_params.save()

    def command_line(self):
        global exposure_mode_names
        global awb_mode_names
        global exposure_meter_mode_names
        self.text_display.insert(tk.END, 'mjpg_streamer -o "output_http.so -w ./www" -i "input_raspicam_696.so \ \n')
        self.text_display.insert(tk.END, '-x 640 -y 480 -vwidth 320 -vheight 240 -fps 30 -quality 20 \ \n')
        self.text_display.insert(tk.END, '-ISO ' + str(self.tcp_comms.tcp_params.ISO) + \
                                 ' -ex ' + exposure_mode_names[self.tcp_comms.tcp_params.exposureMode] + \
                                 ' -awb ' + awb_mode_names[self.tcp_comms.tcp_params.awbMode] + \
                                 ' -awbgainR ' + '%.3f' % self.tcp_comms.tcp_params.awb_gains_r + \
                                 ' -awbgainB ' + '%.3f' % self.tcp_comms.tcp_params.awb_gains_b + ' \ \n')
        self.text_display.insert(tk.END, '-mm ' + exposure_meter_mode_names[self.tcp_comms.tcp_params.exposureMeterMode])
        self.text_display.insert(tk.END, ' -blobyuv %u,%u,%u,%u,%u,%u \ \n' %
                                 (self.tcp_comms.tcp_params.blob_y_min,
                                  self.tcp_comms.tcp_params.blob_y_max,
                                  self.tcp_comms.tcp_params.blob_u_min,
                                  self.tcp_comms.tcp_params.blob_u_max,
                                  self.tcp_comms.tcp_params.blob_v_min,
                                  self.tcp_comms.tcp_params.blob_v_max))

        self.text_display.insert(tk.END, '-againtarget %.3f -againtol %.3f -dgaintarget %.3f -dgaintol %3f\n' % \
                                 (self.tcp_comms.tcp_params.analog_gain_target,
                                  self.tcp_comms.tcp_params.analog_gain_tol,
                                  self.tcp_comms.tcp_params.digital_gain_target,
                                  self.tcp_comms.tcp_params.digital_gain_tol))




def set_pix_from_string_value(old_int_value, string_value, name):
    saw_error = False
    new_int_value = old_int_value
    try:
        new_int_value = int(string_value.get())
    except:
        print(name + " must be floating point value")
        string_value.set(str(old_int_value))
        saw_error = True
    if (new_int_value < 0): new_int_value = 0
    if (new_int_value > 255): new_int_value = 255
    return (saw_error, new_int_value)


class Blob_Yuv_Widget():
    """This widget allows the user to set values for the 'Blob YUV' parameters that define the range of target colors."""
    def __init__(self, parent, tcp_comms):
        self.value_min_y = tk.StringVar()
        self.value_min_y.set(str(tcp_comms.tcp_params.blob_y_min))
        self.widget_min_y = tk.Entry(parent, textvariable=self.value_min_y, width=3, justify='right')

        self.value_min_u = tk.StringVar()
        self.value_min_u.set(str(tcp_comms.tcp_params.blob_u_min))
        self.widget_min_u = tk.Entry(parent, textvariable=self.value_min_u, width=3, justify='right')

        self.value_min_v = tk.StringVar()
        self.value_min_v.set(str(tcp_comms.tcp_params.blob_v_min))
        self.widget_min_v = tk.Entry(parent, textvariable=self.value_min_v, width=3, justify='right')

        self.value_max_y = tk.StringVar()
        self.value_max_y.set(str(tcp_comms.tcp_params.blob_y_max))
        self.widget_max_y = tk.Entry(parent, textvariable=self.value_max_y, width=3, justify='right')

        self.value_max_u = tk.StringVar()
        self.value_max_u.set(str(tcp_comms.tcp_params.blob_u_max))
        self.widget_max_u = tk.Entry(parent, textvariable=self.value_max_u, width=3, justify='right')

        self.value_max_v = tk.StringVar()
        self.value_max_v.set(str(tcp_comms.tcp_params.blob_v_max))
        self.widget_max_v = tk.Entry(parent, textvariable=self.value_max_v, width=3, justify='right')

        self.send_button = tk.Button(parent, text="Send", command=self.command)
        self.tcp_comms = tcp_comms


    def command(self):
        saw_error_1, self.tcp_comms.tcp_params.blob_y_min =\
            set_pix_from_string_value(self.tcp_comms.tcp_params.blob_y_min, self.value_min_y, "Min Y")
        saw_error_2, self.tcp_comms.tcp_params.blob_u_min =\
            set_pix_from_string_value(self.tcp_comms.tcp_params.blob_u_min, self.value_min_u, "Min U")
        saw_error_3, self.tcp_comms.tcp_params.blob_v_min =\
            set_pix_from_string_value(self.tcp_comms.tcp_params.blob_v_min, self.value_min_v, "Min V")
        saw_error_4, self.tcp_comms.tcp_params.blob_y_max =\
            set_pix_from_string_value(self.tcp_comms.tcp_params.blob_y_max, self.value_max_y, "Max Y")
        saw_error_5, self.tcp_comms.tcp_params.blob_u_max =\
            set_pix_from_string_value(self.tcp_comms.tcp_params.blob_u_max, self.value_max_u, "Max U")
        saw_error_6, self.tcp_comms.tcp_params.blob_v_max =\
            set_pix_from_string_value(self.tcp_comms.tcp_params.blob_v_max, self.value_max_v, "Max V")


        if not saw_error_1 and not saw_error_2 and not saw_error_3 and \
           not saw_error_4 and not saw_error_5 and not saw_error_6:
            self.tcp_comms.send_blob_yuv(self.tcp_comms.tcp_params.blob_y_min, self.tcp_comms.tcp_params.blob_y_max,
                                         self.tcp_comms.tcp_params.blob_u_min, self.tcp_comms.tcp_params.blob_u_max,
                                         self.tcp_comms.tcp_params.blob_v_min, self.tcp_comms.tcp_params.blob_v_max)

    def grid(self, row, column):
        self.widget_min_y.grid(row = row, column = column)
        self.widget_min_u.grid(row = row, column = column + 1)
        self.widget_min_v.grid(row = row, column = column + 2)
        self.widget_max_y.grid(row = row + 1, column = column)
        self.widget_max_u.grid(row = row + 1, column = column + 1)
        self.widget_max_v.grid(row = row + 1, column = column + 2)
        self.send_button.grid(row = row + 2, column = column + 1)




class Yuv_Frame(ttk.Frame, object):
    """Defines the 'YUV Color' tab; used to view current YUV color value at the crosshairs, and to set the Blob YUV parameter values."""
    def __init__(self, parent, tcp_comms):
        super(Yuv_Frame, self).__init__(parent)
        self.y_label = ttk.Label(self, text="Y")
        self.u_label = ttk.Label(self, text="U")
        self.v_label = ttk.Label(self, text="V")
        self.meas_label = ttk.Label(self, text="Crosshairs")
        self.meas_y_text = tk.StringVar()
        self.meas_u_text = tk.StringVar()
        self.meas_v_text = tk.StringVar()
        self.meas_y_text.set("128")
        self.meas_u_text.set("128")
        self.meas_v_text.set("128")
        self.meas_y_label = ttk.Label(self, textvariable=self.meas_y_text)
        self.meas_u_label = ttk.Label(self, textvariable=self.meas_u_text)
        self.meas_v_label = ttk.Label(self, textvariable=self.meas_v_text)

        self.y_label.grid(row=0, column=1)
        self.u_label.grid(row=0, column=2)
        self.v_label.grid(row=0, column=3)
        self.meas_label.grid(row=1, column=0)
        self.meas_y_label.grid(row=1, column=1)
        self.meas_u_label.grid(row=1, column=2)
        self.meas_v_label.grid(row=1, column=3)

        self.blob_yuv_label = ttk.Label(self, text="Blob YUV")
        self.min_label = ttk.Label(self, text="Min")
        self.max_label = ttk.Label(self, text="Max")
        self.blob_yuv = Blob_Yuv_Widget(self,tcp_comms)
        self.blob_yuv_label.grid(row=2, column=0)
        self.min_label.grid(row=3, column=0)
        self.max_label.grid(row=4, column=0)
        self.blob_yuv.grid(row=3, column=1)

    def update(self, queue_entry):
        self.meas_y_text.set("%3d" % (queue_entry.y))
        self.meas_u_text.set("%3d" % (queue_entry.u))
        self.meas_v_text.set("%3d" % (queue_entry.v))

class Gain_Frame(ttk.Frame, object):
    """Defines the 'Gains' tab, used to view current camera gain settings."""
    def __init__(self, parent):
        super(Gain_Frame, self).__init__(parent)

        self.exposure_text = tk.StringVar()
        self.analog_text = tk.StringVar()
        self.analog_label_text = tk.StringVar()
        self.digital_text = tk.StringVar()
        self.digital_label_text = tk.StringVar()
        self.awb_red_text = tk.StringVar()
        self.awb_blue_text = tk.StringVar()
        self.y_text = tk.StringVar()
        self.u_text = tk.StringVar()
        self.v_text = tk.StringVar()
        self.k_bits_per_second_text = tk.StringVar()
        self.frames_per_second_text = tk.StringVar()
        self.smooth_k_bits_per_second = 0.0
        self.smooth_frames_per_second = 0.0

        self.exposure_text.set("1.0")
        self.analog_text.set("1.0")
        self.digital_text.set("1.0")
        self.awb_red_text.set("1.0")
        self.awb_blue_text.set("1.0")
        self.y_text.set("128")
        self.u_text.set("128")
        self.v_text.set("128")
        self.k_bits_per_second_text.set("0.0")
        self.frames_per_second_text.set("0.0")

        self.exposure_label_1 = ttk.Label(self, text="exposure")
        self.exposure_label_2 = ttk.Label(self, textvariable = self.exposure_text)
        self.exposure_label_1.grid(row = 0, column = 0)
        self.exposure_label_2.grid(row = 0, column = 1)

        self.analog_label_1 = ttk.Label(self, textvariable = self.analog_label_text)
        self.analog_label_2 = ttk.Label(self, textvariable = self.analog_text)
        self.analog_label_1.grid(row=1, column=0)
        self.analog_label_2.grid(row=1, column=1)

        self.digital_label_1 = ttk.Label(self, textvariable = self.digital_label_text)
        self.digital_label_2 = ttk.Label(self, textvariable = self.digital_text)
        self.digital_label_1.grid(row=2, column=0)
        self.digital_label_2.grid(row=2, column=1)

        self.awb_red_label_1 = ttk.Label(self, text="AWB red")
        self.awb_red_label_2 = ttk.Label(self, textvariable = self.awb_red_text)
        self.awb_red_label_1.grid(row=3, column=0)
        self.awb_red_label_2.grid(row=3, column=1)

        self.awb_blue_label_1 = ttk.Label(self, text="AWB blue")
        self.awb_blue_label_2 = ttk.Label(self, textvariable = self.awb_blue_text)
        self.awb_blue_label_1.grid(row=4, column=0)
        self.awb_blue_label_2.grid(row=4, column=1)

        self.y_label_1 = ttk.Label(self, text="crosshairs Y")
        self.y_label_2 = ttk.Label(self, textvariable=self.y_text)
        self.y_label_1.grid(row=5, column=0)
        self.y_label_2.grid(row=5, column=1)

        self.u_label_1 = ttk.Label(self, text="crosshairs U")
        self.u_label_2 = ttk.Label(self, textvariable=self.u_text)
        self.u_label_1.grid(row=6, column=0)
        self.u_label_2.grid(row=6, column=1)

        self.v_label_1 = ttk.Label(self, text="crosshairs V")
        self.v_label_2 = ttk.Label(self, textvariable=self.v_text)
        self.v_label_1.grid(row=7, column=0)
        self.v_label_2.grid(row=7, column=1)

        self.k_bits_per_second_label_1 = ttk.Label(self, text="bits per second")
        self.k_bits_per_second_label_2 = ttk.Label(self, textvariable = self.k_bits_per_second_text)
        self.k_bits_per_second_label_1.grid(row=8, column=0)
        self.k_bits_per_second_label_2.grid(row=8, column=1)

        self.frames_per_second_label_1 = ttk.Label(self, text="frames per second")
        self.frames_per_second_label_2 = ttk.Label(self, textvariable=self.frames_per_second_text)
        self.frames_per_second_label_1.grid(row=9, column=0)
        self.frames_per_second_label_2.grid(row=9, column=1)

    def update(self, queue_entry):
        # Parse queue_entry.flags for gain_is_frozen bit
        bit0 = 0x1 & queue_entry.flags
        if (bit0):
            self.analog_label_text.set("analog (frozen)")
            self.digital_label_text.set("digital (frozen)")
        else:
            self.analog_label_text.set("analog         ")
            self.digital_label_text.set("digital         ")

        self.exposure_text.set("%10.3f ms" % (queue_entry.exposure_secs))
        self.analog_text.set("%10.3f" %(queue_entry.analog_gain))
        self.digital_text.set("%10.3f" % (queue_entry.digital_gain))
        self.awb_red_text.set("%10.3f" % (queue_entry.awb_red_gain))
        self.awb_blue_text.set("%10.3f" % (queue_entry.awb_blue_gain))
        self.y_text.set("%3d" % (queue_entry.y))
        self.u_text.set("%3d" % (queue_entry.u))
        self.v_text.set("%3d" % (queue_entry.v))
        ALPHA = 0.99
        if queue_entry.bits_per_second > 0.0:
            self.smooth_k_bits_per_second = ALPHA * self.smooth_k_bits_per_second + (1.0 - ALPHA) * (queue_entry.bits_per_second / 1000.0)
            self.k_bits_per_second_text.set("%12.3f K" % (self.smooth_k_bits_per_second))
        if queue_entry.frames_per_second > 0.0:
            self.smooth_frames_per_second = ALPHA * self.smooth_frames_per_second + (1.0 - ALPHA) * queue_entry.frames_per_second
            self.frames_per_second_text.set("%10.3f" % (self.smooth_frames_per_second))


class Dash_696(ttk.Frame, object):
    """Defines the entire Dash696 GUI."""
    def __init__(self, ip_addr, tcp_port, queue, crosshairs_position, do_quit):
        super(Dash_696, self).__init__()
        self.BORDER_WIDTH = 6
        self.style = ttk.Style()
        self.style.theme_use("default")
        self.text_display = ScrolledText.ScrolledText(self.master, width=80, height=2)
        self.text_display.pack(side=tk.BOTTOM, expand=True, fill=tk.BOTH)
        self.queue = queue
        self.notebook = ttk.Notebook(self.master)
        self.tab0 = Gain_Frame(self.notebook)
        self.notebook.add(self.tab0, text="Gains")
        self.tcp_comms = Tcp_Comms(ip_addr, tcp_port, self.text_display, do_quit)
        if self.tcp_comms.sock == None:
            self.master.destroy()
            exit(255)


        if self.tcp_comms.tcp_params.restore():
            self.tcp_comms.send_tcp_params()
        else:
            self.tcp_comms.send_tcp_params_null()
        self.tcp_comms.recv_tcp_params()

        #print("saturation" + str(self.tcp_comms.tcp_params.saturation))
        #print("ISO" + str(self.tcp_comms.tcp_params.ISO))
        #print("rotation" + str(self.tcp_comms.tcp_params.rotation))
        #print("hflip" + str(self.tcp_comms.tcp_params.hflip))
        #print("vflip" + str(self.tcp_comms.tcp_params.vflip))
        #print("shutter_speed" + str(self.tcp_comms.tcp_params.shutter_speed))
        #print("awb_gains_r" + str(self.tcp_comms.tcp_params.awb_gains_r))
        #print("awb_gains_b" + str(self.tcp_comms.tcp_params.awb_gains_b))
        #print("test_img_enable" + str(self.tcp_comms.tcp_params.test_img_enable))
        #print("yuv_write" + str(self.tcp_comms.tcp_params.yuv_write))
        #print("jpg_write" + str(self.tcp_comms.tcp_params.jpg_write))
        #print("detect_yuv" + str(self.tcp_comms.tcp_params.detect_yuv))
        #print("blob_y " + str(self.tcp_comms.tcp_params.blob_y_min) + ".." + str(self.tcp_comms.tcp_params.blob_y_max))
        #print("blob_u " + str(self.tcp_comms.tcp_params.blob_u_min) + ".." + str(self.tcp_comms.tcp_params.blob_u_max))
        #print("blob_v " + str(self.tcp_comms.tcp_params.blob_v_min) + ".." + str(self.tcp_comms.tcp_params.blob_v_max))
        #print("analog_gain_target" + str(self.tcp_comms.tcp_params.analog_gain_target))
        #print("analog_gain_tol" + str(self.tcp_comms.tcp_params.analog_gain_tol))
        #print("digital_gain_target" + str(self.tcp_comms.tcp_params.digital_gain_target))
        #print("digital_gain_tol" + str(self.tcp_comms.tcp_params.digital_gain_tol))
        #print("crosshairs_x" + str(self.tcp_comms.tcp_params.crosshairs_x))
        #print("crosshairs_y" + str(self.tcp_comms.tcp_params.crosshairs_y))
        self.tab1 = Cam_Param_Frame(self.notebook, self.tcp_comms, self.text_display)
        self.notebook.add(self.tab1, text="Cam Config")
        self.tab2 = Yuv_Frame(self.notebook, self.tcp_comms);
        self.notebook.add(self.tab2, text="YUV Color")
        self.image_label = ttk.Label(self.master)# label for the video frame
        self.image_label.pack(side=tk.LEFT, expand=False)
        self.image_label.bind('<Button-3>', self.on_right_click)
        self.do_show = True
        self.hide_button = tk.Button(self.master, text="<", height=20, command=self.show_hide)
        self.hide_button.pack(side=tk.LEFT, expand=False)
        self.notebook.pack(side=tk.LEFT, expand=False)
        self.master.after(0, func=lambda: self.update_all())
        self.crosshairs_position = crosshairs_position

    def show_hide(self):
        if self.do_show:
            self.do_show = False
            self.notebook.pack_forget()
            self.text_display.pack(side=tk.BOTTOM, expand=True, fill=tk.BOTH, ipady=0)
            self.text_display.pack_forget()
            self.image_label.pack_forget()
            self.hide_button.pack_forget()
            self.image_label.pack(side=tk.LEFT, expand=False, ipady=0)
            self.hide_button.pack(side=tk.LEFT, expand=False)
            self.hide_button.configure(text=">")
            self.crosshairs_position.hide()
        else:
            self.do_show = True
            self.image_label.pack_forget()
            self.hide_button.pack_forget()
            self.text_display.pack(side=tk.BOTTOM, expand=True, fill=tk.BOTH)
            self.image_label.pack(side=tk.LEFT, expand=False)
            self.hide_button.pack(side=tk.LEFT, expand=False)
            self.notebook.pack(side=tk.LEFT, expand=False)
            self.hide_button.configure(text="<")
            self.crosshairs_position.show()

    def on_right_click(self, event):
        x = event.x - self.BORDER_WIDTH
        y = event.y - self.BORDER_WIDTH
        if x < 0: x = 0
        if y < 0: y = 0
        self.tcp_comms.send_crosshairs(x, y)
        self.crosshairs_position.set(x, y)

    def update_image(self, cv_img, flags):
        im = cv2.cvtColor(cv_img, cv2.COLOR_BGR2RGB)
        a = PIL.Image.fromarray(im)
        if self.crosshairs_position.x.value < 0:
            width, height = a.size
            self.crosshairs_position.x.value = width / 2
            self.crosshairs_position.y.value = height / 2
        b = PIL.ImageTk.PhotoImage(image=a)
        bit1 = 0x1 & (flags >> 1)
        bit2 = 0x1 & (flags >> 2)
        if bit1:
            if bit2:
                color_str = "yellow"
            else:
                color_str = "green"
        else:
            color_str = "red"
        self.image_label.configure(background=color_str, borderwidth=self.BORDER_WIDTH, image=b)
        self.image_label._image_cache = b  # avoid garbage collection
        self.master.update()

    def update_all(self):
        queue_entry = self.queue.get()
        if queue_entry.do_quit:
            self.quit()
        else:
            self.update_image(queue_entry.cv_img, queue_entry.flags)
            self.tab0.update(queue_entry)
            self.tab1.update(queue_entry)
            self.tab2.update(queue_entry)
            self.tcp_comms.recv_text_msg()
            self.master.after(0, func=lambda: self.update_all())

    def quit(self):
        self.master.quit() # quit mainloop()

class Image_Capture_Queue_Entry():
    """Holds the current image and metadata extracted from the image header; this is sent from image_capture to dash_696 via a queue."""
    def __init__(self,
                 cv_img,
                 exposure_secs = 0.0,
                 analog_gain = 0.0,
                 digital_gain = 0.0,
                 awb_red_gain = 0.0,
                 awb_blue_gain = 0.0,
                 y = 0,
                 u = 0,
                 v = 0,
                 flags = 0,
                 bits_per_second = 0.0,
                 frames_per_second = 0.0):
        self.do_quit = False
        rows, cols, channels = cv_img.shape
        if rows == 0 and cols == 0:
            self.do_quit = True
        self.cv_img = cv_img
        self.exposure_secs = exposure_secs
        self.analog_gain = analog_gain
        self.digital_gain = digital_gain
        self.awb_red_gain = awb_red_gain
        self.awb_blue_gain = awb_blue_gain
        self.y = y
        self.u = u
        self.v = v
        self.flags = flags
        self.bits_per_second = bits_per_second
        self.frames_per_second = frames_per_second

def parse_tif_tags(jpg):
    TIFOFF = 12
    mm, version, ifd_offset, ifd_count = struct.unpack('!HHIH', jpg[TIFOFF:TIFOFF+10])
    exposure = 0
    analog_gain = 0.0
    digital_gain = 0.0
    awb_red_gain = 0.0
    awb_blue_gain = 0.0
    gain_offset = 0
    gain_count = 0
    bbox_offset = 0
    bbox_count = 0
    y = 0
    u = 0
    v = 0
    flags = 0
    for ii in range(0, ifd_count):
        start_off = TIFOFF + ifd_offset + 2 + ii * 12
        tag, type, count, num = struct.unpack('!HHII', jpg[start_off:start_off + 12])
        if tag == 0x9697:
            exposure = num
        elif tag == 0x9698:
            gain_count = count
            gain_offset = num
        elif tag == 0x9699:
            tag, type, count, y, u, v, flags = struct.unpack('!HHIBBBB', jpg[start_off:start_off + 12])
        elif tag == 0x9696:
            bbox_count = count
            bbox_offset = num

    if (gain_offset > 0 and gain_count == 4):
        off = TIFOFF + gain_offset
        analog_gain, digital_gain, awb_red_gain, awb_blue_gain = struct.unpack('!ffff', jpg[off:off + 16])

    rect_list = []
    if bbox_offset > 0:
        for ii in range(0, bbox_count / 4):
            start_off = TIFOFF + bbox_offset + ii * 8
            x0, y0, x1, y1 = struct.unpack('!HHHH', jpg[start_off:start_off + 8])
            rect_list.append(((x0, y0), (x1, y1)))
    return (exposure, analog_gain, digital_gain, awb_red_gain, awb_blue_gain, y, u, v, flags, rect_list)

def connect_to_server(ip_addr, port):
    connected = False
    ip_port = ip_addr + ":" + port
    while not connected:
        try:
            stream = urllib.urlopen('http://' + ip_port + '/?action=stream')
            connected = True
        except IOError as e:
            print("can't urlopen " + ip_port + " " + str(e))
    return stream

def draw_crosshairs(img, crosshairs_position):
    if crosshairs_position.do_show.value:
        LEN = 3  # length in pixels of each component of crosshairs mark
        COLOR = (0, 255, 255)
        rows, cols, channels = img.shape
        x = int(crosshairs_position.x.value + 0.5)
        y = int(crosshairs_position.y.value + 0.5)
        cv2.line(img, (x, y - 2 * LEN), (x, y - LEN), COLOR)
        cv2.line(img, (x, y + 2 * LEN), (x, y + LEN), COLOR)
        cv2.line(img, (x - 2 * LEN, y), (x - LEN, y), COLOR)
        cv2.line(img, (x + 2 * LEN, y), (x + LEN, y), COLOR)


#multiprocessing image processing functions-------------------------------------
def image_capture(ip_addr, queue, do_quit, crosshairs_position):
    bytes = ''
    byte_count = 0
    start_secs = time.time()
    port = "8080"
    stream = connect_to_server(ip_addr, port)
    while not do_quit.value:
        buf = stream.read(1024)
        byte_count += len(buf)
        bytes += buf
        a = bytes.find('\xff\xd8')
        b = bytes.find('\xff\xd9')
        if a != -1 and b != -1:
            jpg = bytes[a:b + 2]
            exposure, analog_gain, digital_gain, awb_red_gain, awb_blue_gain, y, u, v, flags, rect_list = parse_tif_tags(jpg)
            bytes = bytes[b + 2:]
            i = cv2.imdecode(np.fromstring(jpg, dtype=np.uint8), cv2.CV_LOAD_IMAGE_COLOR)
            for rect in rect_list:
                cv2.rectangle(i, rect[0], rect[1], (0, 0, 255))
            draw_crosshairs(i, crosshairs_position)
            frame_secs = time.time() - start_secs
            frames_per_second = 0.0
            bits_per_second = 0.0
            if frame_secs > 0:
                bits_per_second = 8.0 * byte_count / frame_secs
                frames_per_second = 1.0 / frame_secs
            start_secs = time.time()
            byte_count = 0
            queue.put(Image_Capture_Queue_Entry(i, exposure/1000.0, analog_gain, digital_gain, awb_red_gain,
                                                awb_blue_gain, y, u, v, flags, bits_per_second, frames_per_second))
    null_image = np.zeros((0, 0, 3), np.uint8)
    queue.put(Image_Capture_Queue_Entry(null_image))

class Quitter():
    def __init__(self):
        self.do_quit = multiprocessing.Value('d',0)
    def quit(self):
        self.do_quit.value = 1

class Crosshairs_Position():
    def __init__(self):
        self.x = multiprocessing.Value('d', -1)
        self.y = multiprocessing.Value('d', -1)
        self.do_show = multiprocessing.Value('d', 1)
    def set(self, x, y):
        self.x.value = x
        self.y.value = y
    def show(self):
        self.do_show.value = 1
    def hide(self):
        self.do_show.value = 0

if __name__ == '__main__':
    SERVER_IP_ADDR = "10.6.96.96"
    TCP_PORT = 10696
    queue = multiprocessing.Queue()
    root = tk.Tk()
    root.wm_title("dash696")
    quitter = Quitter()
    crosshairs_position = Crosshairs_Position()
    root.protocol("WM_DELETE_WINDOW", quitter.quit) # quit if window is deleted

    dash_696 = Dash_696(SERVER_IP_ADDR, TCP_PORT, queue, crosshairs_position, quitter.do_quit)
    p = multiprocessing.Process(target=image_capture, args=(SERVER_IP_ADDR, queue, quitter.do_quit, crosshairs_position))
    p.start()
    root.mainloop()
    p.join()
    root.destroy()

