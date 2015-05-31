#!/usr/bin/env python

from __future__ import print_function
import sane
import numpy
from PIL import Image

#
# Change these for 16bit / grayscale scans
#
depth = 8
mode = 'color'

#
# Initialize sane
#
ver = sane.init()
print('SANE version:', ver)

#
# Get devices
#
devices = sane.get_devices()
print('Available devices:', devices)

#
# Open first device
#
dev = sane.open(devices[0][0])

#
# Set some options
#
params = dev.get_parameters()
try:
    dev.depth = depth
except:
    print('Cannot set depth, defaulting to %d' % params[3])

try:
    dev.mode = mode
except:
    print('Cannot set mode, defaulting to %s' % params[0])

try:
    dev.br_x = 320.
    dev.br_y = 240.
except:
    print('Cannot set scan area, using default')

params = dev.get_parameters()
print('Device parameters:', params)

#
# Start a scan and get and PIL.Image object
#
dev.start()
im = dev.snap()
im.save('test_pil.png')


#
# Start another scan and get a numpy array object
#
# Initiate the scan and get and numpy array
dev.start()
arr = dev.arr_snap()
print("Array shape: %s, size: %d, type: %s, range: %d-%d, mean: %.1f, stddev: "
      "%.1f" % (repr(arr.shape), arr.size, arr.dtype, arr.min(), arr.max(),
                arr.mean(), arr.std()))

if arr.dtype == numpy.uint16:
    arr = (arr / 255).astype(numpy.uint8)

if params[0] == 'color':
    im = Image.frombytes('RGB', arr.shape[1:], arr.tostring(), 'raw', 'RGB', 0,
                         1)
else:
    im = Image.frombytes('L', arr.shape[1:], arr.tostring(), 'raw', 'L', 0, 1)

im.save('test_numpy.png')

#
# Close the device
#
dev.close()
