#!/usr/bin/env python

#
# Shows how to scan a color image into a numpy array
#

from __future__ import print_function
import sane
import numpy
from PIL import Image

# Change these for 16bit / grayscale scans
depth = 8
mode = 'color'

ver = sane.init()
print('SANE version:', ver)

devices = sane.get_devices()
print('Available devices:', devices)

dev = sane.open(devices[0][0])

params = dev.get_parameters()

try:
    dev.depth = depth
except:
    print('Cannot set depth, defaulting to %d' % params[3])

try:
    dev.mode = mode
except:
    print('Cannot set mode, defaulting to %s' % params[0])

params = dev.get_parameters()
print('Device parameters:', params)

try:
    dev.br_x = 320.
    dev.br_y = 240.
except:
    print('Cannot set scan area, using default')

# Initiate the scan and get and numpy array
dev.start()
arr = dev.arr_snap()

print("Array shape: %s, size: %d, type: %s, range: %d-%d, mean: %.1f, stddev: %.1f" %
      (repr(arr.shape), arr.size, arr.dtype, arr.min(), arr.max(), arr.mean(), arr.std()))

# Convert to PIL Image to save/show
if arr.dtype == numpy.uint16:
    arr = (arr / 255).astype(numpy.uint8)

if params[0] == 'color':
    im = Image.frombytes('RGB', params[2], arr.tostring(), 'raw', 'RGB', 0, 1)
else:
    im = Image.frombytes('L', params[2], arr.tostring(), 'L', 'RGB', 0, 1)

# Save/show output
im.save('test.png')
try:
    im.show()
except:
    print('Show failed.')

# Close the device
dev.close()
