# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/danny/esp/esp-idf/components/bootloader/subproject"
  "/home/danny/esp/esp-idf/examples/peripherals/uart/nmea0183_parser/build/bootloader"
  "/home/danny/esp/esp-idf/examples/peripherals/uart/nmea0183_parser/build/bootloader-prefix"
  "/home/danny/esp/esp-idf/examples/peripherals/uart/nmea0183_parser/build/bootloader-prefix/tmp"
  "/home/danny/esp/esp-idf/examples/peripherals/uart/nmea0183_parser/build/bootloader-prefix/src/bootloader-stamp"
  "/home/danny/esp/esp-idf/examples/peripherals/uart/nmea0183_parser/build/bootloader-prefix/src"
  "/home/danny/esp/esp-idf/examples/peripherals/uart/nmea0183_parser/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/danny/esp/esp-idf/examples/peripherals/uart/nmea0183_parser/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/danny/esp/esp-idf/examples/peripherals/uart/nmea0183_parser/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
