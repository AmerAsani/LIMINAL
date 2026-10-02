#include "gameplay/inventory.hpp"

#include <algorithm>

#include "gameplay/content.hpp"

namespace lim::game {

Inventory::Inventory(const Content* content) : slots(kSize), content_(content) {}

int Inventory::maxStack(const std::string& kind) const {
    if (content_)
        if (const ItemDef* d = content_->item(kind)) return std::max(1, d->maxStack);
    return 16;
}

bool Inventory::add(const std::string& kind) {
    int ms = maxStack(kind);
    for (auto& s : slots)
        if (s && s->kind == kind && s->count < ms) {
            ++s->count;
            return true;
        }
    for (auto& s : slots)
        if (!s) {
            s = Stack{kind, 1};
            return true;
        }
    return false;
}

std::optional<std::string> Inventory::take(int idx) {
    if (idx < 0 || idx >= kSize || !slots[(size_t)idx]) return std::nullopt;
    auto& s = slots[(size_t)idx];
    std::string kind = s->kind;
    if (--s->count <= 0) s.reset();
    return kind;
}

void Inventory::swap(int a, int b) {
    if (a == b || a < 0 || b < 0 || a >= kSize || b >= kSize) return;
    auto& sa = slots[(size_t)a];
    auto& sb = slots[(size_t)b];
    if (sa && sb && sa->kind == sb->kind) {
        int move = std::min(maxStack(sa->kind) - sb->count, sa->count);
        sb->count += move;
        sa->count -= move;
        if (sa->count <= 0) sa.reset();
        return;
    }
    std::swap(sa, sb);
}

int Inventory::count(const std::string& kind) const {
    int n = 0;
    for (const auto& s : slots)
        if (s && (kind.empty() || s->kind == kind)) n += s->count;
    return n;
}

Json Inventory::toJson() const {
    Json a = Json::array();
    for (const auto& s : slots) {
        if (s) {
            Json e = Json::array();
            e.push(s->kind);
            e.push(s->count);
            a.push(std::move(e));
        } else {
            a.push(Json());
        }
    }
    return a;
}

void Inventory::fromJson(const Json& j) {
    slots.assign(kSize, std::nullopt);
    for (size_t i = 0; i < std::min<size_t>(j.size(), kSize); ++i) {
        const Json& e = j[i];
        if (!e.isArray() || e.size() < 2) continue;
        std::string kind = e[0].asString();
        int n = (int)e[1].asInt();
        if (kind.empty() || n <= 0) continue;
        if (content_ && !content_->item(kind)) continue;
        slots[i] = Stack{kind, std::min(n, maxStack(kind))};
    }
}

}  // namespace lim::game
