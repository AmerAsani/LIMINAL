// Kleiner JSON-Leser/-Schreiber fuer Spieldaten, Einstellungen und Spielstaende.
// Objekte behalten die Reihenfolge ihrer Schluessel (wichtig fuer Kataloge,
// deren Reihenfolge die Weltgenerierung beeinflusst).
#pragma once

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace lim {

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool b) : v_(b) {}
    Json(int i) : v_((double)i) {}
    Json(long long i) : v_((double)i) {}
    Json(double d) : v_(d) {}
    Json(const char* s) : v_(std::string(s)) {}
    Json(std::string s) : v_(std::move(s)) {}
    Json(Array a) : v_(std::move(a)) {}
    Json(Object o) : v_(std::move(o)) {}

    static Json array() { return Json(Array{}); }
    static Json object() { return Json(Object{}); }

    bool isNull() const { return std::holds_alternative<std::monostate>(v_); }
    bool isBool() const { return std::holds_alternative<bool>(v_); }
    bool isNumber() const { return std::holds_alternative<double>(v_); }
    bool isString() const { return std::holds_alternative<std::string>(v_); }
    bool isArray() const { return std::holds_alternative<Array>(v_); }
    bool isObject() const { return std::holds_alternative<Object>(v_); }

    bool asBool(bool def = false) const { return isBool() ? std::get<bool>(v_) : def; }
    double asNumber(double def = 0.0) const { return isNumber() ? std::get<double>(v_) : def; }
    float asFloat(float def = 0.0f) const { return isNumber() ? (float)std::get<double>(v_) : def; }
    long long asInt(long long def = 0) const { return isNumber() ? (long long)std::get<double>(v_) : def; }
    const std::string& asString() const;
    std::string asString(const std::string& def) const { return isString() ? std::get<std::string>(v_) : def; }

    const Array& items() const;
    Array& items();
    const Object& members() const;
    Object& members();

    std::size_t size() const;
    const Json& operator[](std::size_t i) const;
    // Objektzugriff; fehlende Schluessel liefern einen Null-Wert.
    const Json& operator[](std::string_view key) const;
    const Json* find(std::string_view key) const;
    bool has(std::string_view key) const { return find(key) != nullptr; }
    // Setzt/ersetzt einen Schluessel (macht einen Null-Wert zum Objekt).
    Json& set(std::string key, Json value);
    Json& push(Json value);

    std::string dump(bool pretty = false) const;
    // Liefert nullopt und eine Fehlermeldung bei ungueltigem JSON.
    static std::optional<Json> parse(std::string_view text, std::string* error = nullptr);

private:
    void dumpTo(std::string& out, bool pretty, int indent) const;
    std::variant<std::monostate, bool, double, std::string, Array, Object> v_;
};

// Laedt eine JSON-Datei; bei Fehlern Protokolleintrag und nullopt.
std::optional<Json> loadJsonFile(const std::string& path);

}  // namespace lim
