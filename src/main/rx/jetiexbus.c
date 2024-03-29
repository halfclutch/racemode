/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 * Thomas Miric - marv-t
 *
 * Jeti EX Bus Communication Protocol:
 * http://www.jetimodel.com/en/show-file/642/
 *
 * JETI Telemetry Communication Protocol:
 * http://www.jetimodel.com/en/show-file/26/
 *
 * Following restrictions:
 * Communication speed: 125000 bps
 * Number of channels: 16
 *
 * Connect as follows:
 * Jeti EX Bus -> Serial RX (connect directly)
 * Serial TX -> Resistor(2k4) ->Serial RX
 * In jeti pdf it is different, but if the resistor breaks, the receiver continues to operate.
 *
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#ifdef USE_SERIAL_RX

#include "build/build_config.h"
#include "build/debug.h"

#include "common/utils.h"

#include "drivers/time.h"

#include "io/serial.h"

#include "rx/rx.h"
#include "rx/jetiexbus.h"


//
// Serial driver for Jeti EX Bus receiver
//
#define JETIEXBUS_BAUDRATE 125000                       // EX Bus 125000; EX Bus HS 250000 not supported
#define JETIEXBUS_OPTIONS (SERIAL_STOPBITS_1 | SERIAL_PARITY_NO)
#define JETIEXBUS_MIN_FRAME_GAP     1000
#define JETIEXBUS_CHANNEL_COUNT     16                  // most Jeti TX transmit 16 channels


#define EXBUS_START_CHANNEL_FRAME       (0x3E)
#define EXBUS_START_REQUEST_FRAME       (0x3D)
#define EXBUS_JETIBOX_REQUEST           (0x3B)

#define EXBUS_CHANNELDATA               (0x3E03)        // Frame contains Channel Data
#define EXBUS_CHANNELDATA_DATA_REQUEST  (0x3E01)        // Frame contains Channel Data, but with a request for data
#define EXBUS_TELEMETRY_REQUEST         (0x3D01)        // Frame is a request Frame



serialPort_t *jetiExBusPort;

uint32_t jetiTimeStampRequest = 0;

static uint8_t jetiExBusFramePosition;
static uint8_t jetiExBusFrameLength;

static uint8_t jetiExBusFrameState = EXBUS_STATE_ZERO;
uint8_t jetiExBusRequestState = EXBUS_STATE_ZERO;

// Use max values for ram areas
static uint8_t jetiExBusChannelFrame[EXBUS_MAX_CHANNEL_FRAME_SIZE];
uint8_t jetiExBusRequestFrame[EXBUS_MAX_REQUEST_FRAME_SIZE];

static uint16_t jetiExBusChannelData[JETIEXBUS_CHANNEL_COUNT];


// Jeti Ex Bus CRC calculations for a frame
uint16_t jetiExBusCalcCRC16(uint8_t *pt, uint8_t msgLen)
{
    uint16_t crc16_data = 0;
    uint8_t data=0;

    for (uint8_t mlen = 0; mlen < msgLen; mlen++) {
        data = pt[mlen] ^ ((uint8_t)(crc16_data) & (uint8_t)(0xFF));
        data ^= data << 4;
        crc16_data = ((((uint16_t)data << 8) | ((crc16_data & 0xFF00) >> 8))
                      ^ (uint8_t)(data >> 4)
                      ^ ((uint16_t)data << 3));
    }
    return(crc16_data);
}

void jetiExBusDecodeChannelFrame(uint8_t *exBusFrame)
{
    uint16_t value;
    uint8_t frameAddr;

    // Decode header
    switch (((uint16_t)exBusFrame[EXBUS_HEADER_SYNC] << 8) | ((uint16_t)exBusFrame[EXBUS_HEADER_REQ])) {

    case EXBUS_CHANNELDATA_DATA_REQUEST:                   // not yet specified
    case EXBUS_CHANNELDATA:
        for (uint8_t i = 0; i < JETIEXBUS_CHANNEL_COUNT; i++) {
            frameAddr = EXBUS_HEADER_LEN + i * 2;
            value = ((uint16_t)exBusFrame[frameAddr + 1]) << 8;
            value += (uint16_t)exBusFrame[frameAddr];
            // Convert to internal format
            jetiExBusChannelData[i] = value >> 3;
        }
        break;
    }
}

