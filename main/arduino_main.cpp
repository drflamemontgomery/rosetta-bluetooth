/****************************************************************************
http://retro.moe/unijoysticle2

Copyright 2021 Ricardo Quesada

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
****************************************************************************/

#include "sdkconfig.h"
#ifndef CONFIG_BLUEPAD32_PLATFORM_ARDUINO
#error "Must only be compiled when using Bluepad32 Arduino platform"
#endif  // !CONFIG_BLUEPAD32_PLATFORM_ARDUINO

#include <Arduino.h>
#include <Bluepad32.h>

#include <Wire.h>

//
// README FIRST, README FIRST, README FIRST
//
// Bluepad32 has a built-in interactive console.
// By default it is enabled (hey, this is a great feature!).
// But it is incompatible with Arduino "Serial" class.
//
// Instead of using "Serial" you can use Bluepad32 "Console" class instead.
// It is somewhat similar to Serial but not exactly the same.
//
// Should you want to still use "Serial", you have to disable the Bluepad32's console
// from "sdkconfig.defaults" with:
//    CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE=n

GamepadPtr myGamepads[BP32_MAX_GAMEPADS];

// This callback gets called any time a new gamepad is connected.
// Up to 4 gamepads can be connected at the same time.
void onConnectedGamepad(GamepadPtr gp) {
    bool foundEmptySlot = false;
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myGamepads[i] == nullptr) {
            Console.printf("CALLBACK: Gamepad is connected, index=%d\n", i);
            // Additionally, you can get certain gamepad properties like:
            // Model, VID, PID, BTAddr, flags, etc.
            GamepadProperties properties = gp->getProperties();
            Console.printf("Gamepad model: %s, VID=0x%04x, PID=0x%04x\n", gp->getModelName(), properties.vendor_id,
                           properties.product_id);
            myGamepads[i] = gp;
            foundEmptySlot = true;
            break;
        }
    }
    if (!foundEmptySlot) {
        Console.println("CALLBACK: Gamepad connected, but could not found empty slot");
    }
}

void onDisconnectedGamepad(GamepadPtr gp) {
    bool foundGamepad = false;

    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        if (myGamepads[i] == gp) {
            Console.printf("CALLBACK: Gamepad is disconnected from index=%d\n", i);
            myGamepads[i] = nullptr;
            foundGamepad = true;
            break;
        }
    }

    if (!foundGamepad) {
        Console.println("CALLBACK: Gamepad disconnected, but not found in myGamepads");
    }
}


/* Communications struct:
id: bits 7-5, controller id 0-7, bit 4, valid, bits 3-0, controller type
 buttons: up, down, left, right, a, b, x, y
 buttons: left stick button, right stick button, RB, LB, alt LB, alt RB (switch joycon), 0, 0
 left x axis
 left y axis
 right x axis
 right y axis
k secondary buttons: home, alt, start, select, 0, 0, 0, 0
 left trigger
 right trigger
 data 6 bytes
*/
struct commsGP{
    byte id;
    byte buttons1;
    byte buttons2;
    short lx_axis;
    short ly_axis;
    short rx_axis;
    short ry_axis;
    byte secondary_buttons;
    ushort left_trigger;
    ushort right_trigger;
    byte data[6];
};


commsGP array_gamepads[BP32_MAX_GAMEPADS];

//Writes MSB then LSB
void wireWriteShort(short value){
    Wire.write((byte)(value >> 8));
    Wire.write((byte)(value && 0xFF));
}

//Event request function called when a request is received
//More details of inter processor communication to implement
void i2cRequest(void) { 
    for(int i = 0; i < BP32_MAX_GAMEPADS; i++){
        Wire.write(array_gamepads[i].id);
        Wire.write(array_gamepads[i].buttons1);
        Wire.write(array_gamepads[i].buttons2);
        wireWriteShort(array_gamepads[i].lx_axis);
        wireWriteShort(array_gamepads[i].ly_axis);
        wireWriteShort(array_gamepads[i].rx_axis);
        wireWriteShort(array_gamepads[i].ry_axis);
        Wire.write(array_gamepads[i].secondary_buttons);
        wireWriteShort(array_gamepads[i].left_trigger);
        wireWriteShort(array_gamepads[i].right_trigger);
        for(int j = 0; j < 6; j++){
            Wire.write(array_gamepads[i].data[j]);
        }
    }
 }


// Arduino setup function. Runs in CPU 1
void setup() {
    Console.printf("Firmware: %s\n", BP32.firmwareVersion());

    // Setup the Bluepad32 callbacks
    BP32.setup(&onConnectedGamepad, &onDisconnectedGamepad);

    // "forgetBluetoothKeys()" should be called when the user performs
    // a "device factory reset", or similar.
    // Calling "forgetBluetoothKeys" in setup() just as an example.
    // Forgetting Bluetooth keys prevents "paired" gamepads to reconnect.
    // But might also fix some connection / re-connection issues.
    BP32.forgetBluetoothKeys();

    Wire.begin(0x20);
    Wire.onRequest(i2cRequest);
}

