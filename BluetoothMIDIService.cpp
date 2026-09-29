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

#include "MicroBitConfig.h"
#include "BluetoothMIDIService.h"

//================================================================
#if MICROBIT_CODAL
//================================================================
// micro:bit v2 implementation taken unchanged from RBilsland/pxt-bluetooth-midi @ 27b1a40 (MIT).

#include "pxt.h"
#include "MicroBitBLEManager.h"
#include "ble_advdata.h"
#include "ble_gap.h"
#include "ble_gatt.h"
#include "CodalFiber.h"

using namespace codal;

static void midiHandshakeFiber(void *param)
{
    ((BluetoothMIDIService *)param)->runHandshakeRetries();
}

// Nordic BLE-MIDI guide: device name in advertising, 128-bit MIDI UUID in scan response.
static const uint8_t midi_scanrsp_uuid[] = {
    0x11, 0x07,
    0x00, 0xc7, 0xc4, 0x4e, 0xe3, 0x6c, 0x51, 0xa7,
    0x33, 0x4b, 0xe8, 0xed, 0x5a, 0x0e, 0xb8, 0x03,
};

// BLE-MIDI empty packet: header + timestamp with zero time bits (nRF cannot HVX length 0).
static const uint8_t midi_handshake_pkt[] = { 0x80, 0x80 };
static const uint16_t midi_handshake_len = sizeof(midi_handshake_pkt);