void jetiExBusFrameReset(void)
{
    jetiExBusFramePosition = 0;
    jetiExBusFrameLength = EXBUS_MAX_CHANNEL_FRAME_SIZE;
}

/*
  supported:
  0x3E 0x01 LEN Packet_ID 0x31 SUB_LEN Data_array CRC16      // Channel Data with telemetry request (2nd byte 0x01)
  0x3E 0x03 LEN Packet_ID 0x31 SUB_LEN Data_array CRC16      // Channel Data forbids answering (2nd byte 0x03)
  0x3D 0x01 0x08 Packet_ID 0x3A 0x00 CRC16                   // Telemetry Request EX telemetry (5th byte 0x3A)

  other messages - not supported:
  0x3D 0x01 0x09 Packet_ID 0x3B 0x01 0xF0 CRC16              // Jetibox request (5th byte 0x3B)
  ...
*/

// Receive ISR callback
static void jetiExBusDataReceive(uint16_t c, void *data)
{
    UNUSED(data);

    uint32_t now;
    static uint32_t jetiExBusTimeLast = 0;
    static int32_t jetiExBusTimeInterval;

    static uint8_t *jetiExBusFrame;

    // Check if we shall reset frame position due to time
    now = micros();

    jetiExBusTimeInterval = now - jetiExBusTimeLast;
    jetiExBusTimeLast = now;

    if (jetiExBusTimeInterval > JETIEXBUS_MIN_FRAME_GAP) {
        jetiExBusFrameReset();
        jetiExBusFrameState = EXBUS_STATE_ZERO;
        jetiExBusRequestState = EXBUS_STATE_ZERO;
    }

    // Check if we shall start a frame?
    if (jetiExBusFramePosition == 0) {
        switch (c) {
        case EXBUS_START_CHANNEL_FRAME:
            jetiExBusFrameState = EXBUS_STATE_IN_PROGRESS;
            jetiExBusFrame = jetiExBusChannelFrame;
            break;

        case EXBUS_START_REQUEST_FRAME:
            jetiExBusRequestState = EXBUS_STATE_IN_PROGRESS;
            jetiExBusFrame = jetiExBusRequestFrame;
            break;

        default:
            return;
        }
    }

    // Store in frame copy
    jetiExBusFrame[jetiExBusFramePosition] = (uint8_t)c;
    jetiExBusFramePosition++;

    // Check the header for the message length
    if (jetiExBusFramePosition == EXBUS_HEADER_LEN) {

        if ((jetiExBusFrameState == EXBUS_STATE_IN_PROGRESS) && (jetiExBusFrame[EXBUS_HEADER_MSG_LEN] <= EXBUS_MAX_CHANNEL_FRAME_SIZE)) {
            jetiExBusFrameLength = jetiExBusFrame[EXBUS_HEADER_MSG_LEN];
            return;
        }

        if ((jetiExBusRequestState == EXBUS_STATE_IN_PROGRESS) && (jetiExBusFrame[EXBUS_HEADER_MSG_LEN] <= EXBUS_MAX_REQUEST_FRAME_SIZE)) {
            jetiExBusFrameLength = jetiExBusFrame[EXBUS_HEADER_MSG_LEN];
            return;
        }

        jetiExBusFrameReset();                  // not a valid frame
        jetiExBusFrameState = EXBUS_STATE_ZERO;
        jetiExBusRequestState = EXBUS_STATE_ZERO;
        return;
    }

    // Done?
    if (jetiExBusFrameLength == jetiExBusFramePosition) {
        if (jetiExBusFrameState == EXBUS_STATE_IN_PROGRESS)
            jetiExBusFrameState = EXBUS_STATE_RECEIVED;
        if (jetiExBusRequestState == EXBUS_STATE_IN_PROGRESS) {
            jetiExBusRequestState = EXBUS_STATE_RECEIVED;
            jetiTimeStampRequest = micros();
        }

        jetiExBusFrameReset();
    }
}

