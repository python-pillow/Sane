#!/usr/bin/env python

#
# Shows how to scan a color image into a PIL rgb-image
#

from __future__ import print_function
import sane

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

# Initiate the scan and get and Image object
dev.start()
im = dev.snap()

# Save/show output
im.save('test.png')
try:
    im.show()
except:
    print('Show failed.')

# Close the device
dev.close()