// Arduino loop function. Runs in CPU 1
void loop() {
    // This call fetches all the gamepad info from the NINA (ESP32) module.
    // Just call this function in your main loop.
    // The gamepads pointer (the ones received in the callbacks) gets updated
    // automatically.
    BP32.update();

    // It is safe to always do this before using the gamepad API.
    // This guarantees that the gamepad is valid and connected.
    for (int i = 0; i < BP32_MAX_GAMEPADS; i++) {
        GamepadPtr myGamepad = myGamepads[i];

        if (myGamepad && myGamepad->isConnected()) {
            // There are different ways to query whether a button is pressed.
            // By query each button individually:
            //  a(), b(), x(), y(), l1(), etc...

            // Rosetta code -----------------
            //Add gamepad values to struct
            array_gamepads[i].id = (byte)(i << 5);
            array_gamepads[i].id = (byte)(array_gamepads[i].id) | (1 << 4);       // isConnected()
            array_gamepads[i].id = (byte)(array_gamepads[i].id) | (3);            // controller enum

            array_gamepads[i].buttons1 = (byte)(myGamepad->buttons() >> 8);

            array_gamepads[i].buttons2 = (byte)(myGamepad->buttons() & 0xFF);

            array_gamepads[i].lx_axis = (short)(myGamepad->axisX());
            array_gamepads[i].ly_axis = (short)(myGamepad->axisY());
            array_gamepads[i].rx_axis = (short)(myGamepad->axisRX());
            array_gamepads[i].ry_axis = (short)(myGamepad->axisRY());

            array_gamepads[i].left_trigger = (short)(myGamepad->brake());
            array_gamepads[i].right_trigger = (short)(myGamepad->throttle());

            array_gamepads[i].secondary_buttons = (byte)(myGamepad->miscButtons());


            if (myGamepad->a()) {
                static int colorIdx = 0;
                // Some gamepads like DS4 and DualSense support changing the color LED.
                // It is possible to change it by calling:
                switch (colorIdx % 3) {
                    case 0:
                        // Red
                        myGamepad->setColorLED(255, 0, 0);
                        break;
                    case 1:
                        // Green
                        myGamepad->setColorLED(0, 255, 0);
                        break;
                    case 2:
                        // Blue
                        myGamepad->setColorLED(0, 0, 255);
                        break;
                }
                colorIdx++;
            }

            if (myGamepad->b()) {
                // Turn on the 4 LED. Each bit represents one LED.
                static int led = 0;
                led++;
                // Some gamepads like the DS3, DualSense, Nintendo Wii, Nintendo Switch
                // support changing the "Player LEDs": those 4 LEDs that usually indicate
                // the "gamepad seat".
                // It is possible to change them by calling:
                myGamepad->setPlayerLEDs(led & 0x0f);
            }

            if (myGamepad->x()) {
                // Duration: 255 is ~2 seconds
                // force: intensity
                // Some gamepads like DS3, DS4, DualSense, Switch, Xbox One S support
                // rumble.
                // It is possible to set it by calling:
                myGamepad->setRumble(0xc0 /* force */, 0xc0 /* duration */);
            }

            static int count = 0;
            if(count < 100){
                count++;
            } else {

                // Another way to query the buttons, is by calling buttons(), or
                // miscButtons() which return a bitmask.
                // Some gamepads also have DPAD, axis and more.
                Console.printf(
                    "idx=%d, dpad: 0x%02x, buttons: 0x%04x, axis L: %4d, %4d, axis R: %4d, "
                    "%4d, brake: %4d, throttle: %4d, misc: 0x%02x\n",
                    i,                        // Gamepad Index
                    myGamepad->dpad(),        // DPAD
                    myGamepad->buttons(),     // bitmask of pressed buttons
                    myGamepad->axisX(),       // (-511 - 512) left X Axis
                    myGamepad->axisY(),       // (-511 - 512) left Y axis
                    myGamepad->axisRX(),      // (-511 - 512) right X axis
                    myGamepad->axisRY(),      // (-511 - 512) right Y axis
                    myGamepad->brake(),       // (0 - 1023): brake button
                    myGamepad->throttle(),    // (0 - 1023): throttle (AKA gas) button
                    myGamepad->miscButtons()  // bitmak of pressed "misc" buttons
                );
                count = 0;
            }

            // You can query the axis and other properties as well. See Gamepad.h
            // For all the available functions.
        }
    }

    delay(2);
}
