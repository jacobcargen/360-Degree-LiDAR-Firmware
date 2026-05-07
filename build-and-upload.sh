#!/bin/bash
set -e

export PATH=$PATH:$HOME/bin
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C3 firmware/
arduino-cli upload --fqbn esp32:esp32:XIAO_ESP32C3 --port ${1:-/dev/ttyACM0} firmware/