// Check if it is time to read a frame from the data...
static uint8_t jetiExBusFrameStatus(rxRuntimeConfig_t *rxRuntimeConfig)
{
    UNUSED(rxRuntimeConfig);

    if (jetiExBusFrameState != EXBUS_STATE_RECEIVED)
        return RX_FRAME_PENDING;

    if (jetiExBusCalcCRC16(jetiExBusChannelFrame, jetiExBusChannelFrame[EXBUS_HEADER_MSG_LEN]) == 0) {
        jetiExBusDecodeChannelFrame(jetiExBusChannelFrame);
        jetiExBusFrameState = EXBUS_STATE_ZERO;
        return RX_FRAME_COMPLETE;
    } else {
        jetiExBusFrameState = EXBUS_STATE_ZERO;
        return RX_FRAME_PENDING;
    }
}

static uint16_t jetiExBusReadRawRC(const rxRuntimeConfig_t *rxRuntimeConfig, uint8_t chan)
{
    if (chan >= rxRuntimeConfig->channelCount)
        return 0;

    return (jetiExBusChannelData[chan]);
}

<<<<<<< HEAD

#ifdef TELEMETRY
/*
  -----------------------------------------------
   Jeti Ex Bus Telemetry
  -----------------------------------------------
*/

void initJetiExBusTelemetry(void)
{
    // Init Ex Bus Frame header
    jetiExBusTelemetryFrame[EXBUS_HEADER_SYNC] = 0x3B;       // Startbytes
    jetiExBusTelemetryFrame[EXBUS_HEADER_REQ] = 0x01;
    jetiExBusTelemetryFrame[EXBUS_HEADER_DATA_ID] = 0x3A;    // Ex Telemetry


    // Init Ex Telemetry header
    uint8_t *jetiExTelemetryFrame = &jetiExBusTelemetryFrame[EXBUS_HEADER_DATA];
    jetiExTelemetryFrame[EXTEL_HEADER_SYNC] = 0x9F;              // Startbyte
    jetiExTelemetryFrame[EXTEL_HEADER_USN_LB] = 0x1E;            // Serial Number 4 Byte
    jetiExTelemetryFrame[EXTEL_HEADER_USN_HB] = 0xA4;
    jetiExTelemetryFrame[EXTEL_HEADER_LSN_LB] = 0x00;            // increment by telemetry count (%16) > only 15 values per device possible
    jetiExTelemetryFrame[EXTEL_HEADER_LSN_HB] = 0x00;
    jetiExTelemetryFrame[EXTEL_HEADER_RES] = 0x00;               // reserved, by default 0x00
}

void createExTelemetrieTextMessage(uint8_t *exMessage, uint8_t messageID, const exBusSensor_t *sensor)
{
    uint8_t labelLength = strlen(sensor->label);
    uint8_t unitLength = strlen(sensor->unit);

    exMessage[EXTEL_HEADER_TYPE_LEN] = EXTEL_OVERHEAD + labelLength + unitLength;
    exMessage[EXTEL_HEADER_LSN_LB] = messageID & 0xF0;                              // Device ID
    exMessage[EXTEL_HEADER_ID] = messageID & 0x0F;                                  // Sensor ID (%16)
    exMessage[EXTEL_HEADER_DATA] = (labelLength << 3) + unitLength;

    memcpy(&exMessage[EXTEL_HEADER_DATA + 1], sensor->label, labelLength);
    memcpy(&exMessage[EXTEL_HEADER_DATA + 1 + labelLength], sensor->unit, unitLength);

    exMessage[exMessage[EXTEL_HEADER_TYPE_LEN] + EXTEL_CRC_LEN] = calcCRC8(&exMessage[EXTEL_HEADER_TYPE_LEN], exMessage[EXTEL_HEADER_TYPE_LEN]);
}


