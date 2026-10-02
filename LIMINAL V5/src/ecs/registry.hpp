// Entity-Component-System (Sparse Sets).
//
// Eine Entity ist nur eine ID. Komponenten sind einfache Datenstrukturen und
// liegen je Typ dicht gepackt im Speicher (cachefreundlich, schnelle
// Iteration auch bei sehr vielen Entities). Systeme sind Funktionen, die ueber
// alle Entities mit bestimmten Komponenten laufen.
//
//   ecs::Registry reg;
//   auto e = reg.create();
//   reg.add<Transform>(e, ...);
//   reg.each<Transform, Pickup>([](ecs::Entity e, Transform& t, Pickup& p) { ... });
#pragma once

#include <cstdint>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

namespace lim::ecs {

using Entity = std::uint32_t;  // 24 Bit Index + 8 Bit Version
constexpr Entity kNull = 0xFFFFFFFFu;
inline std::uint32_t indexOf(Entity e) { return e & 0xFFFFFFu; }
inline std::uint32_t versionOf(Entity e) { return e >> 24; }

namespace detail {
inline std::uint32_t nextTypeId() {
    static std::uint32_t id = 0;
    return id++;
}
template <class C>
std::uint32_t typeId() {
    static const std::uint32_t id = nextTypeId();
    return id;
}

struct PoolBase {
    virtual ~PoolBase() = default;
    virtual void remove(Entity e) = 0;
    virtual bool contains(Entity e) const = 0;
    virtual void clear() = 0;
    virtual std::size_t size() const = 0;
};

template <class C>
struct Pool final : PoolBase {
    static constexpr std::uint32_t kEmpty = 0xFFFFFFFFu;
    std::vector<std::uint32_t> sparse;  // Entity-Index -> dichter Index
    std::vector<Entity> dense;
    std::vector<C> data;

    bool contains(Entity e) const override {
        std::uint32_t i = indexOf(e);
        return i < sparse.size() && sparse[i] != kEmpty && dense[sparse[i]] == e;
    }
    C* get(Entity e) { return contains(e) ? &data[sparse[indexOf(e)]] : nullptr; }
    template <class... A>
    C& emplace(Entity e, A&&... args) {
        std::uint32_t i = indexOf(e);
        if (i >= sparse.size()) sparse.resize(i + 1, kEmpty);
        if (sparse[i] != kEmpty && dense[sparse[i]] == e) {
            data[sparse[i]] = C{std::forward<A>(args)...};
            return data[sparse[i]];
        }
        sparse[i] = (std::uint32_t)dense.size();
        dense.push_back(e);
        data.push_back(C{std::forward<A>(args)...});
        return data.back();
    }
    void remove(Entity e) override {
        if (!contains(e)) return;
        std::uint32_t i = indexOf(e), d = sparse[i], last = (std::uint32_t)dense.size() - 1;
        if (d != last) {
            dense[d] = dense[last];
            data[d] = std::move(data[last]);
            sparse[indexOf(dense[d])] = d;
        }
        dense.pop_back();
        data.pop_back();
        sparse[i] = kEmpty;
    }
    void clear() override {
        sparse.clear();
        dense.clear();
        data.clear();
    }
    std::size_t size() const override { return dense.size(); }
};
}  // namespace detail

class Registry {
public:
    Entity create() {
        std::uint32_t idx;
        if (!free_.empty()) {
            idx = free_.back();
            free_.pop_back();
        } else {
            idx = (std::uint32_t)versions_.size();
            versions_.push_back(0);
        }
        ++alive_;
        return (versions_[idx] << 24) | idx;
    }
    bool alive(Entity e) const {
        std::uint32_t i = indexOf(e);
        return e != kNull && i < versions_.size() && versions_[i] == versionOf(e);
    }
    void destroy(Entity e) {
        if (!alive(e)) return;
        for (auto& p : pools_)
            if (p) p->remove(e);
        std::uint32_t i = indexOf(e);
        versions_[i] = (versions_[i] + 1) & 0xFFu;
        free_.push_back(i);
        --alive_;
    }
    void clear() {
        for (auto& p : pools_)
            if (p) p->clear();
        versions_.clear();
        free_.clear();
        alive_ = 0;
    }
    std::size_t count() const { return alive_; }

    template <class C, class... A>
    C& add(Entity e, A&&... args) {
        return pool<C>().emplace(e, std::forward<A>(args)...);
    }
    template <class C>
    C* get(Entity e) {
        return pool<C>().get(e);
    }
    template <class C>
    bool has(Entity e) {
        return pool<C>().contains(e);
    }
    template <class C>
    void remove(Entity e) {
        pool<C>().remove(e);
    }
    template <class C>
    std::size_t countOf() {
        return pool<C>().size();
    }

    // Ruft fn(entity, C1&, C2&, ...) fuer alle Entities mit allen Komponenten auf.
    // Iteriert ueber die kleinste beteiligte Menge.
    template <class First, class... Rest, class Fn>
    void each(Fn&& fn) {
        auto& base = pool<First>();
        // Kopie der IDs: fn darf Entities zerstoeren
        std::vector<Entity> ids = base.dense;
        for (Entity e : ids) {
            if (!alive(e)) continue;
            First* f = base.get(e);
            if (!f) continue;
            if constexpr (sizeof...(Rest) == 0) {
                fn(e, *f);
            } else {
                auto rest = std::make_tuple(pool<Rest>().get(e)...);
                bool all = std::apply([](auto*... p) { return ((p != nullptr) && ...); }, rest);
                if (all) std::apply([&](auto*... p) { fn(e, *f, *p...); }, rest);
            }
        }
    }

private:
    template <class C>
    detail::Pool<C>& pool() {
        std::uint32_t id = detail::typeId<C>();
        if (id >= pools_.size()) pools_.resize(id + 1);
        if (!pools_[id]) pools_[id] = std::make_unique<detail::Pool<C>>();
        return *static_cast<detail::Pool<C>*>(pools_[id].get());
    }

    std::vector<std::unique_ptr<detail::PoolBase>> pools_;
    std::vector<std::uint32_t> versions_;
    std::vector<std::uint32_t> free_;
    std::size_t alive_ = 0;
};

}  // namespace lim::ecs
