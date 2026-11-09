# BLE state characteristic — what the monitor page connects to

Everything here is also in [`firmware/config.h`](../firmware/config.h); if the two disagree, the
firmware is right and this file is a bug.

| | |
| --- | --- |
| Device name (advertised) | **`AppII-Monitor`** |
| Service UUID (128-bit, primary) | **`53bc7f1a-6719-4a10-917d-11be1aba66a3`** |
| Characteristic UUID | **`e8507aa2-f631-4a05-893c-2a781f9d5be3`** |
| Properties | `READ` + `NOTIFY` |
| Value encoding | plain UTF-8 text, **no newline, no other characters**, max 32 bytes |
| Stack | NimBLE-Arduino 2.3.9, peripheral role, service UUID in the advertisement |

## The only values the characteristic ever holds

Exactly one of the class names exported by `training/train.py` into
`firmware/model_weights.h` (`MODEL_CLASS_NAMES`), so the firmware and the model can never
disagree about a label:

```
calm_breathing
fast_breathing
not_attached
shaking
```

An **empty** value is also possible, and means "the board has not committed to a state yet" —
the first 2.5 s window, or the I²C bus is dead (the LED blinks amber in that case).

## When it notifies

* **on every state change** — the same event that repaints the LED;
* **once per second as a heartbeat**, whether or not anything changed.

The heartbeat is what lets a page distinguish "the board says calm" from "the board is gone":
no notification for ~3 s means the link or the board died, rather than a stable classification.

## Reading it from a browser (Web Bluetooth)

Web Bluetooth needs a secure context, which is why the monitor page is served over **HTTPS from
GitHub Pages**, not from the board's own hotspot. Device selection must start from a click.

```js
const SERVICE = '53bc7f1a-6719-4a10-917d-11be1aba66a3';
const STATE   = 'e8507aa2-f631-4a05-893c-2a781f9d5be3';

const device = await navigator.bluetooth.requestDevice({
  filters: [{ services: [SERVICE] }],          // or acceptAnyDevice-style namePrefix 'AppII-'
});
const server = await device.gatt.connect();
const service = await server.getPrimaryService(SERVICE);
const ch = await service.getCharacteristic(STATE);
await ch.startNotifications();
ch.addEventListener('characteristicvaluechanged', (e) => {
  const text = new TextDecoder().decode(e.target.value);   // 'shaking' | 'calm_breathing' | ...
  document.body.dataset.state = text || 'unknown';
});
```

A central disconnecting does **not** need a power cycle: `onDisconnect` calls
`NimBLEDevice::startAdvertising()`, so the board is discoverable again immediately.

## What is *not* in BLE

No raw samples, no probabilities, no window buffers. The state string is the whole contract, and
it is computed on the chip (`firmware/features.h` + `firmware/model_infer.h`) — BLE only carries
the verdict. Nothing in the sketch waits for a host, and there is no USB serial in it at all
(`grep -c Serial` reads 0), so it runs from a power bank with the monitor page open on another
machine.

Measured with this build: **578221 bytes (44 %)** flash, **40172 bytes (12 %)** RAM
(`esp32:esp32:esp32s3:FlashMode=dio,FlashSize=16M,PSRAM=disabled,CDCOnBoot=default`) — the BLE
stack is most of the flash growth over the 327578-byte non-BLE build.
