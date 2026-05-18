#include "settings_json.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <string_view>

namespace Settings { namespace Json {

namespace {

void appendHex32(std::string& out, uint32_t value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "\"0x%08X\"", value);
    out += buf;
}

void appendHex64(std::string& out, uint64_t value)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "\"0x%016llX\"", static_cast<unsigned long long>(value));
    out += buf;
}

void appendInt(std::string& out, int64_t value)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(value));
    out += buf;
}

void appendBool(std::string& out, bool value)
{
    out += value ? "true" : "false";
}

void appendEscapedString(std::string& out, const char* s)
{
    out += '"';
    for (const char* p = s; *p; ++p) {
        char c = *p;
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c == '\n') { out += "\\n"; }
        else if (c == '\r') { out += "\\r"; }
        else if (c == '\t') { out += "\\t"; }
        else { out += c; }
    }
    out += '"';
}

bool isWhitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void skipWhitespace(std::string_view s, size_t& pos)
{
    while (pos < s.size() && isWhitespace(s[pos])) ++pos;
}

bool parseString(std::string_view s, size_t& pos, std::string& out)
{
    skipWhitespace(s, pos);
    if (pos >= s.size() || s[pos] != '"') return false;
    ++pos;
    out.clear();
    while (pos < s.size() && s[pos] != '"') {
        if (s[pos] == '\\' && pos + 1 < s.size()) {
            char esc = s[pos + 1];
            if (esc == 'n') out += '\n';
            else if (esc == 'r') out += '\r';
            else if (esc == 't') out += '\t';
            else out += esc;
            pos += 2;
        } else {
            out += s[pos++];
        }
    }
    if (pos >= s.size()) return false;
    ++pos;
    return true;
}

bool parseInt64(std::string_view s, size_t& pos, int64_t& out)
{
    skipWhitespace(s, pos);
    size_t start = pos;
    if (pos < s.size() && (s[pos] == '-' || s[pos] == '+')) ++pos;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9') ++pos;
    if (pos == start) return false;
    out = strtoll(std::string(s.substr(start, pos - start)).c_str(), nullptr, 10);
    return true;
}

bool parseBool(std::string_view s, size_t& pos, bool& out)
{
    skipWhitespace(s, pos);
    if (s.compare(pos, 4, "true") == 0) { out = true; pos += 4; return true; }
    if (s.compare(pos, 5, "false") == 0) { out = false; pos += 5; return true; }
    return false;
}

// Parse a hex value that may be either a JSON number or a "0x..." string.
bool parseHex64(std::string_view s, size_t& pos, uint64_t& out)
{
    skipWhitespace(s, pos);
    if (pos < s.size() && s[pos] == '"') {
        std::string str;
        if (!parseString(s, pos, str)) return false;
        out = strtoull(str.c_str(), nullptr, 0);
        return true;
    }
    int64_t v;
    if (!parseInt64(s, pos, v)) return false;
    out = static_cast<uint64_t>(v);
    return true;
}

bool parseHex32(std::string_view s, size_t& pos, uint32_t& out)
{
    uint64_t v;
    if (!parseHex64(s, pos, v)) return false;
    out = static_cast<uint32_t>(v);
    return true;
}

bool expect(std::string_view s, size_t& pos, char c)
{
    skipWhitespace(s, pos);
    if (pos < s.size() && s[pos] == c) { ++pos; return true; }
    return false;
}

bool peek(std::string_view s, size_t pos, char c)
{
    skipWhitespace(s, pos);
    return pos < s.size() && s[pos] == c;
}

// Skip a JSON value (object, array, string, number, true, false, null) without parsing it.
bool skipValue(std::string_view s, size_t& pos)
{
    skipWhitespace(s, pos);
    if (pos >= s.size()) return false;
    char c = s[pos];
    if (c == '"') { std::string tmp; return parseString(s, pos, tmp); }
    if (c == '{' || c == '[') {
        char open = c, close = (c == '{') ? '}' : ']';
        int depth = 0;
        while (pos < s.size()) {
            char ch = s[pos++];
            if (ch == '"') { --pos; std::string tmp; if (!parseString(s, pos, tmp)) return false; continue; }
            if (ch == open) ++depth;
            else if (ch == close) { --depth; if (depth == 0) return true; }
        }
        return false;
    }
    while (pos < s.size() && s[pos] != ',' && s[pos] != '}' && s[pos] != ']' && !isWhitespace(s[pos])) ++pos;
    return true;
}

}

