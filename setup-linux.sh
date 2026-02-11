#!/bin/bash
set -e

arduino-cli config init --overwrite
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

mkdir -p firmware
cat > firmware/firmware.ino << 'EOF'
void setup() {
  Serial.begin(115200);
  Serial.println("Hello World ESP32-C3");
}

void loop() {
  delay(1000);
}
EOF

cat > build.sh << 'EOF'
#!/bin/bash
set -e

export PATH=$PATH:$HOME/bin
arduino-cli compile --fqbn esp32:esp32:esp32c3 firmware/
EOF

cat > build-and-upload.sh << 'EOF'
#!/bin/bash
set -e

export PATH=$PATH:$HOME/bin
arduino-cli compile --fqbn esp32:esp32:esp32c3 firmware/
arduino-cli upload --fqbn esp32:esp32:esp32c3 --port ${1:-/dev/ttyUSB0} firmware/
EOF

chmod +x build.sh build-and-upload.sh

echo "Setup complete"
