#pragma once
#include "Forge/Core/Base.h"

namespace Forge {

// Minimal JSON DOM used for scenes, materials and glTF parsing.
// Objects keep insertion order so saved files diff nicely.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    using Member = std::pair<std::string, Json>;

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool b) : m_Type(Type::Bool), m_Bool(b) {}
    Json(int v) : m_Type(Type::Number), m_Number((double)v) {}
    Json(unsigned v) : m_Type(Type::Number), m_Number((double)v) {}
    Json(int64_t v) : m_Type(Type::Number), m_Number((double)v) {}
    Json(uint64_t v) : m_Type(Type::Number), m_Number((double)v) {}
    Json(float v) : m_Type(Type::Number), m_Number((double)v) {}
    Json(double v) : m_Type(Type::Number), m_Number(v) {}
    Json(const char* s) : m_Type(Type::String), m_String(s ? s : "") {}
    Json(const std::string& s) : m_Type(Type::String), m_String(s) {}

    static Json array();
    static Json object();

    Type type() const { return m_Type; }
    bool isNull() const { return m_Type == Type::Null; }
    bool isBool() const { return m_Type == Type::Bool; }
    bool isNumber() const { return m_Type == Type::Number; }
    bool isString() const { return m_Type == Type::String; }
    bool isArray() const { return m_Type == Type::Array; }
    bool isObject() const { return m_Type == Type::Object; }

    bool asBool(bool def = false) const { return m_Type == Type::Bool ? m_Bool : (m_Type == Type::Number ? m_Number != 0.0 : def); }
    double asNumber(double def = 0.0) const { return m_Type == Type::Number ? m_Number : (m_Type == Type::Bool ? (m_Bool ? 1.0 : 0.0) : def); }
    float asFloat(float def = 0.0f) const { return (float)asNumber(def); }
    int asInt(int def = 0) const { return (int)asNumber(def); }
    const std::string& asString() const;
    std::string asString(const std::string& def) const { return m_Type == Type::String ? m_String : def; }

    // Array access
    size_t size() const;
    const Json& operator[](size_t index) const;
    Json& operator[](size_t index);
    void push(Json value);
    const std::vector<Json>& items() const { return m_Array; }

    // Object access. Const lookup of a missing key returns a shared null value.
    bool has(const std::string& key) const;
    const Json& operator[](const std::string& key) const;
    Json& operator[](const std::string& key); // inserts when missing (converts null -> object)
    const std::vector<Member>& members() const { return m_Object; }

    std::string dump(int indent = 2) const;
    static bool parse(const std::string& text, Json& out, std::string* error = nullptr);

private:
    void dumpImpl(std::string& out, int indent, int depth) const;

    Type m_Type = Type::Null;
    bool m_Bool = false;
    double m_Number = 0.0;
    std::string m_String;
    std::vector<Json> m_Array;
    std::vector<Member> m_Object;
};

} // namespace Forge
