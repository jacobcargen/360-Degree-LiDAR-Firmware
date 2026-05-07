#ifndef LIDAR_H
#define LIDAR_H

class LiDAR
{
private:
    const static int MAX_SAMPLES = 50;

    const static int FRAME_SIZE = 9;
    const static uint8_t FRAME_HEADER = 0x59;

    const static int DEFAULT_BAUD_RATE = 115200;
    const static int HIGH_BAUD_RATE = 921600;

    const static int LIDAR_RX_PIN = 20;
    const static int LIDAR_TX_PIN = 21;

    const static int SERIAL_DELAY_MS = 100;

    const static int TARGET_SAMPLE_COUNT = 20;

    const static float DISTANCE_SCALE = 1.5f;
    const static float MAX_SIGNAL_STRENGTH = 65535.0f;
    const static float SIGNAL_PERCENT_SCALE = 100.0f;

    uint8_t frame[FRAME_SIZE];
    int rxCount = 0;

    uint8_t cmd_baud[8] =
    {
        0x5A, 0x08, 0x06, 0x00,
        0x10, 0x0E, 0x00, 0x86
    };
    
    uint8_t cmd_rate[6] =
    {
        0x5A, 0x06, 0x03,
        0xE8, 0x03, 0x4E
    };

    uint8_t cmd_save[4] =
    {
        0x5A, 0x04, 0x11, 0x6F
    };

public:
    void init()
    {
        Serial1.begin(
            DEFAULT_BAUD_RATE,
            SERIAL_8N1,
            LIDAR_RX_PIN,
            LIDAR_TX_PIN
        );

        delay(SERIAL_DELAY_MS);

        Serial1.write(cmd_baud, sizeof(cmd_baud));

        delay(SERIAL_DELAY_MS);

        Serial1.end();

        Serial1.begin(
            HIGH_BAUD_RATE,
            SERIAL_8N1,
            LIDAR_RX_PIN,
            LIDAR_TX_PIN
        );
        
        delay(SERIAL_DELAY_MS); 

        Serial1.write(cmd_rate, sizeof(cmd_rate));

        delay(SERIAL_DELAY_MS);

        Serial1.write(cmd_save, sizeof(cmd_save));

        delay(SERIAL_DELAY_MS);
    }

    bool readFrame(float& distance, float& strength)
    {
        while (Serial1.available())
        {
            uint8_t byte = Serial1.read();

            if (rxCount == 0 && byte != FRAME_HEADER)
            {
                continue;
            }

            if (rxCount == 1 && byte != FRAME_HEADER)
            {
                rxCount = 0;

                if (byte == FRAME_HEADER)
                {
                    frame[0] = FRAME_HEADER;
                    rxCount = 1;
                }

                continue;
            }

            frame[rxCount++] = byte;

            if (rxCount == FRAME_SIZE)
            {
                rxCount = 0;

                uint16_t sum = 0;

                for (int i = 0; i < FRAME_SIZE - 1; i++)
                {
                    sum += frame[i];
                }

                if ((sum & 0xFF) == frame[FRAME_SIZE - 1])
                {
                    int dist = frame[2] + (frame[3] << 8);
                    int str = frame[4] + (frame[5] << 8);

                    distance = dist / DISTANCE_SCALE;

                    strength =
                        (str / MAX_SIGNAL_STRENGTH) *
                        SIGNAL_PERCENT_SCALE;
                    /*
                    Serial.printf(
                        "Raw: %d cm, Str: %d\n",
                        (int)distance,
                        str
                    );
                    */
                    return true;
                }
            }
        }

        return false;
    }

    void collectBest(
        float angle,
        int waitUs,
        void (*streamFn)(float, float, float)
    )
    {
        float dists[MAX_SAMPLES];
        float strengths[MAX_SAMPLES];

        int count = 0;

        unsigned long start = micros();

        while (count < TARGET_SAMPLE_COUNT)
        {
            float d;
            float s;

            if (readFrame(d, s))
            {
                dists[count] = d;
                strengths[count] = s;

                count++;
            }

            if ((long)(micros() - start) >= (long)waitUs)
            {
                break;
            }
        }

        if (count == 0)
        {
            return;
        }

        for (int i = 1; i < count; i++)
        {
            float kd = dists[i];
            float ks = strengths[i];

            int j = i - 1;

            while (j >= 0 && dists[j] > kd)
            {
                dists[j + 1] = dists[j];
                strengths[j + 1] = strengths[j];

                j--;
            }

            dists[j + 1] = kd;
            strengths[j + 1] = ks;
        }

        // Get closest
        const int CLOSEST_SAMPLE_INDEX = 0;

        float avg_s = strengths[CLOSEST_SAMPLE_INDEX];

        streamFn(
            angle,
            dists[CLOSEST_SAMPLE_INDEX],
            avg_s
        );
    }
};

#endif