// Two-button Koala pad controller for micro:bit v1 and v2.
// Paste into the JavaScript view of a MakeCode project that has this extension added
// (see README: MakeCode setup). Not part of the extension build - pxt.json doesn't list it.
//
// Sends what a Novation Launchpad sends, on MIDI channel 1:
//   press   -> note-on, velocity 127
//   release -> note-on, velocity 0
// A = note 60, B = note 67. Change the note numbers to target other pads.

bluetooth.onBluetoothConnected(function () {
    basic.showString("C")
})
bluetooth.onBluetoothDisconnected(function () {
    basic.showString("D")
})
control.onEvent(EventBusSource.MICROBIT_ID_BUTTON_A, EventBusValue.MICROBIT_BUTTON_EVT_DOWN, function () {
    midi.sendMessage([0x90, 60, 127])
})
control.onEvent(EventBusSource.MICROBIT_ID_BUTTON_A, EventBusValue.MICROBIT_BUTTON_EVT_UP, function () {
    midi.sendMessage([0x90, 60, 0])
})
control.onEvent(EventBusSource.MICROBIT_ID_BUTTON_B, EventBusValue.MICROBIT_BUTTON_EVT_DOWN, function () {
    midi.sendMessage([0x90, 67, 127])
})
control.onEvent(EventBusSource.MICROBIT_ID_BUTTON_B, EventBusValue.MICROBIT_BUTTON_EVT_UP, function () {
    midi.sendMessage([0x90, 67, 0])
})
basic.showNumber(bluetooth.midiAdvertisingStatus())
