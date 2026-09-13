#pragma once
#include <rack.hpp>
#include <string>
#include <vector>
#include <cctype>
#include <mutex>

using namespace rack;

// ---------------------------------------------------------------------------
// Utility: parse hex SysEx string → byte vector
// Returns empty vector on any error.
// Rules:
//   • ignore whitespace/newlines
//   • accept upper/lower hex
//   • must start with F0, end with F7
//   • every token must be exactly 2 hex digits
// ---------------------------------------------------------------------------
static inline std::vector<uint8_t> parseSysEx(const std::string& s) {
    std::vector<uint8_t> result;
    result.reserve(64);

    std::string token;
    for (size_t i = 0; i <= s.size(); ++i) {
        char c = (i < s.size()) ? s[i] : '\0';

        if (std::isspace((unsigned char)c) || c == '\0') {
            if (!token.empty()) {
                if (token.size() != 2) return {};
                char h = token[0], l = token[1];
                auto hexval = [](char x) -> int {
                    if (x >= '0' && x <= '9') return x - '0';
                    if (x >= 'a' && x <= 'f') return x - 'a' + 10;
                    if (x >= 'A' && x <= 'F') return x - 'A' + 10;
                    return -1;
                };
                int hi = hexval(h), lo = hexval(l);
                if (hi < 0 || lo < 0) return {};
                result.push_back((uint8_t)((hi << 4) | lo));
                token.clear();
            }
        } else {
            token += c;
            if (token.size() > 2) return {};
        }
    }

    if (result.size() < 2) return {};
    if (result.front() != 0xF0) return {};
    if (result.back()  != 0xF7) return {};
    return result;
}

// ---------------------------------------------------------------------------
// Per-slot data (shared between audio thread and UI thread)
// ---------------------------------------------------------------------------
struct SlotData {
    // UI thread writes, audio thread reads via atomic snapshot
    std::string  hexString;                // raw user string
    std::vector<uint8_t> cachedBytes;      // pre-parsed
    bool         valid = false;            // cached result is usable

    // set from UI thread; protected by module-level mutex
    void update(const std::string& newStr) {
        hexString   = newStr;
        cachedBytes = parseSysEx(newStr);
        valid       = !cachedBytes.empty();
    }
};

// ---------------------------------------------------------------------------
// SysExBank module
// ---------------------------------------------------------------------------
struct SysExBank : Module {
    static const int NUM_SLOTS = 16;

    enum InputIds {
        ENUMS(TRIG_INPUT, NUM_SLOTS),
        NUM_INPUTS
    };
    enum OutputIds {
        NUM_OUTPUTS
    };
    enum ParamIds {
        ENUMS(SEND_PARAM, NUM_SLOTS),
        NUM_PARAMS
    };
    enum LightIds {
        ENUMS(SEND_LIGHT, NUM_SLOTS),
        NUM_LIGHTS
    };

    // MIDI output
    midi::Output midiOutput;

    // Slot data (mutex protects string/cache, audio thread only reads valid/cachedBytes atomically)
    mutable std::mutex slotMutex;
    SlotData slots[NUM_SLOTS];

    // Trigger detectors (per slot)
    dsp::SchmittTrigger trigDetector[NUM_SLOTS];

    // Button detectors (per slot) — BooleanTrigger for momentary params
    dsp::BooleanTrigger btnDetector[NUM_SLOTS];

    // Light blink timer per slot
    float lightTimer[NUM_SLOTS] = {};

    // -----------------------------------------------------------------------
    SysExBank() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        for (int i = 0; i < NUM_SLOTS; i++) {
            configInput(TRIG_INPUT + i,  string::f("Slot %d trigger", i + 1));
            configParam(SEND_PARAM + i, 0.f, 1.f, 0.f,
                        string::f("Send slot %d", i + 1), "");
        }
    }

    // -----------------------------------------------------------------------
    // Send SysEx for slot i (called from audio thread)
    // -----------------------------------------------------------------------
    void sendSysex(int i) {
        // Take a local snapshot under lock
        std::vector<uint8_t> bytes;
        bool valid = false;
        {
            std::lock_guard<std::mutex> lock(slotMutex);
            valid = slots[i].valid;
            if (valid) bytes = slots[i].cachedBytes;
        }

        if (!valid || bytes.empty()) return;

        midi::Message msg;
        msg.setSize(bytes.size());
        for (size_t k = 0; k < bytes.size(); k++)
            msg.bytes[k] = bytes[k];

        midiOutput.sendMessage(msg);
        lightTimer[i] = 0.1f; // 100ms blink
    }

    // -----------------------------------------------------------------------
    // Update slot string from UI thread
    // -----------------------------------------------------------------------
    void setSlotString(int i, const std::string& s) {
        std::lock_guard<std::mutex> lock(slotMutex);
        slots[i].update(s);
    }

    std::string getSlotString(int i) const {
        std::lock_guard<std::mutex> lock(slotMutex);
        return slots[i].hexString;
    }

    // -----------------------------------------------------------------------
    void process(const ProcessArgs& args) override {
        for (int i = 0; i < NUM_SLOTS; i++) {
            // Trigger input — rising edge
            if (inputs[TRIG_INPUT + i].isConnected()) {
                float v = inputs[TRIG_INPUT + i].getVoltage();
                if (trigDetector[i].process(v, 0.1f, 2.f)) {
                    sendSysex(i);
                }
            }

            // Button — momentary param
            bool btnPressed = (params[SEND_PARAM + i].getValue() > 0.5f);
            if (btnDetector[i].process(btnPressed)) {
                sendSysex(i);
            }

            // Light decay
            if (lightTimer[i] > 0.f) {
                lightTimer[i] -= args.sampleTime;
                lights[SEND_LIGHT + i].setBrightness(1.f);
            } else {
                lights[SEND_LIGHT + i].setBrightness(0.f);
            }
        }

    }

    // -----------------------------------------------------------------------
    json_t* dataToJson() override {
        json_t* root = json_object();

        json_t* slotsArr = json_array();
        for (int i = 0; i < NUM_SLOTS; i++) {
            json_array_append_new(slotsArr,
                json_string(getSlotString(i).c_str()));
        }
        json_object_set_new(root, "slots", slotsArr);

        json_object_set_new(root, "midi", midiOutput.toJson());

        return root;
    }

    void dataFromJson(json_t* root) override {
        json_t* slotsArr = json_object_get(root, "slots");
        if (slotsArr && json_is_array(slotsArr)) {
            for (int i = 0; i < NUM_SLOTS; i++) {
                json_t* item = json_array_get(slotsArr, i);
                if (item && json_is_string(item)) {
                    setSlotString(i, json_string_value(item));
                }
            }
        }

        json_t* midiJ = json_object_get(root, "midi");
        if (midiJ) midiOutput.fromJson(midiJ);
    }
};
