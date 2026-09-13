#include "plugin.hpp"

Plugin* pluginInstance = nullptr;
// modelSysExBank is defined in module.cpp via createModel<>

void init(Plugin* p) {
    pluginInstance = p;
    p->addModel(modelSysExBank);
}
