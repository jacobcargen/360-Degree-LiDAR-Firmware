#ifndef MOTOR_H
#define MOTOR_H

class Motor
{
private:
    constexpr static int DIR_PIN = 2;
    constexpr static int STEP_PIN = 3;
    constexpr static int SLP_PIN = 4;
    constexpr static int RST_PIN = 5;
    constexpr static int EN_PIN = 6;
    constexpr static int MS1_PIN = 7;
    constexpr static int MS2_PIN = 8;
    constexpr static int MS3_PIN = 9;

    constexpr static int DIRECTION_SETUP_DELAY_US = 5;
    constexpr static int STEP_PULSE_DELAY_US = 10;

    constexpr static float HOME_ANGLE = 0.0f;

    void setupPin(int pin)
    {
        pinMode(pin, OUTPUT);
    }

public:
    int stepsFromHome = 0;

    void init()
    {
        setupPin(DIR_PIN);
        setupPin(STEP_PIN);
        setupPin(SLP_PIN);
        setupPin(RST_PIN);
        setupPin(EN_PIN);
        setupPin(MS1_PIN);
        setupPin(MS2_PIN);
        setupPin(MS3_PIN);

        digitalWrite(EN_PIN, HIGH);
        digitalWrite(DIR_PIN, HIGH);
        digitalWrite(SLP_PIN, LOW);
        digitalWrite(RST_PIN, HIGH);

        // 1/8 STEP
        digitalWrite(MS1_PIN, HIGH);
        digitalWrite(MS2_PIN, HIGH);
        digitalWrite(MS3_PIN, LOW);
    }

    void enable()
    {
        digitalWrite(EN_PIN, LOW);
        digitalWrite(SLP_PIN, HIGH);
    }

    void disable()
    {
        digitalWrite(EN_PIN, HIGH);
        digitalWrite(SLP_PIN, LOW);
    }

    void setDirection(bool cw)
    {
        digitalWrite(DIR_PIN, cw ? HIGH : LOW);
        delayMicroseconds(DIRECTION_SETUP_DELAY_US);
    }

    void step(int delayUs)
    {
        digitalWrite(STEP_PIN, HIGH);
        delayMicroseconds(STEP_PULSE_DELAY_US);
        digitalWrite(STEP_PIN, LOW);
        delayMicroseconds(delayUs);
    }

    void returnHome(float& angle, int stepDelayUs)
    {
        int back = abs(stepsFromHome);

        if (back == 0)
        {
            return;
        }

        bool cw = (stepsFromHome < 0);

        setDirection(cw);

        for (int i = 0; i < back; i++)
        {
            step(stepDelayUs);
        }

        stepsFromHome = 0;
        angle = HOME_ANGLE;

    }
};

#endif