uint8_t createExTelemetrieValueMessage(uint8_t *exMessage, uint8_t itemStart)
{
    uint8_t item = itemStart;
    uint8_t iCount;
    uint8_t messageSize;
    uint32_t sensorValue;

    if ((item & 0x0F) == 0)
        item++;

    if (item >= JETI_EX_SENSOR_COUNT)
        item = 1;

    exMessage[EXTEL_HEADER_LSN_LB] = item & 0xF0;                                   // Device ID
    uint8_t *p = &exMessage[EXTEL_HEADER_ID];

    while (item <= (itemStart | 0x0F)) {
        *p++ = ((item & 0x0F) << 4) | jetiExSensors[item].exDataType;               // Sensor ID (%16) | EX Data Type

        sensorValue = jetiExSensors[item].value;
        iCount = exDataTypeLen[jetiExSensors[item].exDataType];
        while (iCount > 1) {
            *p++ = sensorValue;
            sensorValue = sensorValue >> 8;
            iCount--;
        }
        *p++ = (sensorValue & 0x9F) | jetiExSensors[item].decimals;

        item++;
        if (item > JETI_EX_SENSOR_COUNT)
            break;
        if (EXTEL_MAX_PAYLOAD <= ((p-&exMessage[EXTEL_HEADER_ID]) + exDataTypeLen[jetiExSensors[item].exDataType]) + 1)
            break;
    }

    messageSize = (EXTEL_HEADER_LEN + (p-&exMessage[EXTEL_HEADER_ID]));
    exMessage[EXTEL_HEADER_TYPE_LEN] = EXTEL_DATA_MSG | messageSize;
    exMessage[messageSize + EXTEL_CRC_LEN] = calcCRC8(&exMessage[EXTEL_HEADER_TYPE_LEN], messageSize);

    return item;        // return the next item
}


void createExBusMessage(uint8_t *exBusMessage, uint8_t *exMessage, uint8_t packetID)
{
    uint16_t crc16;

    exBusMessage[EXBUS_HEADER_PACKET_ID] = packetID;
    exBusMessage[EXBUS_HEADER_SUBLEN] = (exMessage[EXTEL_HEADER_TYPE_LEN] & EXTEL_UNMASK_TYPE) + 2;    // +2: startbyte & CRC8
    exBusMessage[EXBUS_HEADER_MSG_LEN] = EXBUS_OVERHEAD + exBusMessage[EXBUS_HEADER_SUBLEN];

    crc16 = calcCRC16(exBusMessage, exBusMessage[EXBUS_HEADER_MSG_LEN] - EXBUS_CRC_LEN);
    exBusMessage[exBusMessage[EXBUS_HEADER_MSG_LEN] - 2] = crc16;
    exBusMessage[exBusMessage[EXBUS_HEADER_MSG_LEN] - 1] = crc16 >> 8;
}


void checkJetiExBusTelemetryState(void)
{
    return;
}


