# bluetooth-midi (Koala workshop fork)

A [Bluetooth Low Energy MIDI](https://www.midi.org/specifications/item/bluetooth-le-midi) extension for the micro:bit that works on **both micro:bit v1 and v2**.

## Why this fork exists

This fork is for testing BBC micro:bit v1 and v2 boards as custom MIDI controllers for the
[Koala Sampler](https://www.koalasampler.com/) app on iPad, in workshop settings run with
[Collusion](https://collusion.org.uk). Learners build their own controller in MakeCode, connect it to an iPad
over Bluetooth, and use it to play pads in Koala.

It is not an official Microsoft or micro:bit Educational Foundation extension.

## Credits

* [microsoft/pxt-bluetooth-midi](https://github.com/microsoft/pxt-bluetooth-midi): the original extension.
  It only builds for micro:bit v1, and iOS MIDI apps can't find the micro:bit because it doesn't advertise the MIDI service.
* [RBilsland/pxt-bluetooth-midi](https://github.com/RBilsland/pxt-bluetooth-midi): the **micro:bit v2**
  implementation is taken unchanged from this fork (commit `27b1a40`, MIT). If you only need v2, and want
  macOS/Windows notes, look there.

## What's changed from the Microsoft original

| | micro:bit v1 | micro:bit v2 |
|---|---|---|
| Builds | yes | yes (was a build failure) |
| MIDI service UUID advertised, so iOS MIDI apps can find it | added | from RBilsland |
| MIDI characteristic has *write without response* (required by the BLE MIDI spec) | added | from RBilsland |
| Bluetooth event service | disabled (see below) | disabled (see below) |
| `bluetooth midi advertising status` diagnostic block | added | added |

The event service is turned off in this extension's `pxt.json`. On v2, leaving it on uses up the
Bluetooth chip's room for custom UUIDs, and the MIDI characteristic ends up with the wrong UUID. The
iPad then connects but never receives notes.

## MakeCode setup

1. Open [makecode.microbit.org](https://makecode.microbit.org) and create a **new project**.
2. **Extensions** → paste `https://github.com/richhall/pxt-bluetooth-midi` → add **bluetooth-midi**.
   Accept the prompt to remove the **radio** extension. Bluetooth and radio can't be used together.
3. **Project Settings** (gear icon) → choose **No Pairing Required: Anyone can connect via Bluetooth**.
4. Add your code (example below), then **Download** and copy the `.hex` to the micro:bit.
   One `.hex` works on both v1 and v2.

### About the permissive settings

"No Pairing Required" means **any nearby device can connect to the micro:bit and send or receive
MIDI**, with no pairing or encryption. It's used here because iOS BLE MIDI apps can't complete the
micro:bit's pairing process, and it keeps workshop setup quick. Bear in mind:

* Each micro:bit shows up by its own name, e.g. `BBC micro:bit [zegap]`. In a room with many
  micro:bits, have learners find their board's name in the KORG app list (unplug a board and see which name disappears).
* Use it for workshops and play, not for anything where an unexpected connection would matter.

If MakeCode needs a specific version, pin it by tag or commit in **Project Settings → Edit settings as text**,
e.g. `"bluetooth-midi": "github:richhall/pxt-bluetooth-midi#v2.1.1"`. MakeCode caches GitHub versions, so
creating a new project is the most reliable way to pick up a new release.

## Example: a two-button pad controller

Koala triggers pads from MIDI **notes**. This example sends exactly what a Novation Launchpad sends:
a note-on at velocity 127 when a button is pressed, and a note-on at velocity 0 when it's released, on channel 1.

The same code is in [`examples/koala-pad-controller.ts`](examples/koala-pad-controller.ts). Paste it into
the **JavaScript** view of your MakeCode project, then switch to **Blocks** if you prefer.

```typescript
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
```

At startup the micro:bit scrolls the advertising status: **1** means the MIDI service is set up and
advertising. It shows **C** when an iPad connects and **D** when it disconnects.

The standard [MIDI extension](https://github.com/microsoft/pxt-midi) blocks (`midi play tone`, `note on`,
`control change`, …) also work. Koala only triggers pads from notes, not CC messages.

## Connecting to Koala on iPad

**Connect with the KORG BLE-MIDI app first, then open Koala.** Koala does not reliably connect to
Bluetooth MIDI devices by itself.

1. Install KORG's free **BLE-MIDI** app from the App Store. GarageBand's
   *Settings → Advanced → Bluetooth MIDI Devices* also works.
2. Power the micro:bit. Don't pair it in iPad **Settings → Bluetooth**: BLE MIDI devices never appear there.
3. In **KORG BLE-MIDI**, tap the micro:bit to connect. The micro:bit shows **C**.
4. Now open **Koala** → Settings → MIDI and make sure the micro:bit input is switched on.
   Watch the MIDI monitor line at the bottom of the settings: it should change from "no MIDI notes so far" when you press a button.
5. Press **A** / **B**.

Once connected this way, iOS remembers the micro:bit until you choose *Forget*. If you forget it,
or Koala stops receiving, repeat the KORG step. Disconnecting and reconnecting from inside Koala
doesn't work reliably, so reconnect in KORG instead.

### Koala MIDI settings that worked in testing

| Setting | Value |
|---|---|
| Use MIDI mapping | **On** to assign buttons with *Map MIDI*, or **Off** to use the note offset |
| MIDI channel | 1 or ALL |
| MIDI Note Offset (first pad note) | C3 = note 60, so button A is pad 1 when mapping is off |
| Keyboard Mode MIDI channel | **not** ALL or 1, e.g. 16. If it's ALL, notes change the pitch of the selected sample instead of triggering pads |
| MIDI can select pads / Enable MIDI velocity | either |

## Tested with

* micro:bit v1 (board ID 9900) and micro:bit v2.2 (board ID 9906), same `.hex`
* iPad, Koala Sampler, KORG BLE-MIDI, GarageBand (September 2026)

## Supported targets

* for PXT/microbit
* for PXT/calliope

(The metadata above is needed for package search.)

## License

MIT