std::string Serialize(const PluginSettings& s)
{
    std::string out;
    out.reserve(1024);
    out += "{\n";

    auto comma = [&](){ out += ",\n"; };

    out += "  \"configVersion\": ";       appendInt(out, s.configVersion); comma();
    out += "  \"lastIndex\": ";           appendInt(out, s.lastIndex); comma();
    out += "  \"lastCategoryIndex\": ";   appendInt(out, s.lastCategoryIndex); comma();
    out += "  \"showNumbers\": ";         appendBool(out, s.showNumbers); comma();
    out += "  \"showFavorites\": ";       appendBool(out, s.showFavorites); comma();
    out += "  \"layoutFontScale\": ";     appendInt(out, s.layoutPrefs.fontScale); comma();
    out += "  \"layoutListWidth\": ";     appendInt(out, s.layoutPrefs.listWidthPercent); comma();
    out += "  \"layoutIconSize\": ";      appendInt(out, s.layoutPrefs.iconSizePercent); comma();
    out += "  \"bgColor\": ";             appendHex32(out, s.bgColor); comma();
    out += "  \"titleColor\": ";          appendHex32(out, s.titleColor); comma();
    out += "  \"highlightedColor\": ";    appendHex32(out, s.highlightedTitleColor); comma();
    out += "  \"favoriteColor\": ";       appendHex32(out, s.favoriteColor); comma();
    out += "  \"headerColor\": ";         appendHex32(out, s.headerColor); comma();
    out += "  \"categoryColor\": ";       appendHex32(out, s.categoryColor); comma();
    out += "  \"nextCategoryId\": ";      appendInt(out, s.nextCategoryId); comma();

    out += "  \"favorites\": [";
    for (size_t i = 0; i < s.favorites.size(); ++i) {
        if (i) out += ", ";
        appendHex64(out, s.favorites[i]);
    }
    out += "]";
    comma();

    out += "  \"categories\": [";
    for (size_t i = 0; i < s.categories.size(); ++i) {
        const Category& c = s.categories[i];
        if (i) out += ", ";
        out += "{\"id\": "; appendInt(out, c.id);
        out += ", \"order\": "; appendInt(out, c.order);
        out += ", \"name\": "; appendEscapedString(out, c.name);
        out += ", \"hidden\": "; appendBool(out, c.hidden);
        out += "}";
    }
    out += "]";
    comma();

    out += "  \"titleCategories\": [";
    for (size_t i = 0; i < s.titleCategories.size(); ++i) {
        const TitleCategoryAssignment& a = s.titleCategories[i];
        if (i) out += ", ";
        out += "{\"titleId\": "; appendHex64(out, a.titleId);
        out += ", \"categoryId\": "; appendInt(out, a.categoryId);
        out += "}";
    }
    out += "]\n";

    out += "}\n";
    return out;
}

