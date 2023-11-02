/* Author : XentiQ
Date created - 2022.12.14 - XentiQ version
*/

#ifndef _UART_H_
#define _UART_H_

#include <Arduino.h>

#define Serial_SOT 0xA5
#define Serial_EOT 0x5A
#define BUFFER_SIZE 5

#define ACKNOWLEDGE 0x60
#define VIBRATION_ON 0x1C
#define VIBRATION_OFF 0x2C
#define DISPENSE_DONE 0x3C
#define IR_SENSOR_FAILURE 0xE1
#define MARKER_NOT_DETECTED 0xE2

#define SERIAL_2_DEFAULT  //Comment this out for Serial 3 usage

typedef struct Response
{
  uint8_t sot;
  uint8_t command;
  uint8_t data1;
  uint8_t data2;
  uint8_t eot;
} Response;

enum CommandState
{
  IDLE,
  HANDSHAKE,
  START_BUTTON,
  PAUSE_BUTTON,
  STOP_BUTTON,
  ERROR1,
  ERROR2,
  HOMING
};

enum Command
{
  Handshake = 0x06,
  Vibrate_Mode_ON = 0xC1,
  Vibrate_Mode_OFF = 0xC2,
  SDB_Dispense_START = 0xC3,
  SDB_Dispense_PAUSE = 0xC4,
  SDB_Dispense_STOP = 0xC5
};

enum ResponseType
{
  Acknowledge = 0x60,
  Vibration_ON = 0x1C,
  Vibration_OFF = 0x2C,
  Dispense_DONE = 0x3C,
  Error_IR_Censor_Failure = 0xE1,
  Error_Marker_Not_Detected = 0xE2
};

uint8_t get_expected_response(Command cmd);

void flushSerialBuffer(Stream &serial);
void sendCommand(Command cmd);

Response receiveResponse();
Response handle_uart_command(Command cmd);

#endif /*_UART_H_*/
