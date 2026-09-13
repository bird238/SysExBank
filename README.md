# SysEx Bank — VCV Rack 2 Plugin

**SysEx Bank** is a 32HP utility plugin for VCV Rack 2 that stores and transmits raw MIDI System Exclusive (SysEx) messages. It features 16 independent slots arranged in two 8-slot columns, each with its own CV trigger input, manual send button, editable hex string field, and activity LED, sharing a single MIDI output port selector.

---

## Build

### Prerequisites

- VCV Rack 2 SDK (2.4.0+ / 2.6.x)
- C++17 compiler toolchain (`g++` / `clang++`, `make`)

### Compiling

```bash
export RACK_DIR=/path/to/Rack-SDK
make -j$(nproc)
```

This generates `plugin.so` (Linux), `plugin.dylib` (macOS), or `plugin.dll` (Windows).

### Installation

Copy the compiled plugin directory to your system's VCV Rack plugins folder:

- **Linux**: `~/.local/share/Rack2/plugins-lin-x64/SysExBank`
- **macOS**: `~/Documents/Rack2/plugins-mac-x64/SysExBank` (or `plugins-mac-arm64`)
- **Windows**: `%USERPROFILE%\Documents\Rack2\plugins-win-x64\SysExBank`

Example Linux install command:

```bash
PLUGIN_DIR=~/.local/share/Rack2/plugins-lin-x64/SysExBank
mkdir -p "$PLUGIN_DIR"
cp plugin.so plugin.json "$PLUGIN_DIR/"
```

Restart VCV Rack after installing.

---

## Panel Layout & Usage

The module uses a 32HP light brushed-aluminum panel with 16 slots divided into two 8-slot columns (slots 1–8 in the left column, 9–16 in the right column).

| Element | Type | Function |
|---|---|---|
| **MIDI Output** | Custom `MidiOutChoice` (single-row, click-to-open combined driver+device menu via `app::appendMidiMenu`) | Selects MIDI driver and target output device (shared across all 16 slots). |
| **S01–S16 Jack** | CV Input | Trigger jack (`dsp::SchmittTrigger`, 0.1 V low / 2.0 V high) to transmit the slot's SysEx message on rising edge. |
| **Hex Text Field** | Text Input | Editable hex string field (~49mm) containing raw SysEx bytes. Clipped to a single line regardless of payload length, so a long hex string never visually wraps into the row below. |
| **SEND Button** | Pushbutton | Momentary button (`dsp::BooleanTrigger`) to send the SysEx payload manually. |
| **Status LED** | Green LED | Blinks for 100 ms whenever the slot sends its SysEx message. |

---

## SysEx String Format

Slot payloads are entered as space-separated hexadecimal strings.

### Formatting Rules

- Message must start with `F0` (SysEx Start) and end with `F7` (SysEx End).
- Case-insensitive hexadecimal bytes (`f0` or `F0`), exactly 2 hex digits per token.
- Whitespace between tokens is ignored.
- Invalid or malformed strings are silently ignored and will not transmit partial garbage data.

### Generic Example

Using manufacturer ID `0x7D` (reserved by the MIDI Manufacturers Association specification for non-commercial, educational, and prototype use):

```
F0 7D 01 02 03 F7
```

---

## State Persistence

All 16 hex string payloads and the shared MIDI output driver/device selection are saved directly into the VCV Rack patch file (`dataToJson` / `dataFromJson`).

---

## Architecture & Performance

- **Thread Safety**: Hex text strings and pre-parsed byte caches are guarded by `std::mutex`.
- **Pre-parsing**: Text fields are re-parsed into byte caches on UI edits, not on trigger events.
- **Audio Hot Path**: Audio-thread execution locks the mutex briefly to copy the pre-parsed `std::vector<uint8_t>` and send it to `midi::Output`; no string parsing or dynamic heap allocations occur at audio rate.
- **Trigger Hysteresis**: Jack inputs use `dsp::SchmittTrigger` (0.1 V / 2.0 V thresholds) to prevent multi-firing.
