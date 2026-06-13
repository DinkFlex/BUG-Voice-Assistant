import board
import displayio
import busio
from adafruit_st7735r import ST7735R
import digitalio
import fourwire
import neopixel
import time
from adafruit_display_text import label
import terminalio
import math

# Release display
displayio.release_displays()

# ---------------- DISPLAY ----------------
tft_dc = board.IO14
tft_cs = board.IO4
tft_clk = board.IO18
tft_mosi = board.IO17
tft_rst = board.IO5

spi = busio.SPI(tft_clk, MOSI=tft_mosi)

display_bus = fourwire.FourWire(spi, command=tft_dc, chip_select=tft_cs, reset=tft_rst)
display = ST7735R(
    display_bus, rotation=90, width=160, height=80, rowstart=1, colstart=26, invert=True
)

# BACKLIGHT
tft_bl = board.IO38
led = digitalio.DigitalInOut(tft_bl)
led.direction = digitalio.Direction.OUTPUT
led.value = True

# RGB
pixel = neopixel.NeoPixel(board.IO45, 1, brightness=0.4, auto_write=True)

# =================================================
# SHOW IMAGE
# =================================================
bitmap_file = open("/BUG.bmp", "rb")
bitmap = displayio.OnDiskBitmap(bitmap_file)

tile_grid = displayio.TileGrid(
    bitmap,
    pixel_shader=getattr(bitmap, "pixel_shader", displayio.ColorConverter())
)

image_group = displayio.Group()
image_group.append(tile_grid)
display.root_group = image_group

time.sleep(4)
bitmap_file.close()

# =================================================
# THANK YOU SCREEN
# =================================================
thank_group = displayio.Group()
display.root_group = thank_group

title = label.Label(
    terminalio.FONT,
    text="SEPCIAL THANKS :)",
    color=0xFFFF00,
    scale=1
)
title.x = 30
title.y = 12
thank_group.append(title)

time.sleep(2)

display.root_group = displayio.Group()
time.sleep(0.3)

# =================================================
# SCROLL NAMES
# =================================================
scroll_group = displayio.Group()
display.root_group = scroll_group

backers = [
    "Jason Giambi","Shaun MacKay","Suji Goudar","Houdari Freeman",
    "Racquel Silvers","Mallory Tompsett","Lord Brown",
    "Fernando Q Macedo","Jeffrey Daniels","Michael Lord",
    "Richard Hart","Mark Crisp","Piotr Kajdas","Ellen Pruden",
    "Thomas Reinhart","Alexander White","Robert Foppa",
    "Nathan Sewell","Khalifa AlMarar","Antonio Abreu","YeoCheon Yun"
]

scroll_label = label.Label(
    terminalio.FONT,
    text="\n".join(backers),
    color=0xFFFFFF,
    scale=1
)
scroll_label.x = 10
scroll_label.y = display.height + 10
scroll_group.append(scroll_label)

line_height = 12
total_height = len(backers) * line_height

# =================================================
# RGB BREATHING
# =================================================
phase = 0.0
colors = [
    (255,0,0),(0,255,0),(0,0,255),
    (255,255,0),(255,0,255),(0,255,255)
]
color_index = 0

# =================================================
# LOOP
# =================================================
while True:

    # Scroll
    scroll_label.y -= 1
    if scroll_label.y < -total_height:
        scroll_label.y = display.height + 10

    # RGB
    brightness = (math.sin(phase) + 1) / 2
    r, g, b = colors[color_index]

    pixel[0] = (
        int(r * brightness),
        int(g * brightness),
        int(b * brightness)
    )

    phase += 0.05
    if phase >= 2 * math.pi:
        phase = 0
        color_index = (color_index + 1) % len(colors)

    time.sleep(0.03)