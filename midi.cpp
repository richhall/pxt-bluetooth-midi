#include "pxt.h"
#include "BluetoothMIDIService.h"
using namespace pxt;

// v0 backward compat support
#ifndef PXT_BUFFER_DATA
#define PXT_BUFFER_DATA(buffer) buffer->payload
#endif
/**
* A set of functions to send MIDI commands over Bluetooth
*/
namespace bluetooth {
    BluetoothMIDIService* pMidi = NULL;
    BluetoothMIDIService* getMidi()
    {
        if (NULL == pMidi)
#if MICROBIT_CODAL
            pMidi = new BluetoothMIDIService(*uBit.ble);
#else
            pMidi = new BluetoothMIDIService(uBit.ble);
#endif
        return pMidi;
    }

    /**
     * Diagnostic: 1 if the MIDI service UUID was added to the advert, 0 if not attempted,
     * otherwise step * 100 + BLE error code.
     */
    //% blockId=bluetooth_midi_advertising_status block="bluetooth midi advertising status"
    //% advanced=true
    int midiAdvertisingStatus() {
#if MICROBIT_CODAL
        getMidi();
        return 1; // v2 advertising setup halts with a BLE error code on failure, so reaching here means OK
#else
        return getMidi()->advertisingStatus;
#endif
    }

    //%
    void midiSendMessage(Buffer data) {
        BluetoothMIDIService* pMidi = getMidi();            
        auto buf = PXT_BUFFER_DATA(data);
        
        switch(data->length) {
            case 1: 
                pMidi->sendMidiMessage(buf[0]);
                break;
            case 2:
                pMidi->sendMidiMessage(buf[0], buf[1]);
                break;
            case 3:
                pMidi->sendMidiMessage(buf[0], buf[1], buf[2]);
                break;
        }
    }
}
