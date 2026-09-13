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

    // ui::TextField's own draw() renders through Blendish's bndTextField(),
    // which visibly drops overflowing tail characters below the field once
    // the payload is long enough -- confirmed on a real hex string, and not
    // fixable by scissoring around the call (Blendish does its own thing
    // regardless). Drawn entirely ourselves instead: single NanoVG line,
    // hard-clipped, with our own cursor/selection/scroll -- same approach
    // already used for MidiPortChoice below for the same reason (don't trust
    // a Rack/Blendish built-in to keep long text on one line).
    float scrollX = 0.f;
    static constexpr float PAD = 4.f;
    static constexpr float FONT_SIZE = 9.5f;

    static void setFont(NVGcontext* vg) {
        nvgFontSize(vg, FONT_SIZE);
        nvgFontFaceId(vg, APP->window->uiFont->handle);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    }

    static float textWidth(NVGcontext* vg, const std::string& s) {
        float bounds[4];
        nvgTextBounds(vg, 0, 0, s.c_str(), nullptr, bounds);
        return bounds[2] - bounds[0];
    }

    // Overridden so clicks map to a character index using the *same* font
    // metrics as our own draw() -- the base class's implementation calls
    // Blendish's bndTextFieldTextPosition(), which assumes Blendish's own
    // internal font/size. Since we no longer draw through Blendish at all,
    // that mismatch made the cursor land visibly off from where you clicked.
    int getTextPosition(math::Vec mousePos) override {
        NVGcontext* vg = APP->window->vg;
        setFont(vg);
        float targetX = mousePos.x - (PAD - scrollX);
        int best = 0;
        float bestDist = std::abs(targetX);
        for (size_t i = 1; i <= text.size(); i++) {
            float dist = std::abs(targetX - textWidth(vg, text.substr(0, i)));
            if (dist < bestDist) { bestDist = dist; best = (int)i; }
        }
        return best;
    }

    void draw(const DrawArgs& args) override {
        const float pad = PAD;
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0.f, 0.f, box.size.x, box.size.y, 2.f);
        nvgFillColor(args.vg, nvgRGB(0xf3, 0xf3, 0xf0));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGBA(0x23, 0x26, 0x2a, 0x50));
        nvgStrokeWidth(args.vg, 0.6f);
        nvgStroke(args.vg);

        nvgSave(args.vg);
        nvgIntersectScissor(args.vg, 0.f, 0.f, box.size.x, box.size.y);
        setFont(args.vg);

        bool focused = (APP->event->selectedWidget == this);
        if (text.empty()) {
            nvgFillColor(args.vg, nvgRGBA(0x23, 0x26, 0x2a, 0x60));
            nvgText(args.vg, pad, box.size.y * 0.5f, placeholder.c_str(), nullptr);
        }
        else {
            // Keep the cursor in view: shift the whole line left/right just
            // enough that the cursor's x position stays within the padded box.
            float cursorX = textWidth(args.vg, text.substr(0, cursor));
            float visibleW = box.size.x - 2.f * pad;
            if (cursorX - scrollX > visibleW) scrollX = cursorX - visibleW;
            if (cursorX - scrollX < 0.f) scrollX = cursorX;
            if (scrollX < 0.f) scrollX = 0.f;
            float originX = pad - scrollX;

            if (focused && cursor != selection) {
                int lo = std::min(cursor, selection), hi = std::max(cursor, selection);
                float x0 = originX + textWidth(args.vg, text.substr(0, lo));
                float x1 = originX + textWidth(args.vg, text.substr(0, hi));
                nvgBeginPath(args.vg);
                nvgRect(args.vg, x0, 1.f, x1 - x0, box.size.y - 2.f);
                nvgFillColor(args.vg, nvgRGBA(0x3a, 0x7a, 0xd0, 0x50));
                nvgFill(args.vg);
            }

            nvgFillColor(args.vg, nvgRGB(0x23, 0x26, 0x2a));
            nvgText(args.vg, originX, box.size.y * 0.5f, text.c_str(), nullptr);

            if (focused) {
                float cx = originX + cursorX;
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, cx, 2.f);
                nvgLineTo(args.vg, cx, box.size.y - 2.f);
                nvgStrokeColor(args.vg, nvgRGB(0x23, 0x26, 0x2a));
                nvgStrokeWidth(args.vg, 1.f);
                nvgStroke(args.vg);
            }
        }
        nvgRestore(args.vg);
    }

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
