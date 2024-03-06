#! /usr/bin/env pybricks-micropython

from micropython import const
from pybricks.tools import wait
import array
import ustruct
import select
import _thread

EV_KEY = const(0x01)
EV_ABS = const(0x03)


class Buttons:
    BTN_X = const(304)
    BTN_O = const(305)
    BTN_TRI = const(308)
    BTN_SQ = const(307)
    BTN_LSTICK = const(317)
    BTN_RSTICK = const(318)
    BTN_SELECT = const(314)
    BTN_START = const(315)
    BTN_BUMP_L = const(310)
    BTN_BUMP_R = const(311)


_AXIS_DPAD_X = const(16)
_AXIS_DPAD_Y = const(17)
_AXIS_LSTK_X = const(0)
_AXIS_LSTK_Y = const(1)
_AXIS_LTRIG = const(2)  # max = 255
_AXIS_RSTK_X = const(3)
_AXIS_RSTK_Y = const(4)
_AXIS_RTRIG = const(5)

# The directions of each axis. Not sure if we'll use these.
_AXIS_UP = const(-1)
_AXIS_DOWN = const(1)
_AXIS_LEFT = const(-1)
_AXIS_RIGHT = const(1)

CONTROLLER_FILENAME = (
    "/dev/input/by-id/usb-Razer_Razer_Wolverine_V2_Pro_2.4-event-joystick"
)
INPUT_EVENT_FMT = "llHHi"
INPUT_EVENT_SZ = ustruct.calcsize(INPUT_EVENT_FMT)


class Controller:
    def __init__(self):
        self_axis_dpad_x = 0
        self_axis_dpad_y = 0
        self_axis_lstick_x = 0
        self_axis_lstick_y = 0
        self_axis_rstick_x = 0
        self_axis_rstick_y = 0
        self_axis_ltrig = 0
        self_axis_rtrig = 0
        self.buttons = []
        self.watching = False

    def watch(self, filename):
        self.watching = True
        _thread.start_new_thread(
            Controller._watch,
            (
                self,
                filename,
            ),
        )

    def _watch(self, filename):
        with open(filename, "rb") as f:

            while self.watching:
                # Note: this may not exit if the input stops producing events.
                # We tried using the poller and found that it stopped timing
                # out after it first got data, so there didn't seem to be
                # any benefit to using it.
                data = f.read(INPUT_EVENT_SZ)
                result = ustruct.unpack_from(INPUT_EVENT_FMT, data)
                self._process_event(result[2], result[3], result[4])

    def _process_event(self, type, code, value):
        if type == EV_KEY:
            if value == 1:
                self.buttons.append(code)
            else:
                self.buttons.remove(code)
        elif type == EV_ABS:
            if code == _AXIS_LSTK_X:
                self_axis_lstick_x = value
            elif code == _AXIS_LSTK_Y:
                self_axis_lstick_y = value
            elif code == _AXIS_RSTK_X:
                self_axis_rstick_x = value
            elif code == _AXIS_RSTK_Y:
                self_axis_rstick_y = value
            elif code == _AXIS_DPAD_X:
                self_axis_dpad_x = value
            elif code == _AXIS_DPAD_Y:
                self_axis_dpad_y = value
            elif code == _AXIS_LTRIG:
                self_axis_ltrig = value
            elif code == _AXIS_RTRIG:
                self_axis_rtrig = value
        elif type == 0:
            pass
        else:
            print(type, code, value)

    def unwatch(self):
        self.watching = False
