#include "core/json.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <format>

#include "core/log.hpp"
#include "core/paths.hpp"

namespace lim {

static const Json kNull;
static const std::string kEmpty;
static const Json::Array kEmptyArray;
static const Json::Object kEmptyObject;

const std::string& Json::asString() const { return isString() ? std::get<std::string>(v_) : kEmpty; }
const Json::Array& Json::items() const { return isArray() ? std::get<Array>(v_) : kEmptyArray; }
Json::Array& Json::items() {
    if (!isArray()) v_ = Array{};
    return std::get<Array>(v_);
}
const Json::Object& Json::members() const { return isObject() ? std::get<Object>(v_) : kEmptyObject; }
Json::Object& Json::members() {
    if (!isObject()) v_ = Object{};
    return std::get<Object>(v_);
}

std::size_t Json::size() const {
    if (isArray()) return std::get<Array>(v_).size();
    if (isObject()) return std::get<Object>(v_).size();
    return 0;
}

const Json& Json::operator[](std::size_t i) const {
    const auto& a = items();
    return i < a.size() ? a[i] : kNull;
}

const Json* Json::find(std::string_view key) const {
    if (!isObject()) return nullptr;
    for (const auto& [k, v] : std::get<Object>(v_))
        if (k == key) return &v;
    return nullptr;
}

const Json& Json::operator[](std::string_view key) const {
    const Json* j = find(key);
    return j ? *j : kNull;
}

Json& Json::set(std::string key, Json value) {
    auto& obj = members();
    for (auto& [k, v] : obj)
        if (k == key) {
            v = std::move(value);
            return v;
        }
    obj.emplace_back(std::move(key), std::move(value));
    return obj.back().second;
}

Json& Json::push(Json value) {
    auto& a = items();
    a.push_back(std::move(value));
    return a.back();
}

// --- Schreiben -----------------------------------------------------------------------
static void escapeTo(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) out += std::format("\\u{:04x}", c);
                else out += (char)c;
        }
    }
    out += '"';
}

static void numberTo(std::string& out, double d) {
    if (!std::isfinite(d)) {
        out += "0";
        return;
    }
    if (d == std::floor(d) && std::fabs(d) < 9.007e15) {
        out += std::format("{}", (long long)d);
        return;
    }
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof(buf), d);  // kuerzeste exakte Darstellung
    out.append(buf, res.ptr);
}

void Json::dumpTo(std::string& out, bool pretty, int indent) const {
    auto nl = [&](int ind) {
        if (pretty) {
            out += '\n';
            out.append((size_t)ind * 2, ' ');
        }
    };
    if (isNull()) out += "null";
    else if (isBool()) out += std::get<bool>(v_) ? "true" : "false";
    else if (isNumber()) numberTo(out, std::get<double>(v_));
    else if (isString()) escapeTo(out, std::get<std::string>(v_));
    else if (isArray()) {
        const auto& a = std::get<Array>(v_);
        out += '[';
        for (size_t i = 0; i < a.size(); ++i) {
            if (i) out += ',';
            nl(indent + 1);
            a[i].dumpTo(out, pretty, indent + 1);
        }
        if (!a.empty()) nl(indent);
        out += ']';
    } else {
        const auto& o = std::get<Object>(v_);
        out += '{';
        for (size_t i = 0; i < o.size(); ++i) {
            if (i) out += ',';
            nl(indent + 1);
            escapeTo(out, o[i].first);
            out += pretty ? ": " : ":";
            o[i].second.dumpTo(out, pretty, indent + 1);
        }
        if (!o.empty()) nl(indent);
        out += '}';
    }
}

std::string Json::dump(bool pretty) const {
    std::string out;
    dumpTo(out, pretty, 0);
    return out;
}

// --- Lesen ---------------------------------------------------------------------------
namespace {

struct Parser {
    std::string_view s;
    size_t i = 0;
    std::string err;

