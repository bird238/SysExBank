#pragma once
#include <rack.hpp>
#include "module.hpp"

using namespace rack;

// ---------------------------------------------------------------------------
// SysExTextField — single-line text field bound to a slot
// ---------------------------------------------------------------------------
struct SysExTextField : ui::TextField {
    SysExBank* module = nullptr;
    int slotIndex     = -1;

    void onChange(const ChangeEvent& e) override {
        ui::TextField::onChange(e);
        if (module && slotIndex >= 0) {
            module->setSlotString(slotIndex, text);
        }
    }

    void step() override {
        ui::TextField::step();
        // Sync display text from module after dataFromJson or undo
        if (module && slotIndex >= 0) {
            std::string moduleStr = module->getSlotString(slotIndex);
            if (text != moduleStr) {
                setText(moduleStr);
            }
        }
    }
};

// ---------------------------------------------------------------------------
// SysExBankWidget
// ---------------------------------------------------------------------------
struct SysExBankWidget : ModuleWidget {
    SysExBankWidget(SysExBank* module);
};
