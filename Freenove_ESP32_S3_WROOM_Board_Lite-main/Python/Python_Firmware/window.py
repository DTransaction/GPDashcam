import os
import sys
import time

os.system("python esptool/esptool.py --chip esp32s3  erase_flash")

os.system("python esptool/esptool.py --chip esp32s3 --baud 2000000 write_flash -z 0 ESP32_GENERIC_S3-SPIRAM_OCT-20240602-v1.23.0.bin")
