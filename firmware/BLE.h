#ifndef BLE_H
#define BLE_H

const char* SERVICE_UUID =
    "12345678-1234-1234-1234-123456789abc";

const char* CHAR_LIDAR_UUID =
    "12345678-1234-1234-1234-123456789abd";

const char* CHAR_CMD_UUID =
    "12345678-1234-1234-1234-123456789abe";

const char* BLE_DEVICE_NAME = "LiDAR360";

const int PACKET_SIZE = 12;

const int ANGLE_OFFSET = 0;
const int DIST_OFFSET = 4;
const int STRENGTH_OFFSET = 8;

const int MAX_NOTIFY_FAILS = 20;

const int DEFAULT_STEP_DELAY = 2600;
const int DEFAULT_JOG_STEPS = 10;
const int DEFAULT_BACKLASH = 10;

const int MIN_STEP_DELAY = 500;
const int MAX_STEP_DELAY = 20000;

const int MIN_JOG_STEPS = 1;
const int MAX_JOG_STEPS = 1600;

const int MIN_BACKLASH = 0;
const int MAX_BACKLASH = 100;

extern class BluetoothManager ble;

class BLECommandHandler : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic* ch);
};

class BLEConnectionHandler : public BLEServerCallbacks
{
    void onConnect(BLEServer* s);
    void onDisconnect(BLEServer* s);
};

class BluetoothManager
{
public:
    bool connected = false;

    volatile int notifyFails = 0;

    volatile bool motorEnabled = false;
    volatile bool autoMode = true;

    volatile int jogRequest = 0;

    volatile bool setHome = false;
    volatile bool returnHome = false;
    volatile bool quit = false;

    volatile int stepDelay = DEFAULT_STEP_DELAY;
    volatile int jogSteps = DEFAULT_JOG_STEPS;
    volatile int backlash = DEFAULT_BACKLASH;

    BLECharacteristic* lidar_char = nullptr;
    BLECharacteristic* cmd_char = nullptr;

    BLEServer* server = nullptr;

    void init()
    {
        BLEDevice::init(BLE_DEVICE_NAME);
        server = BLEDevice::createServer();
        BLEService* svc = server->createService(SERVICE_UUID);

        lidar_char = svc->createCharacteristic(
            CHAR_LIDAR_UUID,
            BLECharacteristic::PROPERTY_NOTIFY
        );

        lidar_char->addDescriptor(new BLE2902());

        cmd_char = svc->createCharacteristic(
            CHAR_CMD_UUID,
            BLECharacteristic::PROPERTY_WRITE |
            BLECharacteristic::PROPERTY_WRITE_NR
        );

        svc->start();

        BLEAdvertising* adv =BLEDevice::getAdvertising();
        adv->addServiceUUID(SERVICE_UUID);
        adv->setScanResponse(true);
        adv->start();
    }

    void setCommandCallback(BLECharacteristicCallbacks* cb)
    {
        cmd_char->setCallbacks(cb);
    }

    void setConnectionCallback(BLEServerCallbacks* cb)
    {
        server->setCallbacks(cb);
    }

    bool sendData(float angle, float dist, float strength)
    {
        if (!connected || lidar_char == nullptr)
        {
            return false;
        }

        uint8_t pkt[PACKET_SIZE];

        memcpy(&pkt[ANGLE_OFFSET], &angle, sizeof(float));
        memcpy(&pkt[DIST_OFFSET], &dist, sizeof(float));
        memcpy(&pkt[STRENGTH_OFFSET], &strength, sizeof(float));

        lidar_char->setValue(pkt, PACKET_SIZE);

        try
        {
            lidar_char->notify();

            notifyFails = 0;

            return true;
        }
        catch (...)
        {
            notifyFails++;

            if (notifyFails > MAX_NOTIFY_FAILS)
            {
                Serial.println("Too many notify fails");
                notifyFails = 0;
                if (server)
                {
                    server->disconnect(server->getConnId());
                }
                connected = false;
            }

            return false;
        }
    }
};

void BLECommandHandler::onWrite(BLECharacteristic* ch)
{
    extern class Motor motor;
    String val = ch->getValue();

    if (val.length() == 0 )
    {
        return;
    }
    char cmd = val[0];
    String arg = val.substring(1);
    switch (cmd)
    {
        case 'E':
        {
            ble.motorEnabled = true;


            motor.enable();

            ble.returnHome = true;

            Serial.println("enable");

            break;
        }
        case 'D':
        {
            ble.motorEnabled = false;
            motor.disable();
            Serial.println("disable");

            break;
        }
        case 'A':
        {
            ble.autoMode = true;

            Serial.println("Auto");

            break;
        }
        case 'M':
        {
            ble.autoMode = false;

            Serial.println("manual");

            break;
        }
        case 'L':
        {
            ble.jogRequest = -1;

            Serial.println("left");

            break;
        }
        case 'R':
        {
            ble.jogRequest = 1;

            Serial.println("right ");

            break;
        } 
        case 'H':
        {
            ble.setHome = true;

            Serial.println("go home");

            break;
        }
        case 'Q':
        {
            ble.quit = true;

            Serial.println("Quit");

            break;
        }
        case 'S':
        {
            if (arg.length() > 0)
            {
                int v = arg.toInt();

                if (
                    v >= MIN_STEP_DELAY &&
                    v <= MAX_STEP_DELAY
                )
                {
                    ble.stepDelay = v;
                }
            }

            break;
        }

        case 'J':
        {
            if (arg.length() > 0)
            {
                int v = arg.toInt();

                if (
                    v >= MIN_JOG_STEPS &&
                    v <= MAX_JOG_STEPS
                )
                {
                    ble.jogSteps = v;
                }
            }

            break;
        }
        case 'B':
        {
            if (arg.length() > 0)
            {
                int v = arg.toInt();

                if (
                    v >= MIN_BACKLASH &&
                    v <= MAX_BACKLASH
                )
                {
                    ble.backlash = v;
                }
            }

            break;
        }
    }
}

void BLEConnectionHandler::onConnect(BLEServer* s)
{
    ble.connected = true;

    Serial.println("Connected");
}

void BLEConnectionHandler::onDisconnect(BLEServer* s)
{
    ble.connected = false;

    Serial.println("Disconnected");

    ble.server->startAdvertising();
}

#endif