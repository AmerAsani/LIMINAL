// Minimaler COM-Zeiger (vermeidet die WRL-Abhaengigkeit).
#pragma once

#include <cstddef>

namespace lim {

template <class T>
class Com {
public:
    Com() = default;
    Com(std::nullptr_t) {}
    ~Com() { reset(); }
    Com(const Com& o) : p_(o.p_) {
        if (p_) p_->AddRef();
    }
    Com(Com&& o) noexcept : p_(o.p_) { o.p_ = nullptr; }
    Com& operator=(const Com& o) {
        if (this != &o) {
            if (o.p_) o.p_->AddRef();
            reset();
            p_ = o.p_;
        }
        return *this;
    }
    Com& operator=(Com&& o) noexcept {
        if (this != &o) {
            reset();
            p_ = o.p_;
            o.p_ = nullptr;
        }
        return *this;
    }
    void reset() {
        if (p_) p_->Release();
        p_ = nullptr;
    }
    T* get() const { return p_; }
    T* operator->() const { return p_; }
    T** put() {
        reset();
        return &p_;
    }
    T* const* addr() const { return &p_; }
    explicit operator bool() const { return p_ != nullptr; }

private:
    T* p_ = nullptr;
};

}  // namespace lim
