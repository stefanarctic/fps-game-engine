#include "Forge/Core/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Forge {

namespace {
const Json& nullJson() {
    static const Json n;
    return n;
}
const std::string& emptyString() {
    static const std::string s;
    return s;
}

void appendEscaped(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out += (char)c;
            }
        }
    }
    out += '"';
}

void appendNumber(std::string& out, double v) {
    if (!std::isfinite(v)) { out += "0"; return; }
    char buf[64];
    if (v == std::floor(v) && std::fabs(v) < 1e15) std::snprintf(buf, sizeof(buf), "%.0f", v);
    else std::snprintf(buf, sizeof(buf), "%.9g", v);
    out += buf;
}

// ---------------------------------------------------------------------------
class Parser {
public:
    Parser(const std::string& text) : m_Text(text) {}

    bool parse(Json& out, std::string* error) {
        skipWs();
        if (!parseValue(out, 0)) {
            if (error) *error = m_Error + " at offset " + std::to_string(m_Pos);
            return false;
        }
        skipWs();
        if (m_Pos != m_Text.size()) {
            if (error) *error = "Trailing characters at offset " + std::to_string(m_Pos);
            return false;
        }
        return true;
    }

private:
    bool fail(const char* msg) { m_Error = msg; return false; }

    void skipWs() {
        while (m_Pos < m_Text.size()) {
            char c = m_Text[m_Pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { m_Pos++; continue; }
            // Tolerate // comments (handy for hand-edited files).
            if (c == '/' && m_Pos + 1 < m_Text.size() && m_Text[m_Pos + 1] == '/') {
                while (m_Pos < m_Text.size() && m_Text[m_Pos] != '\n') m_Pos++;
                continue;
            }
            break;
        }
    }

    bool parseValue(Json& out, int depth) {
        if (depth > 256) return fail("Nesting too deep");
        if (m_Pos >= m_Text.size()) return fail("Unexpected end of input");
        char c = m_Text[m_Pos];
        if (c == '{') return parseObject(out, depth);
        if (c == '[') return parseArray(out, depth);
        if (c == '"') {
            std::string s;
            if (!parseString(s)) return false;
            out = Json(s);
            return true;
        }
        if (c == 't' && m_Text.compare(m_Pos, 4, "true") == 0) { m_Pos += 4; out = Json(true); return true; }
        if (c == 'f' && m_Text.compare(m_Pos, 5, "false") == 0) { m_Pos += 5; out = Json(false); return true; }
        if (c == 'n' && m_Text.compare(m_Pos, 4, "null") == 0) { m_Pos += 4; out = Json(); return true; }
        if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(out);
        return fail("Unexpected character");
    }

    bool parseNumber(Json& out) {
        const char* start = m_Text.c_str() + m_Pos;
        char* end = nullptr;
        double v = std::strtod(start, &end);
        if (end == start) return fail("Invalid number");
        m_Pos += (size_t)(end - start);
        out = Json(v);
        return true;
    }

    static void appendUtf8(std::string& s, uint32_t cp) {
        if (cp < 0x80) s += (char)cp;
        else if (cp < 0x800) { s += (char)(0xC0 | (cp >> 6)); s += (char)(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { s += (char)(0xE0 | (cp >> 12)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
        else { s += (char)(0xF0 | (cp >> 18)); s += (char)(0x80 | ((cp >> 12) & 0x3F)); s += (char)(0x80 | ((cp >> 6) & 0x3F)); s += (char)(0x80 | (cp & 0x3F)); }
    }

    bool parseHex4(uint32_t& v) {
        if (m_Pos + 4 > m_Text.size()) return fail("Bad unicode escape");
        v = 0;
        for (int i = 0; i < 4; ++i) {
            char h = m_Text[m_Pos++];
            v <<= 4;
            if (h >= '0' && h <= '9') v |= (uint32_t)(h - '0');
            else if (h >= 'a' && h <= 'f') v |= (uint32_t)(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') v |= (uint32_t)(h - 'A' + 10);
            else return fail("Bad unicode escape");
        }
        return true;
    }

    bool parseString(std::string& out) {
        m_Pos++; // opening quote
        while (m_Pos < m_Text.size()) {
            char c = m_Text[m_Pos++];
            if (c == '"') return true;
            if (c == '\\') {
                if (m_Pos >= m_Text.size()) break;
                char e = m_Text[m_Pos++];
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
                    uint32_t cp;
                    if (!parseHex4(cp)) return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF && m_Pos + 6 <= m_Text.size() && m_Text[m_Pos] == '\\' && m_Text[m_Pos + 1] == 'u') {
                        m_Pos += 2;
                        uint32_t lo;
                        if (!parseHex4(lo)) return false;
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    appendUtf8(out, cp);
                    break;
                }
                default: return fail("Bad escape sequence");
                }
            } else {
                out += c;
            }
        }
        return fail("Unterminated string");
    }

    bool parseArray(Json& out, int depth) {
        m_Pos++;
        out = Json::array();
        skipWs();
        if (m_Pos < m_Text.size() && m_Text[m_Pos] == ']') { m_Pos++; return true; }
        while (true) {
            skipWs();
            Json item;
            if (!parseValue(item, depth + 1)) return false;
            out.push(std::move(item));
            skipWs();
            if (m_Pos >= m_Text.size()) return fail("Unterminated array");
            char c = m_Text[m_Pos++];
            if (c == ']') return true;
            if (c != ',') return fail("Expected ',' or ']'");
        }
    }

    bool parseObject(Json& out, int depth) {
        m_Pos++;
        out = Json::object();
        skipWs();
        if (m_Pos < m_Text.size() && m_Text[m_Pos] == '}') { m_Pos++; return true; }
        while (true) {
            skipWs();
            if (m_Pos >= m_Text.size() || m_Text[m_Pos] != '"') return fail("Expected key string");
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (m_Pos >= m_Text.size() || m_Text[m_Pos] != ':') return fail("Expected ':'");
            m_Pos++;
            skipWs();
            Json value;
            if (!parseValue(value, depth + 1)) return false;
            out[key] = std::move(value);
            skipWs();
            if (m_Pos >= m_Text.size()) return fail("Unterminated object");
            char c = m_Text[m_Pos++];
            if (c == '}') return true;
            if (c != ',') return fail("Expected ',' or '}'");
        }
    }

    const std::string& m_Text;
    size_t m_Pos = 0;
    std::string m_Error;
};
} // namespace

Json Json::array() {
    Json j;
    j.m_Type = Type::Array;
    return j;
}

Json Json::object() {
    Json j;
    j.m_Type = Type::Object;
    return j;
}

const std::string& Json::asString() const { return m_Type == Type::String ? m_String : emptyString(); }

size_t Json::size() const {
    if (m_Type == Type::Array) return m_Array.size();
    if (m_Type == Type::Object) return m_Object.size();
    return 0;
}

const Json& Json::operator[](size_t index) const {
    if (m_Type != Type::Array || index >= m_Array.size()) return nullJson();
    return m_Array[index];
}

Json& Json::operator[](size_t index) {
    if (m_Type != Type::Array) { m_Type = Type::Array; m_Array.clear(); }
    if (index >= m_Array.size()) m_Array.resize(index + 1);
    return m_Array[index];
}

void Json::push(Json value) {
    if (m_Type != Type::Array) { m_Type = Type::Array; m_Array.clear(); }
    m_Array.push_back(std::move(value));
}

bool Json::has(const std::string& key) const {
    if (m_Type != Type::Object) return false;
    for (const Member& m : m_Object)
        if (m.first == key) return true;
    return false;
}

const Json& Json::operator[](const std::string& key) const {
    if (m_Type != Type::Object) return nullJson();
    for (const Member& m : m_Object)
        if (m.first == key) return m.second;
    return nullJson();
}

Json& Json::operator[](const std::string& key) {
    if (m_Type != Type::Object) { m_Type = Type::Object; m_Object.clear(); }
    for (Member& m : m_Object)
        if (m.first == key) return m.second;
    m_Object.emplace_back(key, Json());
    return m_Object.back().second;
}

std::string Json::dump(int indent) const {
    std::string out;
    dumpImpl(out, indent, 0);
    if (indent > 0) out += '\n';
    return out;
}

void Json::dumpImpl(std::string& out, int indent, int depth) const {
    auto newline = [&](int d) {
        if (indent <= 0) return;
        out += '\n';
        out.append((size_t)(indent * d), ' ');
    };

    switch (m_Type) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += m_Bool ? "true" : "false"; break;
    case Type::Number: appendNumber(out, m_Number); break;
    case Type::String: appendEscaped(out, m_String); break;
    case Type::Array: {
        if (m_Array.empty()) { out += "[]"; break; }
        // Short arrays of plain numbers (vectors, colors) stay on one line.
        bool compact = m_Array.size() <= 16;
        for (const Json& j : m_Array)
            if (!j.isNumber() && !j.isBool()) { compact = false; break; }
        out += '[';
        for (size_t i = 0; i < m_Array.size(); ++i) {
            if (i) out += compact ? ", " : ",";
            if (!compact) newline(depth + 1);
            m_Array[i].dumpImpl(out, indent, depth + 1);
        }
        if (!compact) newline(depth);
        out += ']';
        break;
    }
    case Type::Object: {
        if (m_Object.empty()) { out += "{}"; break; }
        out += '{';
        for (size_t i = 0; i < m_Object.size(); ++i) {
            if (i) out += ',';
            newline(depth + 1);
            appendEscaped(out, m_Object[i].first);
            out += indent > 0 ? ": " : ":";
            m_Object[i].second.dumpImpl(out, indent, depth + 1);
        }
        newline(depth);
        out += '}';
        break;
    }
    }
}

bool Json::parse(const std::string& text, Json& out, std::string* error) {
    Parser p(text);
    return p.parse(out, error);
}

} // namespace Forge
