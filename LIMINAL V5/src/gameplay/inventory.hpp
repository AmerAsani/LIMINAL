// Inventar im Stil von Minecraft (Port von V4, inventory.py):
// 36 Plaetze, 0-8 Schnellleiste, 9-35 Hauptinventar. Gleiche Items stapeln sich.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/json.hpp"

namespace lim::game {

class Content;

struct Stack {
    std::string kind;
    int count = 0;
};

class Inventory {
public:
    static constexpr int kSize = 36;
    static constexpr int kHotbar = 9;

    explicit Inventory(const Content* content = nullptr);

    bool add(const std::string& kind);
    std::optional<std::string> take(int idx);  // ein Item aus dem Platz
    void swap(int a, int b);
    int count(const std::string& kind = "") const;
    int maxStack(const std::string& kind) const;

    Json toJson() const;
    void fromJson(const Json& j);

    std::vector<std::optional<Stack>> slots;
    int selected = 0;

private:
    const Content* content_;
};

}  // namespace lim::game
