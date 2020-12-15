#-------------------------------------------------------
# Copyright (c) 2020, Elehobica
# Released under the BSD-2-Clause
# refer to https://opensource.org/licenses/BSD-2-Clause
#-------------------------------------------------------

import sys
import struct
from PIL import Image

args = sys.argv
img = Image.open(args[1])
out_file = args[2]

img_resize = img.resize((80, 80))
#img_resize.save("_script/cover.bmp")

#f = open("_script/cover.bin", 'wb')
f = open(out_file, 'wb')

checker = False
grad_r = False

for y in range(0, 80):
    for x in range(0, 80):
        val = img_resize.getpixel((x, y))
        (r, g, b) = val
        # 565: RGB
        if (grad_r):
            r = int(x * 255 / 79)
            g = 255
            b = 255
            r = (r >> 3) & 0x1f
            g = (g >> 2) & 0x3f
            b = (b >> 3) & 0x1f
            val565 = (r<<11) | (g<<5) | (b)
        elif (checker):
            if (x < 40 and y < 40):
                val565 = 0xffff # white
            elif (x >= 40 and y < 40):
                val565 = 0xf800 # red
            elif (x < 40 and y >= 40):
                val565 = 0x07e0 # green
            elif (x >= 40 and y >= 40):
                val565 = 0x001f # blue
        else:
            r = (r >> 3) & 0x1f
            g = (g >> 2) & 0x3f
            b = (b >> 3) & 0x1f
            val565 = (r<<11) | (g<<5) | (b)
        val0 = val565 & 0xff
        val1 = (val565>>8) & 0xff
        f.write(struct.pack('2B', val1, val0)) # byte order swapped

f.close()