    bool fail(const char* msg) {
        if (err.empty()) {
            int line = 1, col = 1;
            for (size_t k = 0; k < i && k < s.size(); ++k) {
                if (s[k] == '\n') {
                    ++line;
                    col = 1;
                } else ++col;
            }
            err = std::format("{} (Zeile {}, Spalte {})", msg, line, col);
        }
        return false;
    }
    void ws() {
        while (i < s.size()) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++i;
            else if (c == '/' && i + 1 < s.size() && s[i + 1] == '/') {  // Kommentare in Datendateien erlaubt
                while (i < s.size() && s[i] != '\n') ++i;
            } else break;
        }
    }
    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += (char)cp;
        else if (cp < 0x800) {
            out += (char)(0xC0 | (cp >> 6));
            out += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += (char)(0xE0 | (cp >> 12));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        } else {
            out += (char)(0xF0 | (cp >> 18));
            out += (char)(0x80 | ((cp >> 12) & 0x3F));
            out += (char)(0x80 | ((cp >> 6) & 0x3F));
            out += (char)(0x80 | (cp & 0x3F));
        }
    }
    bool hex4(unsigned& cp) {
        if (i + 4 > s.size()) return fail("unvollstaendiges \\u");
        cp = 0;
        for (int k = 0; k < 4; ++k) {
            char c = s[i++];
            cp <<= 4;
            if (c >= '0' && c <= '9') cp |= c - '0';
            else if (c >= 'a' && c <= 'f') cp |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') cp |= c - 'A' + 10;
            else return fail("ungueltiges \\u");
        }
        return true;
    }
    bool str(std::string& out) {
        ++i;  // "
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (i >= s.size()) break;
            char e = s[i++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp < 0xDC00 && i + 6 <= s.size() && s[i] == '\\' && s[i + 1] == 'u') {
                        i += 2;
                        unsigned lo;
                        if (!hex4(lo)) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    utf8(out, cp);
                    break;
                }
                default: return fail("ungueltige Escape-Sequenz");
            }
        }
        return fail("Zeichenkette nicht beendet");
    }
    bool value(Json& out, int depth) {
        if (depth > 200) return fail("zu tief verschachtelt");
        ws();
        if (i >= s.size()) return fail("unerwartetes Ende");
        char c = s[i];
        if (c == '{') {
            ++i;
            Json::Object obj;
            ws();
            if (i < s.size() && s[i] == '}') {
                ++i;
                out = Json(std::move(obj));
                return true;
            }
            while (true) {
                ws();
                if (i >= s.size() || s[i] != '"') return fail("Schluessel erwartet");
                std::string key;
                if (!str(key)) return false;
                ws();
                if (i >= s.size() || s[i] != ':') return fail("':' erwartet");
                ++i;
                Json v;
                if (!value(v, depth + 1)) return false;
                obj.emplace_back(std::move(key), std::move(v));
                ws();
                if (i < s.size() && s[i] == ',') {
                    ++i;
                    ws();
                    if (i < s.size() && s[i] == '}') {  // abschliessendes Komma tolerieren
                        ++i;
                        break;
                    }
                    continue;
                }
                if (i < s.size() && s[i] == '}') {
                    ++i;
                    break;
                }
                return fail("',' oder '}' erwartet");
            }
            out = Json(std::move(obj));
            return true;
        }
        if (c == '[') {
            ++i;
            Json::Array arr;
            ws();
            if (i < s.size() && s[i] == ']') {
                ++i;
                out = Json(std::move(arr));
                return true;
            }
            while (true) {
                Json v;
                if (!value(v, depth + 1)) return false;
                arr.push_back(std::move(v));
                ws();
                if (i < s.size() && s[i] == ',') {
                    ++i;
                    ws();
                    if (i < s.size() && s[i] == ']') {
                        ++i;
                        break;
                    }
                    continue;
                }
                if (i < s.size() && s[i] == ']') {
                    ++i;
                    break;
                }
                return fail("',' oder ']' erwartet");
            }
            out = Json(std::move(arr));
            return true;
        }
        if (c == '"') {
            std::string str_;
            if (!str(str_)) return false;
            out = Json(std::move(str_));
            return true;
        }
        if (s.substr(i, 4) == "true") {
            i += 4;
            out = Json(true);
            return true;
        }
        if (s.substr(i, 5) == "false") {
            i += 5;
            out = Json(false);
            return true;
        }
        if (s.substr(i, 4) == "null") {
            i += 4;
            out = Json();
            return true;
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            size_t start = i;
            ++i;
            while (i < s.size() && (std::isdigit((unsigned char)s[i]) || s[i] == '.' || s[i] == 'e' || s[i] == 'E' ||
                                    s[i] == '+' || s[i] == '-'))
                ++i;
            double d = 0;
            auto res = std::from_chars(s.data() + start, s.data() + i, d);
            if (res.ec != std::errc() || res.ptr != s.data() + i) return fail("ungueltige Zahl");
            out = Json(d);
            return true;
        }
        return fail("unerwartetes Zeichen");
    }
};

}  // namespace

std::optional<Json> Json::parse(std::string_view text, std::string* error) {
    Parser p{text};
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF)
        p.i = 3;  // UTF-8-BOM
    Json out;
    if (!p.value(out, 0)) {
        if (error) *error = p.err;
        return std::nullopt;
    }
    p.ws();
    if (p.i != text.size()) {
        p.fail("Zusaetzliche Zeichen nach dem Wert");
        if (error) *error = p.err;
        return std::nullopt;
    }
    return out;
}

std::optional<Json> loadJsonFile(const std::string& path) {
    auto text = paths::readFile(path);
    if (!text) {
        log::warn("JSON-Datei nicht gefunden: {}", path);
        return std::nullopt;
    }
    std::string err;
    auto j = Json::parse(*text, &err);
    if (!j) log::error("JSON-Fehler in {}: {}", path, err);
    return j;
}

}  // namespace lim