void BluetoothMIDIService::configureMidiAdvertising(uint8_t serviceUuidType)
{
    (void)serviceUuidType;

    uint8_t adv_handle = 0;
    // static: the SoftDevice keeps this pointer while advertising, so it must outlive the call
    static uint8_t enc_adv[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
    uint16_t adv_len = sizeof(enc_adv);

    // Built by hand rather than with ble_advdata_encode: on a v2 the encoder's name was
    // missing from the advert seen by Windows (2026-09-29), so Windows/MIDIberry hid the
    // device. Layout: flags (3 bytes) + local name (complete, or shortened if > 26 chars).
    enc_adv[0] = 0x02;
    enc_adv[1] = 0x01;
    enc_adv[2] = BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED | BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE;
    adv_len = 3;

    uint8_t name[BLE_GAP_DEVNAME_DEFAULT_LEN + 1];
    uint16_t name_len = sizeof(name);
    if (sd_ble_gap_device_name_get(name, &name_len) == NRF_SUCCESS && name_len > 0) {
        uint16_t room = BLE_GAP_ADV_SET_DATA_SIZE_MAX - adv_len - 2;
        bool truncated = name_len > room;
        uint16_t n = truncated ? room : name_len;
        enc_adv[adv_len++] = n + 1;
        enc_adv[adv_len++] = truncated ? 0x08 : 0x09;
        memcpy(&enc_adv[adv_len], name, n);
        adv_len += n;
    }

    ble_gap_adv_data_t gap_adv_data;
    memset(&gap_adv_data, 0, sizeof(gap_adv_data));
    gap_adv_data.adv_data.p_data = enc_adv;
    gap_adv_data.adv_data.len = adv_len;
    // Scan response = MIDI UUID (18 bytes) + shortened name (up to 11 bytes). Windows was only
    // ever seen to receive the scan response from this board (2026-09-29), so the name goes here
    // too, otherwise Windows/MIDIberry hide the device as unnamed.
    static uint8_t scanrsp[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
    memcpy(scanrsp, midi_scanrsp_uuid, sizeof(midi_scanrsp_uuid));
    uint16_t scanrsp_len = sizeof(midi_scanrsp_uuid);
    static const char shortName[] = "micro:bit";
    scanrsp[scanrsp_len++] = sizeof(shortName);          // length = type byte + 9 chars
    scanrsp[scanrsp_len++] = 0x08;                       // shortened local name
    memcpy(&scanrsp[scanrsp_len], shortName, sizeof(shortName) - 1);
    scanrsp_len += sizeof(shortName) - 1;
    gap_adv_data.scan_rsp_data.p_data = scanrsp;
    gap_adv_data.scan_rsp_data.len = scanrsp_len;

    ble_gap_adv_params_t gap_adv_params;
    memset(&gap_adv_params, 0, sizeof(gap_adv_params));
    gap_adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    gap_adv_params.interval = (1000 * MICROBIT_BLE_ADVERTISING_INTERVAL) / 625;
    if (gap_adv_params.interval < BLE_GAP_ADV_INTERVAL_MIN)
        gap_adv_params.interval = BLE_GAP_ADV_INTERVAL_MIN;
    gap_adv_params.duration = 0;
    // Always accept connections from any central. Whitelist filtering here caused
    // "visible in scan, connection timeout" when no bonded peers exist.
    gap_adv_params.filter_policy = BLE_GAP_ADV_FP_ANY;
    gap_adv_params.primary_phy = BLE_GAP_PHY_1MBPS;

    sd_ble_gap_adv_stop(adv_handle);
    MICROBIT_BLE_ECHK(sd_ble_gap_adv_set_configure(&adv_handle, &gap_adv_data, &gap_adv_params));

    if (MicroBitBLEManager::manager) {
        MicroBitBLEManager::manager->setAdvertiseOnDisconnect(true);
        MicroBitBLEManager::manager->advertise();
    }
}

// BLE MIDI service: 03B80E5A-EDE8-4B33-A751-6CE34EC4C700
const uint8_t BluetoothMIDIService::service_base_uuid[16] =
    { 0x03, 0xb8, 0x00, 0x00, 0xed, 0xe8, 0x4b, 0x33,
      0xa7, 0x51, 0x6c, 0xe3, 0x4e, 0xc4, 0xc7, 0x00 };

// BLE MIDI characteristic: 7772E5DB-3868-412A-A1A9-F2669D106BF3
const uint8_t BluetoothMIDIService::char_base_uuid[16] =
    { 0x77, 0x72, 0x00, 0x00, 0x38, 0x68, 0x41, 0x12,
      0xa1, 0xa9, 0xf2, 0x66, 0x9d, 0x10, 0x6b, 0xf3 };

const uint16_t BluetoothMIDIService::serviceUUID = 0x0e5a;
const uint16_t BluetoothMIDIService::charUUID[mbbs_cIdxCOUNT] = { 0xe5db };

BluetoothMIDIService::BluetoothMIDIService(BLEDevice &_ble)
{
    memset(midiBuffer, 0, sizeof(midiBuffer));
    pendingHandshake = true;

    RegisterBaseUUID(service_base_uuid);
    CreateService(serviceUUID);
    midiAdvUuidType = bs_uuid_type;

    RegisterBaseUUID(char_base_uuid);
    uint16_t props = microbit_propREAD | microbit_propWRITE | microbit_propWRITE_WITHOUT | microbit_propNOTIFY;
#if !CONFIG_ENABLED(MICROBIT_BLE_OPEN)
    props |= microbit_propREADAUTH;
#endif
    CreateCharacteristic(mbbs_cIdxMIDI, charUUID[mbbs_cIdxMIDI],
        midiBuffer, 0, sizeof(midiBuffer), props);

    configureMidiAdvertising(midiAdvUuidType);

    if (MicroBitBLEManager::manager)
        MicroBitBLEManager::manager->servicesChanged();
}

void BluetoothMIDIService::completeMidiHandshake()
{
    if (!pendingHandshake || !getConnected())
        return;
    if (!chars[mbbs_cIdxMIDI].cccdNotify())
        return;

    microbit_gaphandle_t conn = getConnectionHandle();
    uint16_t len = midi_handshake_len;
    ble_gatts_hvx_params_t hvx;
    memset(&hvx, 0, sizeof(hvx));
    hvx.handle = valueHandle(mbbs_cIdxMIDI);
    hvx.type = BLE_GATT_HVX_NOTIFICATION;
    hvx.p_len = &len;
    hvx.p_data = (uint8_t *)midi_handshake_pkt;

    if (sd_ble_gatts_hvx(conn, &hvx) == NRF_SUCCESS)
        pendingHandshake = false;
}

void BluetoothMIDIService::scheduleMidiHandshake()
{
    if (!pendingHandshake || !getConnected())
        return;
    create_fiber(midiHandshakeFiber, this);
}

void BluetoothMIDIService::runHandshakeRetries()
{
    for (int attempt = 0; attempt < 24 && pendingHandshake; attempt++) {
        fiber_sleep(50);
        if (!getConnected())
            return;
        completeMidiHandshake();
    }
}

void BluetoothMIDIService::onConnect(const microbit_ble_evt_t *p_ble_evt)
{
    (void)p_ble_evt;
    pendingHandshake = true;
    scheduleMidiHandshake();
}

void BluetoothMIDIService::onDataRead(microbit_onDataRead_t *params)
{
    if (params->handle != valueHandle(mbbs_cIdxMIDI))
        return;

    params->data = midiBuffer;
    params->length = 0;
    params->allow = true;
    params->update = true;

    scheduleMidiHandshake();
}

void BluetoothMIDIService::onDataWritten(const microbit_ble_evt_write_t *params)
{
    microbit_charattr_t type;
    int idx = charHandleToIdx(params->handle, &type);
    if (idx < 0)
        return;

    if (type == microbit_charattrCCCD && params->len == 2) {
        uint16_t cccd = uint16_decode(params->data);
        if (cccd & BLE_GATT_HVX_NOTIFICATION)
            scheduleMidiHandshake();
    } else if (idx == mbbs_cIdxMIDI) {
        scheduleMidiHandshake();
    }
}

void BluetoothMIDIService::onDisconnect(const microbit_ble_evt_t *p_ble_evt)
{
    (void)p_ble_evt;
    pendingHandshake = true;
    configureMidiAdvertising(midiAdvUuidType);
}

bool BluetoothMIDIService::connected()
{
    return getConnected();
}

void BluetoothMIDIService::sendMidiMessage(uint8_t data0)
{
    if (connected())
    {
        unsigned int ticks = (unsigned int)system_timer_current_time() & 0x1fff;
        midiBuffer[0] = 0x80 | ((ticks >> 7) & 0x3f);
        midiBuffer[1] = 0x80 | (ticks & 0x7f);
        midiBuffer[2] = data0;

        notifyChrValue(mbbs_cIdxMIDI, midiBuffer, 3);
    }
}

void BluetoothMIDIService::sendMidiMessage(uint8_t data0, uint8_t data1)
{
    if (connected())
    {
        unsigned int ticks = (unsigned int)system_timer_current_time() & 0x1fff;
        midiBuffer[0] = 0x80 | ((ticks >> 7) & 0x3f);
        midiBuffer[1] = 0x80 | (ticks & 0x7f);
        midiBuffer[2] = data0;
        midiBuffer[3] = data1;

        notifyChrValue(mbbs_cIdxMIDI, midiBuffer, 4);
    }
}

void BluetoothMIDIService::sendMidiMessage(uint8_t data0, uint8_t data1, uint8_t data2)
{
    if (connected())
    {
        unsigned int ticks = (unsigned int)system_timer_current_time() & 0x1fff;
        midiBuffer[0] = 0x80 | ((ticks >> 7) & 0x3f);
        midiBuffer[1] = 0x80 | (ticks & 0x7f);
        midiBuffer[2] = data0;
        midiBuffer[3] = data1;
        midiBuffer[4] = data2;

        notifyChrValue(mbbs_cIdxMIDI, midiBuffer, 5);
    }
}

//================================================================
#else // MICROBIT_CODAL
//================================================================

#include "MicroBit.h"
#include "pxt.h"


// MIDI characteristic
const uint8_t midiCharacteristicUuid[] = {
        0x77, 0x72, 0xe5, 0xdb, 0x38, 0x68, 0x41, 0x12, 
        0xa1, 0xa9, 0xf2, 0x66, 0x9d, 0x10, 0x6b, 0xf3
};

// MIDI service
const uint8_t midiServiceUuid[] = {
        0x03, 0xb8, 0x0e, 0x5a, 0xed, 0xe8, 0x4b, 0x33, 
        0xa7, 0x51, 0x6c, 0xe3, 0x4e, 0xc4, 0xc7, 0x00
};

BluetoothMIDIService::BluetoothMIDIService(BLEDevice *dev): ble(*dev) {
    timestamp = 0;
    advertisingStatus = 0;
    memset(midiBuffer, 0, sizeof(midiBuffer));
    firstRead = true;

    GattCharacteristic midiCharacteristic(midiCharacteristicUuid, midiBuffer, 0, sizeof(midiBuffer), 
          GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ
        | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_WRITE_WITHOUT_RESPONSE // required by the BLE MIDI spec; incoming MIDI is ignored
        | GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY
        );
    GattCharacteristic *midiChars[] = {&midiCharacteristic};

    midiCharacteristic.requireSecurity(SecurityManager::MICROBIT_BLE_SECURITY_LEVEL);

    GattService midiService(midiServiceUuid, midiChars, sizeof(midiChars) / sizeof(GattCharacteristic *));

    ble.addService(midiService);

    // iOS/macOS BLE MIDI browsers only list peripherals that advertise the MIDI
    // service UUID. Put the UUID in the advertising packet and move the device
    // name to the scan response (both don't fit in 31 bytes).
    uint8_t advUuid[16];
    for (int i = 0; i < 16; i++)
        advUuid[i] = midiServiceUuid[15 - i]; // advertising data is little-endian

    // same name the DAL advertises, e.g. "BBC micro:bit [zegap]"
    ManagedString bleName = ManagedString("BBC micro:bit [") + uBit.getName() + ManagedString("]");
    const uint8_t *name = (const uint8_t *)bleName.toCharArray();
    unsigned nameLength = bleName.length();
    ble.gap().setDeviceName(name);

    // advertisingStatus: 1 = all steps ok, otherwise step * 100 + BLE error code
    ble_error_t err;
    advertisingStatus = 1;
    if ((err = ble.gap().stopAdvertising()) != BLE_ERROR_NONE) advertisingStatus = 100 + err;
    ble.gap().clearAdvertisingPayload();
    ble.gap().clearScanResponse();
    if ((err = ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::BREDR_NOT_SUPPORTED | GapAdvertisingData::LE_GENERAL_DISCOVERABLE)) != BLE_ERROR_NONE && advertisingStatus == 1) advertisingStatus = 200 + err;
    if ((err = ble.gap().accumulateAdvertisingPayload(GapAdvertisingData::COMPLETE_LIST_128BIT_SERVICE_IDS, advUuid, sizeof(advUuid))) != BLE_ERROR_NONE && advertisingStatus == 1) advertisingStatus = 300 + err;
    if (nameLength > 0 && (err = ble.gap().accumulateScanResponse(GapAdvertisingData::COMPLETE_LOCAL_NAME, name, nameLength)) != BLE_ERROR_NONE && advertisingStatus == 1) advertisingStatus = 400 + err;
    if ((err = ble.gap().startAdvertising()) != BLE_ERROR_NONE && advertisingStatus == 1) advertisingStatus = 500 + err;

    midiCharacteristicHandle = midiCharacteristic.getValueHandle();

    ble.gattServer().onDataRead(this, &BluetoothMIDIService::onDataRead);
    ble.onDisconnection(this, &BluetoothMIDIService::onDisconnection);

    tick.start();
}

void BluetoothMIDIService::onDataRead(const GattReadCallbackParams* params) 
{
    if (params->handle == midiCharacteristicHandle) 
    {
        if (firstRead) {
            // send empty payload upon first connect
            ble.gattServer().notify(midiCharacteristicHandle, (uint8_t *)midiBuffer, 0);
            firstRead = false;
        }
    }
}

void BluetoothMIDIService::onDisconnection(const Gap::DisconnectionCallbackParams_t* params) {
    // clear read bit
    firstRead = true;
}

bool BluetoothMIDIService::connected() {
    return ble.getGapState().connected;
}

void BluetoothMIDIService::sendMidiMessage(uint8_t data0) {
    if (connected()) {
        unsigned int ticks = tick.read_ms() & 0x1fff;
        midiBuffer[0] = 0x80 | ((ticks >> 7) & 0x3f);
        midiBuffer[1] = 0x80 | (ticks & 0x7f);
        midiBuffer[2] = data0;
        
        ble.gattServer().notify(midiCharacteristicHandle, (uint8_t *)midiBuffer, 3);
    }
}

void BluetoothMIDIService::sendMidiMessage(uint8_t data0, uint8_t data1) {
    if (connected()) {
        unsigned int ticks = tick.read_ms() & 0x1fff;
        midiBuffer[0] = 0x80 | ((ticks >> 7) & 0x3f);
        midiBuffer[1] = 0x80 | (ticks & 0x7f);
        midiBuffer[2] = data0;
        midiBuffer[3] = data1;
        
        ble.gattServer().notify(midiCharacteristicHandle, (uint8_t *)midiBuffer, 4);
    }
}

void BluetoothMIDIService::sendMidiMessage(uint8_t data0, uint8_t data1, uint8_t data2) {
    if (connected()) {
        unsigned int ticks = tick.read_ms() & 0x1fff;
        midiBuffer[0] = 0x80 | ((ticks >> 7) & 0x3f);
        midiBuffer[1] = 0x80 | (ticks & 0x7f);
        midiBuffer[2] = data0;
        midiBuffer[3] = data1;
        midiBuffer[4] = data2;
        
        ble.gattServer().notify(midiCharacteristicHandle, (uint8_t *)midiBuffer, 5);
    }
}

//================================================================
#endif // MICROBIT_CODAL
//================================================================
