/* Copyright (c) 2014 mbed.org, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __BLEMIDI_H__
#define __BLEMIDI_H__

#include "MicroBitConfig.h"
#include "pxt.h"

// Max BLE MIDI packet size at default ATT MTU (spec allows up to negotiated MTU).
#define BLE_MIDI_CHAR_MAX_LEN 20

//================================================================
#if MICROBIT_CODAL
//================================================================

#include "MicroBitBLEManager.h"
#include "MicroBitBLEService.h"

/**
 * A class to communicate with a BLE MIDI device (micro:bit v2 / CODAL).
 */
class BluetoothMIDIService : public MicroBitBLEService
{
public:
    BluetoothMIDIService(BLEDevice &_ble);

    bool connected();

    void sendMidiMessage(uint8_t data0);
    void sendMidiMessage(uint8_t data0, uint8_t data1);
    void sendMidiMessage(uint8_t data0, uint8_t data1, uint8_t data2);

    void runHandshakeRetries();

private:
    void onConnect(const microbit_ble_evt_t *p_ble_evt) override;
    void onDataRead(microbit_onDataRead_t *params) override;
    void onDataWritten(const microbit_ble_evt_write_t *params) override;
    void onDisconnect(const microbit_ble_evt_t *p_ble_evt) override;
    void configureMidiAdvertising(uint8_t serviceUuidType);
    void completeMidiHandshake();
    void scheduleMidiHandshake();

    uint8_t midiBuffer[BLE_MIDI_CHAR_MAX_LEN];
    bool pendingHandshake;
    uint8_t midiAdvUuidType;

    typedef enum mbbs_cIdx
    {
        mbbs_cIdxMIDI,
        mbbs_cIdxCOUNT
    } mbbs_cIdx;

    static const uint8_t service_base_uuid[16];
    static const uint8_t char_base_uuid[16];
    static const uint16_t serviceUUID;
    static const uint16_t charUUID[mbbs_cIdxCOUNT];

    MicroBitBLEChar chars[mbbs_cIdxCOUNT];

public:
    int characteristicCount() { return mbbs_cIdxCOUNT; }
    MicroBitBLEChar *characteristicPtr(int idx) { return &chars[idx]; }
};

//================================================================
#else // MICROBIT_CODAL
//================================================================

#include "ble/BLE.h"

/** 
 * A class to communicate a BLE MIDI device
 */
class BluetoothMIDIService {
public:
    /**
     * Constructor
     */
    BluetoothMIDIService(BLEDevice *device);

    /**
     * Check if a BLE MIDI device is connected
     *
     * @returns true if a midi device is connected
     */
    bool connected();

    void sendMidiMessage(uint8_t data0);
    void sendMidiMessage(uint8_t data0, uint8_t data1);
    void sendMidiMessage(uint8_t data0, uint8_t data1, uint8_t data2);

    int advertisingStatus;

private:    
    void onDataRead(const GattReadCallbackParams* params);
    void onDisconnection(const Gap::DisconnectionCallbackParams_t* params);

    uint16_t timestamp;
    uint8_t midiBuffer[20]; // max BLE packet payload, so writes from the central fit
    bool firstRead;

    BLEDevice &ble;
    GattAttribute::Handle_t midiCharacteristicHandle;    
    Timer tick;
};

//================================================================
#endif // MICROBIT_CODAL
//================================================================

#endif /* __BLEMIDI_H__ */