void handleJetiExBusTelemetry(void)
{
    static uint16_t framesLost = 0; // only for debug
    uint32_t timeDiff;
    // Check if we shall reset frame position due to time

    if (jetiExBusRequestState == EXBUS_STATE_RECEIVED) {

        // to prevent timing issues from request to answer - max. 4ms
        timeDiff = micros() - jetiTimeStampRequest;

        if (timeDiff > 3000) {   // include reserved time
            jetiExBusRequestState = EXBUS_STATE_ZERO;
            framesLost++;
            return;
        }

        if ((jetiExBusRequestFrame[EXBUS_HEADER_DATA_ID] == EXBUS_EX_REQUEST) && (calcCRC16(jetiExBusRequestFrame, jetiExBusRequestFrame[EXBUS_HEADER_MSG_LEN]) == 0)) {
            jetiExSensors[EX_VOLTAGE].value = getBatteryVoltage();
            jetiExSensors[EX_CURRENT].value = getAmperage();
            jetiExSensors[EX_ALTITUDE].value = getEstimatedAltitude();
            jetiExSensors[EX_CAPACITY].value = getMAhDrawn();
            jetiExSensors[EX_FRAMES_LOST].value = framesLost;
            jetiExSensors[EX_TIME_DIFF].value = timeDiff;

            // switch to TX mode
            if (serialRxBytesWaiting(jetiExBusPort) == 0) {
                serialSetMode(jetiExBusPort, MODE_TX);
                jetiExBusTransceiveState = EXBUS_TRANS_TX;
                sendJetiExBusTelemetry(jetiExBusRequestFrame[EXBUS_HEADER_PACKET_ID]);
                jetiExBusRequestState = EXBUS_STATE_PROCESSED;
            }
        } else {
            jetiExBusRequestState = EXBUS_STATE_ZERO;
            return;
        }
    }

    // check the state if transmit is ready
    if (jetiExBusTransceiveState == EXBUS_TRANS_IS_TX_COMPLETED) {
        if (isSerialTransmitBufferEmpty(jetiExBusPort)) {
            serialSetMode(jetiExBusPort, MODE_RX);
            jetiExBusTransceiveState = EXBUS_TRANS_RX;
            jetiExBusRequestState = EXBUS_STATE_ZERO;
        }
    }
}


void sendJetiExBusTelemetry(uint8_t packetID)
{
    static uint8_t sensorDescriptionCounter = 0;
    static uint8_t sensorValueCounter = 1;
    static uint8_t requestLoop = 0;
    uint8_t *jetiExTelemetryFrame = &jetiExBusTelemetryFrame[EXBUS_HEADER_DATA];

    if (requestLoop == 100) {              //every nth request send the name of a value
        if (sensorDescriptionCounter == JETI_EX_SENSOR_COUNT )
            sensorDescriptionCounter = 0;

        createExTelemetrieTextMessage(jetiExTelemetryFrame, sensorDescriptionCounter, &jetiExSensors[sensorDescriptionCounter]);
        createExBusMessage(jetiExBusTelemetryFrame, jetiExTelemetryFrame, packetID);

        requestLoop = 0;
        sensorDescriptionCounter++;
    } else {
        sensorValueCounter = createExTelemetrieValueMessage(jetiExTelemetryFrame, sensorValueCounter);
        createExBusMessage(jetiExBusTelemetryFrame, jetiExTelemetryFrame, packetID);
    }

    for (uint8_t iCount = 0; iCount < jetiExBusTelemetryFrame[EXBUS_HEADER_MSG_LEN]; iCount++) {
        serialWrite(jetiExBusPort, jetiExBusTelemetryFrame[iCount]);
    }
    jetiExBusTransceiveState = EXBUS_TRANS_IS_TX_COMPLETED;
    requestLoop++;
}
#endif // TELEMETRY

=======
>>>>>>> test
bool jetiExBusInit(const rxConfig_t *rxConfig, rxRuntimeConfig_t *rxRuntimeConfig)
{
    UNUSED(rxConfig);

    rxRuntimeConfig->channelCount = JETIEXBUS_CHANNEL_COUNT;
    rxRuntimeConfig->rxRefreshRate = 5500;

    rxRuntimeConfig->rcReadRawFn = jetiExBusReadRawRC;
    rxRuntimeConfig->rcFrameStatusFn = jetiExBusFrameStatus;

    jetiExBusFrameReset();

    const serialPortConfig_t *portConfig = findSerialPortConfig(FUNCTION_RX_SERIAL);

    if (!portConfig) {
        return false;
    }

    jetiExBusPort = openSerialPort(portConfig->identifier,
        FUNCTION_RX_SERIAL,
        jetiExBusDataReceive,
        NULL,
        JETIEXBUS_BAUDRATE,
        MODE_RXTX,
        JETIEXBUS_OPTIONS | (rxConfig->serialrx_inverted ? SERIAL_INVERTED : 0) | (rxConfig->halfDuplex ? SERIAL_BIDIR : 0)
        );
    serialSetMode(jetiExBusPort, MODE_RX);
    return jetiExBusPort != NULL;
}
#endif // SERIAL_RX
