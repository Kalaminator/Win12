# usage: w16mouse.py x,y[,hold_s] ...  moves a virtual absolute pointer (never clicks)
#        w16mouse.py drag x1,y1 x2,y2 [steps]  press left button at 1, move to 2 in steps (default 20), release
import fcntl, os, struct, sys, time
UI_SET_EVBIT, UI_SET_KEYBIT, UI_SET_ABSBIT = 0x40045564, 0x40045565, 0x40045567
UI_DEV_CREATE, UI_DEV_DESTROY = 0x5501, 0x5502
EV_SYN, EV_KEY, EV_ABS, BTN_LEFT = 0, 1, 3, 0x110
fd = os.open('/dev/uinput', os.O_WRONLY | os.O_NONBLOCK)
fcntl.ioctl(fd, UI_SET_EVBIT, EV_KEY); fcntl.ioctl(fd, UI_SET_KEYBIT, BTN_LEFT)
fcntl.ioctl(fd, UI_SET_EVBIT, EV_ABS); fcntl.ioctl(fd, UI_SET_ABSBIT, 0); fcntl.ioctl(fd, UI_SET_ABSBIT, 1)
absmax = [0] * 64; absmax[0] = absmax[1] = 65535
os.write(fd, struct.pack('80sHHHHi', b'w16-mouse', 3, 1, 1, 1, 0) + struct.pack('64i', *absmax) + struct.pack('64i', *([0] * 64)) * 3)
fcntl.ioctl(fd, UI_DEV_CREATE); time.sleep(1)
ev = lambda t, c, v: os.write(fd, struct.pack('llHHi', 0, 0, t, c, v))
def mv(x, y): ev(EV_ABS, 0, int(x * 65535 / 2559)); ev(EV_ABS, 1, int(y * 65535 / 1439)); ev(EV_SYN, 0, 0)
if sys.argv[1] == "drag":
    (x1, y1), (x2, y2) = (map(int, a.split(',')) for a in sys.argv[2:4]); n = int(sys.argv[4]) if len(sys.argv) > 4 else 20
    mv(x1, y1); time.sleep(0.3); ev(EV_KEY, BTN_LEFT, 1); ev(EV_SYN, 0, 0)
    for i in range(1, n + 1): mv(x1 + (x2 - x1) * i / n, y1 + (y2 - y1) * i / n); time.sleep(0.025)
    time.sleep(0.15); ev(EV_KEY, BTN_LEFT, 0); ev(EV_SYN, 0, 0); time.sleep(0.2); sys.argv = sys.argv[:1]
for a in sys.argv[1:]:
    x, y, *h = a.split(','); ev(EV_ABS, 0, int(int(x) * 65535 / 2559)); ev(EV_ABS, 1, int(int(y) * 65535 / 1439)); ev(EV_SYN, 0, 0)
    time.sleep(float(h[0]) if h else 0.3)
fcntl.ioctl(fd, UI_DEV_DESTROY); os.close(fd)
