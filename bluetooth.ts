namespace bluetooth {
    /**
     * Starts the MIDI service over Bluetooth and registers it as the MIDI transport.
     */
    //% blockId=bluetooth_start_midi block="bluetooth start midi service"
    //% part=bluetooth
    //% blockHidden=1 deprecated=true
    export function startMidiService() {
        function send(buffer: Buffer) {
            bluetooth.midiSendMessage(buffer);
        }
        midi.setTransport(send);
        bluetooth.midiSendMessage(pins.createBuffer(0)); // does nothing but starts service lazily
    }

    /**
     * Diagnostic: 1 if the MIDI service UUID was added to the advert, 0 if not attempted,
     * otherwise step * 100 + BLE error code.
     */
    //% blockId=bluetooth_midi_advertising_status block="bluetooth midi advertising status"
    //% advanced=true shim=bluetooth::midiAdvertisingStatus
    export function midiAdvertisingStatus(): number {
        // simulator only; the micro:bit runs the C++ version
        return 1;
    }

    /**
     * Sends a MIDI message
     */
    //% shim=bluetooth::midiSendMessage
    //% advanced=true
    export function midiSendMessage(data: Buffer) {
        return;
    }

}
// automatically start midi service
bluetooth.startMidiService();
