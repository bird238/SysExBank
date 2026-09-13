#include "ui.hpp"
#include "plugin.hpp"
#include <rack.hpp>

using namespace rack;

// ---------------------------------------------------------------------------
// Layout constants (mm). Module is 2 columns x 8 slots instead of 1 column x
// 16, so each row gets roughly double the height of the original single-file
// layout and the module is narrower overall.
// ---------------------------------------------------------------------------

static const float MODULE_HP   = 32.f;
static const float MODULE_W_MM = MODULE_HP * 5.08f; // 162.56 mm

static const float RACK_H_MM = 380.f / (75.f / 25.4f); // ~128.69 mm

static const float TITLE_H = 6.f;
static const float MIDI_H  = 10.f;

static const float SLOTS_TOP = TITLE_H + 1.f;
static const float SLOTS_BOT = RACK_H_MM - MIDI_H - 1.5f;
static const int   ROWS_PER_COL = SysExBank::NUM_SLOTS / 2;
static const float SLOT_H = (SLOTS_BOT - SLOTS_TOP) / ROWS_PER_COL;

static const float MARGIN = 3.f;
static const float GUTTER = 4.f;
static const float COL_W  = (MODULE_W_MM - 2.f * MARGIN - GUTTER) / 2.f;
static const float COL_X[2] = {MARGIN, MARGIN + COL_W + GUTTER};

// Column-local horizontal positions
static const float CX_JACK  = 5.f;
static const float CX_NUM   = 9.f;
static const float CX_FIELD = 15.f;
static const float CX_BTN   = COL_W - 8.f;
static const float CX_LED   = COL_W - 3.f;
static const float FIELD_W  = (CX_BTN - 4.f) - CX_FIELD;
// Fixed, compact height regardless of the (now much taller) row height -- ui::TextField anchors
// its text a fixed distance from the top of its own box, so a box much taller than one text line
// reads as "text sitting too high"; keeping the box close to natural text-line height and
// centering that box within the row avoids the problem instead of fighting the native widget.
static const float FIELD_H  = 6.4f;

static const NVGcolor INK = nvgRGB(0x23, 0x26, 0x2a);

// ---------------------------------------------------------------------------
// BgPanel — standard light brushed-aluminum look shared with other panels in
// this collection, drawn procedurally since this module ships no SVG assets.
// ---------------------------------------------------------------------------
struct BgPanel : Widget {
    int rowsPerCol;
    float slotH_px, slotsTop_px;
    float col0X_px, col1X_px, colW_px;

    void draw(const DrawArgs& args) override {
        NVGpaint grad = nvgLinearGradient(args.vg, 0, 0, 0, box.size.y,
            nvgRGB(0xe8, 0xe8, 0xe6), nvgRGB(0xcc, 0xcd, 0xc9));
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgFillPaint(args.vg, grad);
        nvgFill(args.vg);

        float colX[2] = {col0X_px, col1X_px};
        for (float cx : colX) {
            for (int i = 0; i < rowsPerCol; i++) {
                if (i % 2 == 0) {
                    nvgBeginPath(args.vg);
                    nvgRect(args.vg, cx, slotsTop_px + i * slotH_px, colW_px, slotH_px);
                    nvgFillColor(args.vg, nvgRGBAf(0, 0, 0, 0.035f));
                    nvgFill(args.vg);
                }
            }

            nvgStrokeWidth(args.vg, 0.5f);
            nvgStrokeColor(args.vg, nvgRGBAf(0, 0, 0, 0.12f));
            for (int i = 0; i <= rowsPerCol; i++) {
                float y = slotsTop_px + i * slotH_px;
                nvgBeginPath(args.vg);
                nvgMoveTo(args.vg, cx, y);
                nvgLineTo(args.vg, cx + colW_px, y);
                nvgStroke(args.vg);
            }
        }
    }
};

// ---------------------------------------------------------------------------
// MidiOutChoice — single-row MIDI port selector. Clicking it opens the
// standard combined driver+device menu (app::appendMidiMenu) instead of
// stacking separate driver/device rows, so the whole MIDI section is one
// line instead of two.
// ---------------------------------------------------------------------------
struct MidiOutChoice : app::LedDisplayChoice {
    midi::Port* port = nullptr;

    void step() override {
        if (port) {
            int deviceId = port->getDeviceId();
            text = "MIDI OUT: " + (deviceId < 0 ? std::string("click to select") : port->getDeviceName(deviceId));
        }
        app::LedDisplayChoice::step();
    }
    // Draw the text ourselves, vertically centered -- the inherited LedDisplayChoice::draw()
    // anchors text a fixed distance from the top, which only looks centered at the small
    // (~13px) row height it was designed for; this widget is taller than that.
    // LedDisplayChoice draws its text in drawLayer(layer 1), not draw() -- overriding only
    // draw() left the inherited drawLayer() still running, so the text was rendered twice
    // (once by us, once by the base class). Replace drawLayer() instead of adding a second
    // draw path, and don't call the base implementation.
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;
        nvgFontSize(args.vg, 12.f);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, color);
        nvgText(args.vg, 6.f, box.size.y * 0.5f, text.c_str(), nullptr);
    }
    void onAction(const ActionEvent& e) override {
        if (!port) return;
        ui::Menu* menu = createMenu();
        menu->addChild(createMenuLabel("MIDI output"));
        app::appendMidiMenu(menu, port);
    }
};

