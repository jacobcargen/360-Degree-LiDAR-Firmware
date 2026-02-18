#!/bin/bash
set -e

export PATH="$PATH:$HOME/bin"

FQBN="esp32:esp32:XIAO_ESP32C3"

arduino-cli compile --fqbn "$FQBN" firmware/