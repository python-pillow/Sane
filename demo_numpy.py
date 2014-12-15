#!/usr/bin/env python

#
# Shows how to scan a 8/16 bit grayscale or 8 bit RGB image into a numpy object
#       16 bit RGB can be tried if supported by scanner and drivers
#

from __future__ import print_function

# Get the path set up to find PIL modules if not installed yet:
import sys
sys.path.append('./')
from numpy import *
import sane
import Image

def toImage(arr):
    if arr.dtype != 'uint8' :
        arr_c = arr - arr.min()
        maxi = arr_c.max()
        if maxi > 0 : 
            arr_c *= (255./maxi)
        arr = arr_c.astype('uint8')
        print("Array shape %s, size %d, type %s, range %d-%d, mean %.1f, std %.1f"%(repr(arr.shape), arrx.size, arr.dtype, arr.min(), arr.max(), arr.mean(), arr.std()))
    # need to swap coordinates btw array and image (with [::-1])
    if len(arr.shape) == 2 :
        im = Image.frombytes('L', arr.shape[::-1], arr.tostring())
    else :
        im = Image.frombytes('RGB', (arr.shape[1],arr.shape[0]), arr.tostring())
    return im

print('SANE version:', sane.init())
devices = sane.get_devices()
print('Available devices=', devices)

s = sane.open(devices[0][0])

# Set scan parameters
s.resolution=200
s.br_x=120 
s.br_y=140
#for depth in [16] : 
for depth in [8,16] :
    for mode in  ['gray', 'color'] :
        # Set scan parameters
        s.mode = mode
        s.depth = depth
        print('Device parameters:', s.get_parameters())

        if mode == "color" and depth == 16 : 
            print ("Skipping epkowa? bug in color16")
            s.close()        
            sys.exit()
        arrx = s.arr_scan()
        print("Array shape %s, size %d, type %s, range %d-%d, mean %.1f, std %.1f"%(    
            repr(arrx.shape), arrx.size, arrx.dtype, arrx.min(), arrx.max(), arrx.mean(), arrx.std()))
        toImage(arrx).show()
s.close()        
        