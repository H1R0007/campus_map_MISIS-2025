#pragma once
#include <string>
#include <emscripten/bind.h>

class SimpleBridge {
public:
    static std::string getVersion();
    static std::string getBuildings();
    static std::string calculateRoute(const std::string& from, const std::string& to);
};

EMSCRIPTEN_BINDINGS(SimpleBridge) {
    emscripten::class_<SimpleBridge>("SimpleBridge")
        .class_function("getVersion", &SimpleBridge::getVersion)
        .class_function("getBuildings", &SimpleBridge::getBuildings)
        .class_function("calculateRoute", &SimpleBridge::calculateRoute);
}