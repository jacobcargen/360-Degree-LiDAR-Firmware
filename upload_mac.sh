#!/bin/bash
set -e

export PATH=$PATH:$HOME/bin
arduino-cli upload --fqbn esp32:esp32:esp32c3 --port ${1:-/dev/uv.usbmodem1101} firmware/
