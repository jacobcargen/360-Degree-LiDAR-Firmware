#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "Motor.h"
#include "LiDAR.h"
#include "BLE.h"

const float FULL_ROTATION_DEG = 360.0f;
const int MOTOR_STEPS_PER_REV = 200;
const int MICROSTEPS = 8;
const int STEPS_PER_DIRECTION = MOTOR_STEPS_PER_REV * MICROSTEPS;

const unsigned long SERIAL_BAUD_RATE = 115200;
const int BOOT_DELAY_MS = 1000;
const int DISCONNECTED_DELAY_MS = 100;
const int IDLE_LIDAR_DELAY_US = 10000;
const int IDLE_LOOP_DELAY_MS = 10;
const int MANUAL_IDLE_DELAY_MS = 5;
const float DEG_PER_STEP = FULL_ROTATION_DEG / (MOTOR_STEPS_PER_REV * MICROSTEPS);

Motor motor;
LiDAR lidar;
BluetoothManager ble;

float absAngle = 0.0f;

void streamData(float angle, float dist, float str)
{
    ble.sendData(angle, dist, str);
}

void normalizeAngle(float& angle)
{
    if (angle < 0.0f)
    {
        angle += FULL_ROTATION_DEG;
    }

    if (angle >= FULL_ROTATION_DEG)
    {
        angle -= FULL_ROTATION_DEG;
    }
}

bool shouldStopSweep()
{
    return !ble.connected || ble.quit || !ble.autoMode;
}

bool shouldStopMotor()
{
    return shouldStopSweep() || !ble.motorEnabled;
}

void performBacklashCompensation()
{
    for (int i = 0; i < ble.backlash; i++)
    {
        if (shouldStopSweep())
        {
            return;
        }

        motor.step(ble.stepDelay);
    }
}

void performSweepSteps(bool clockwise)
{
    motor.setDirection(clockwise);

    for (int i = 0; i < STEPS_PER_DIRECTION; i++)
    {
        if (shouldStopMotor())
        {
            return;
        }

        motor.step(ble.stepDelay);

        if (clockwise)
        {
            motor.stepsFromHome++;
            absAngle -= DEG_PER_STEP;
        }
        else
        {
            motor.stepsFromHome--;
            absAngle += DEG_PER_STEP;
        }

        normalizeAngle(absAngle);

        lidar.collectBest(absAngle, ble.stepDelay, streamData);
    }
}

void sweep()
{
    motor.setDirection(true);

    performBacklashCompensation();
    performSweepSteps(true);

    if (shouldStopMotor())
    {
        return;
    }
    motor.setDirection(false);

    performBacklashCompensation();
    performSweepSteps(false);
}

void setup()
{
    Serial.begin(SERIAL_BAUD_RATE);

    delay(BOOT_DELAY_MS);

    motor.init();
    lidar.init();

    ble.init();
    ble.setConnectionCallback(new BLEConnectionHandler());
    ble.setCommandCallback(new BLECommandHandler());
}

void loop()
{
    if (!ble.connected)
    {
        delay(DISCONNECTED_DELAY_MS);
        return;
    }

    if (ble.setHome)
    {
        ble.setHome = false;
        absAngle = 0.0f;
        motor.stepsFromHome = 0;
    }

    if (ble.returnHome)
    {
        ble.returnHome = false;
        motor.returnHome(absAngle, ble.stepDelay);
    }

    if (ble.quit)
    {
        ble.quit = false;

        motor.returnHome(absAngle, ble.stepDelay);

        ble.motorEnabled = false;
        motor.disable();
    }

    if (!ble.motorEnabled)
    {
        lidar.collectBest(absAngle, IDLE_LIDAR_DELAY_US, streamData);

        delay(IDLE_LOOP_DELAY_MS);
        return;
    }

    if (!ble.autoMode)
    {
        int jog = ble.jogRequest;

        ble.jogRequest = 0;

        if (jog != 0)
        {
            bool clockwise = (jog > 0);

            motor.setDirection(clockwise);

            for (int i = 0; i < ble.jogSteps; i++)
            {
                if (!ble.connected || ble.quit)
                {
                    motor.returnHome(absAngle, ble.stepDelay);
                    return;
                }

                motor.step(ble.stepDelay);

                if (clockwise)
                {
                    motor.stepsFromHome++;
                    absAngle -= DEG_PER_STEP;
                }
                else
                {
                    motor.stepsFromHome--;
                    absAngle += DEG_PER_STEP;
                }

                normalizeAngle(absAngle);

                lidar.collectBest(absAngle, ble.stepDelay, streamData);
            }
        }
        else
        {
            lidar.collectBest(absAngle, IDLE_LIDAR_DELAY_US, streamData);

            delay(MANUAL_IDLE_DELAY_MS);
        }

        return;
    }

    sweep();
}