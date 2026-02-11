#!/bin/bash
set -e

export PATH=$PATH:$HOME/bin
arduino-cli compile --fqbn esp32:esp32:esp32c3 firmware/