bool Deserialize(std::string_view input, PluginSettings& out)
{
    size_t pos = 0;
    if (!expect(input, pos, '{')) return false;

    while (true) {
        skipWhitespace(input, pos);
        if (peek(input, pos, '}')) { ++pos; break; }

        std::string key;
        if (!parseString(input, pos, key)) return false;
        if (!expect(input, pos, ':')) return false;

        if (key == "configVersion") { int64_t v; if (parseInt64(input, pos, v)) out.configVersion = static_cast<int32_t>(v); }
        else if (key == "lastIndex") { int64_t v; if (parseInt64(input, pos, v)) out.lastIndex = static_cast<int32_t>(v); }
        else if (key == "lastCategoryIndex") { int64_t v; if (parseInt64(input, pos, v)) out.lastCategoryIndex = static_cast<int32_t>(v); }
        else if (key == "showNumbers") { bool v; if (parseBool(input, pos, v)) out.showNumbers = v; }
        else if (key == "showFavorites") { bool v; if (parseBool(input, pos, v)) out.showFavorites = v; }
        else if (key == "layoutFontScale") { int64_t v; if (parseInt64(input, pos, v)) out.layoutPrefs.fontScale = static_cast<int32_t>(v); }
        else if (key == "layoutListWidth") { int64_t v; if (parseInt64(input, pos, v)) out.layoutPrefs.listWidthPercent = static_cast<int32_t>(v); }
        else if (key == "layoutIconSize") { int64_t v; if (parseInt64(input, pos, v)) out.layoutPrefs.iconSizePercent = static_cast<int32_t>(v); }
        else if (key == "bgColor") { uint32_t v; if (parseHex32(input, pos, v)) out.bgColor = v; }
        else if (key == "titleColor") { uint32_t v; if (parseHex32(input, pos, v)) out.titleColor = v; }
        else if (key == "highlightedColor") { uint32_t v; if (parseHex32(input, pos, v)) out.highlightedTitleColor = v; }
        else if (key == "favoriteColor") { uint32_t v; if (parseHex32(input, pos, v)) out.favoriteColor = v; }
        else if (key == "headerColor") { uint32_t v; if (parseHex32(input, pos, v)) out.headerColor = v; }
        else if (key == "categoryColor") { uint32_t v; if (parseHex32(input, pos, v)) out.categoryColor = v; }
        else if (key == "nextCategoryId") { int64_t v; if (parseInt64(input, pos, v)) out.nextCategoryId = static_cast<uint16_t>(v); }
        else if (key == "favorites") {
            if (!expect(input, pos, '[')) return false;
            out.favorites.clear();
            while (!peek(input, pos, ']')) {
                uint64_t v;
                if (!parseHex64(input, pos, v)) return false;
                if (static_cast<int>(out.favorites.size()) < MAX_FAVORITES) {
                    out.favorites.push_back(v);
                }
                skipWhitespace(input, pos);
                if (peek(input, pos, ',')) ++pos;
            }
            ++pos;
        }
        else if (key == "categories") {
            if (!expect(input, pos, '[')) return false;
            out.categories.clear();
            while (!peek(input, pos, ']')) {
                if (!expect(input, pos, '{')) return false;
                Category c;
                while (!peek(input, pos, '}')) {
                    std::string fk;
                    if (!parseString(input, pos, fk)) return false;
                    if (!expect(input, pos, ':')) return false;
                    if (fk == "id") { int64_t v; if (parseInt64(input, pos, v)) c.id = static_cast<uint16_t>(v); }
                    else if (fk == "order") { int64_t v; if (parseInt64(input, pos, v)) c.order = static_cast<uint16_t>(v); }
                    else if (fk == "name") {
                        std::string n;
                        if (parseString(input, pos, n)) {
                            strncpy(c.name, n.c_str(), MAX_CATEGORY_NAME - 1);
                            c.name[MAX_CATEGORY_NAME - 1] = '\0';
                        }
                    }
                    else if (fk == "hidden") { bool v; if (parseBool(input, pos, v)) c.hidden = v; }
                    else { skipValue(input, pos); }
                    skipWhitespace(input, pos);
                    if (peek(input, pos, ',')) ++pos;
                }
                ++pos;
                if (static_cast<int>(out.categories.size()) < MAX_CATEGORIES) {
                    out.categories.push_back(c);
                }
                skipWhitespace(input, pos);
                if (peek(input, pos, ',')) ++pos;
            }
            ++pos;
        }
        else if (key == "titleCategories") {
            if (!expect(input, pos, '[')) return false;
            out.titleCategories.clear();
            while (!peek(input, pos, ']')) {
                if (!expect(input, pos, '{')) return false;
                TitleCategoryAssignment a;
                while (!peek(input, pos, '}')) {
                    std::string fk;
                    if (!parseString(input, pos, fk)) return false;
                    if (!expect(input, pos, ':')) return false;
                    if (fk == "titleId") { uint64_t v; if (parseHex64(input, pos, v)) a.titleId = v; }
                    else if (fk == "categoryId") { int64_t v; if (parseInt64(input, pos, v)) a.categoryId = static_cast<uint16_t>(v); }
                    else { skipValue(input, pos); }
                    skipWhitespace(input, pos);
                    if (peek(input, pos, ',')) ++pos;
                }
                ++pos;
                if (static_cast<int>(out.titleCategories.size()) < MAX_TITLE_CATEGORIES) {
                    out.titleCategories.push_back(a);
                }
                skipWhitespace(input, pos);
                if (peek(input, pos, ',')) ++pos;
            }
            ++pos;
        }
        else {
            if (!skipValue(input, pos)) return false;
        }

        skipWhitespace(input, pos);
        if (peek(input, pos, ',')) ++pos;
    }

    return true;
}

}}
