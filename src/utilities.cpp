#include <Arduino.h>
#include "constants.hh"
#include "User_Setup.h"

long blPreviousMillis = 0;
long blInterval = 1000;
bool backlightOn = false;

void turnBacklightOnOff()
{

    unsigned long blCurrentMillis = millis();
    if (blCurrentMillis - blPreviousMillis >= blInterval)
    {
        blPreviousMillis = blCurrentMillis;
        // Place any code here that you want to run at the specified interval

        /*  Read the LDR value and map it to a backlight value for the TFT display
            The LDR value is inverted, so that when the LDR is in darkness, the backlight is at maximum brightness (255)
            When the LDR is in bright light, the backlight is at minimum brightness (20)
        */
        uint16_t lightlevel = analogRead(LDR);
        lightlevel = constrain(lightlevel, 0, 600);
        if (lightlevel > 90)
        {
            digitalWrite(TFT_BL, TFT_BACKLIGHT_OFF);
            backlightOn = false;
        }
        else
        {
            digitalWrite(TFT_BL, TFT_BACKLIGHT_ON);
            backlightOn = true;
        }
    }
}