// ---------------------------------------------------------------------------
// TinyLabel — compact text label, no built-in padding.
// ---------------------------------------------------------------------------
struct TinyLabel : Widget {
    std::string text;
    NVGcolor color = INK;
    float fontSize = 8.5f;
    int align = NVG_ALIGN_LEFT;

    void draw(const DrawArgs& args) override {
        nvgFontSize(args.vg, fontSize);
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgTextAlign(args.vg, align | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, color);
        float x = (align & NVG_ALIGN_CENTER) ? box.size.x * 0.5f : 0.f;
        nvgText(args.vg, x, box.size.y * 0.5f, text.c_str(), nullptr);
    }
};

// ---------------------------------------------------------------------------
SysExBankWidget::SysExBankWidget(SysExBank* module) {
    setModule(module);
    box.size = Vec(mm2px(MODULE_W_MM), RACK_GRID_HEIGHT);

    {
        auto* bg = new BgPanel;
        bg->box.size    = box.size;
        bg->rowsPerCol  = ROWS_PER_COL;
        bg->slotH_px    = mm2px(SLOT_H);
        bg->slotsTop_px = mm2px(SLOTS_TOP);
        bg->col0X_px    = mm2px(COL_X[0]);
        bg->col1X_px    = mm2px(COL_X[1]);
        bg->colW_px     = mm2px(COL_W);
        addChild(bg);
    }

    {
        auto* lbl = new TinyLabel;
        lbl->text     = "SYSEX BANK";
        lbl->color    = INK;
        lbl->fontSize = 9.5f;
        lbl->align    = NVG_ALIGN_CENTER;
        lbl->box.pos  = Vec(0, mm2px(0.8f));
        lbl->box.size = Vec(box.size.x, mm2px(TITLE_H));
        addChild(lbl);
    }

    for (int i = 0; i < SysExBank::NUM_SLOTS; i++) {
        int col = i / ROWS_PER_COL;
        int row = i % ROWS_PER_COL;
        float colX = COL_X[col];
        float cy = SLOTS_TOP + (row + 0.5f) * SLOT_H;

        {
            auto* lbl  = new TinyLabel;
            lbl->text  = string::f("%2d", i + 1);
            lbl->color = INK;
            lbl->fontSize = 8.f;
            lbl->box.pos  = Vec(mm2px(colX + CX_NUM), mm2px(cy - SLOT_H * 0.4f));
            lbl->box.size = Vec(mm2px(5.f), mm2px(SLOT_H * 0.8f));
            addChild(lbl);
        }

        addInput(createInputCentered<PJ301MPort>(
            mm2px(Vec(colX + CX_JACK, cy)), module, SysExBank::TRIG_INPUT + i
        ));

        {
            auto* tf        = new SysExTextField;
            tf->module      = module;
            tf->slotIndex   = i;
            tf->placeholder = "F0 ... F7";
            tf->multiline   = false;
            tf->box.pos     = Vec(mm2px(colX + CX_FIELD), mm2px(cy - FIELD_H * 0.5f));
            tf->box.size    = Vec(mm2px(FIELD_W), mm2px(FIELD_H));
            if (module) tf->text = module->getSlotString(i);
            addChild(tf);
        }

        addParam(createParamCentered<TL1105>(
            mm2px(Vec(colX + CX_BTN, cy)), module, SysExBank::SEND_PARAM + i
        ));

        addChild(createLightCentered<SmallLight<GreenLight>>(
            mm2px(Vec(colX + CX_LED, cy)), module, SysExBank::SEND_LIGHT + i
        ));
    }

    // MIDI output selector: one row, driver+device combined into a single
    // click-to-open menu (see MidiOutChoice above). Raw SysEx bytes carry no
    // channel nibble, so appendMidiMenu's channel entry has no effect here,
    // but that's how the whole ecosystem exposes MIDI channel regardless.
    {
        float dispY = SLOTS_BOT + 1.5f;
        float dispW = MODULE_W_MM - 2.f * MARGIN;
        float dispH = MIDI_H - 3.f;

        // LedDisplayChoice only draws its text -- the dark background box comes from the
        // parent LedDisplay, so it must stay wrapped in one or the text floats unreadably
        // on the panel's light background.
        auto* display = new app::LedDisplay;
        display->box.pos  = mm2px(Vec(MARGIN, dispY));
        display->box.size = mm2px(Vec(dispW, dispH));
        addChild(display);

        auto* midiChoice = new MidiOutChoice;
        midiChoice->box.pos  = Vec(0, 0);
        midiChoice->box.size = display->box.size;
        midiChoice->port = module ? &module->midiOutput : nullptr;
        display->addChild(midiChoice);
    }
}
