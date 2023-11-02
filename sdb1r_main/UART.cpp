/* Author : XentiQ
Date created - 2022.12.14 - XentiQ version
*/

#include "UART.h"
#include "SaveProfile.h"
#include "App_Common.h"

void flushSerialBuffer(Stream &serial)
{
    while (serial.available())
    {
        serial.read();
    }
}

void sendCommand(Command cmd)
{
    // byte buffer[BUFFER_SIZE] = {Serial_SOT, cmd, 0x00, 0x00, Serial_EOT};
    // Serial1.write(buffer, BUFFER_SIZE);
#ifdef SERIAL_2_DEFAULT
    Serial2.write(Serial_SOT);
    delay(50);
    Serial2.write(cmd);
    Serial2.write(0x00);
    Serial2.write(0x00);
    delay(50);
    Serial2.write(Serial_EOT);
#endif

#ifndef SERIAL_2_DEFAULT
    Serial3.write(Serial_SOT);
    delay(50);
    Serial3.write(cmd);
    Serial3.write(0x00);
    Serial3.write(0x00);
    delay(50);
    Serial3.write(Serial_EOT);
#endif

    // Serial Debugging
    Serial.print("Send Command:     0x");
    Serial.print(Serial_SOT, HEX);
    Serial.print(" 0x");
    Serial.print(cmd, HEX);
    Serial.print(" 0x0");
    Serial.print(0x0, HEX);
    Serial.print(" 0x0");
    Serial.print(0x0, HEX);
    Serial.print(" 0x");
    Serial.println(Serial_EOT, HEX);
}

Response receiveResponse()
{
    Response resp;
    resp.command = 0xFF;
    resp.data1 = 0x00;
    resp.data2 = 0x00;
    resp.sot = 0x00;

#ifdef SERIAL_2_DEFAULT
    if (Serial2.available() >= 5)
    {
        resp.sot = Serial2.read();

        if (resp.sot == Serial_SOT)
        {
            resp.command = Serial2.read();
            resp.data1 = Serial2.read();
            resp.data2 = Serial2.read();
            resp.eot = Serial2.read();

            if (resp.eot != Serial_EOT)
            {
                resp.command = 0xFF;
                resp.data1 = 0x00;
                resp.data2 = 0x00;
            }
        }
    }
#endif

#ifndef SERIAL_2_DEFAULT
    if (Serial3.available() >= 5)
    {
        resp.sot = Serial3.read();

        if (resp.sot == Serial_SOT)
        {
            resp.command = Serial3.read();
            resp.data1 = Serial3.read();
            resp.data2 = Serial3.read();
            resp.eot = Serial3.read();

            if (resp.eot != Serial_EOT)
            {
                resp.command = 0xFF;
                resp.data1 = 0x00;
                resp.data2 = 0x00;
            }
        }
    }
#endif

    return resp;
}

Response handle_uart_command(Command cmd)
{
#ifdef SERIAL_2_DEFAULT  
    flushSerialBuffer(Serial2);
#endif

#ifndef SERIAL_2_DEFAULT  
    flushSerialBuffer(Serial3);
#endif
    // Send the command
    // sendCommand(cmd);
    // delay(100); // Add a short delay
    // sendCommand(SDB_Dispense_STOP);
    Response response = receiveResponse();

    if (response.command == 0x60 && response.data1 == 0x3C)
    {
        Serial.print("Received Command: 0x");
        Serial.print(Serial_SOT, HEX);
        Serial.print(" 0x");
        Serial.print(response.command, HEX);
        Serial.print(" 0x");
        Serial.print(response.data1, HEX);
        Serial.print(" 0x0");
        Serial.print(response.data2, HEX);
        Serial.print(" 0x");
        Serial.println(Serial_EOT, HEX);
        Serial.println("Command successful!");
    }
// Run_profile();

    return response;
}

uint8_t get_expected_response(Command cmd)
{
    switch (cmd)
    {
    case Handshake:
        return 0x60;
    case Vibrate_Mode_ON:
        return 0x1C;
    case Vibrate_Mode_OFF:
        return 0x2C;
    case SDB_Dispense_START:
        Run_profile();
        return 0xFF; // Update with expected response when implemented
    case SDB_Dispense_PAUSE:
        return 0xFF; // Update with expected response when implemented
    case SDB_Dispense_STOP:
        return 0x3C;
    case Error_IR_Censor_Failure:
        return 0xE1;
    case Error_Marker_Not_Detected:
        return 0xE2;
    default:
        return 0xFF;
    }
}
