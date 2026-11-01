#pragma once
#include "main_mapfont_settings.h"
#include <vector>

struct GuiMapFontSettings {
    char name[64] = {};
    wchar_t userTtfFile[MAX_PATH] = {};
    int height = 14;
    bool antiAliasing = false;
    BYTE colorRed = 0;
    BYTE colorGreen = 0;
    BYTE colorBlue = 0;
};

static inline bool SafeGuiFontName(const char* name) {
    if (!name || !*name || strlen(name) >= 64) return false;
    for (const unsigned char* p = (const unsigned char*)name; *p; ++p)
        if (!( (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
               (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == '.' )) return false;
    return true;
}

static inline bool ParseGuiFontColor(const char* text, BYTE& red, BYTE& green, BYTE& blue) {
    if (!text || !*text) return false;
    int values[3] = {};
    const char* p = text;
    for (int i = 0; i < 3; ++i) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) return false;
        char* end = nullptr;
        const long value = strtol(p, &end, 10);
        if (end == p || value < 0 || value > 255) return false;
        values[i] = (int)value;
        p = end;
        if (i < 2 && *p != ' ' && *p != '\t') return false;
    }
    while (*p == ' ' || *p == '\t') ++p;
    if (*p) return false;
    red = (BYTE)values[0]; green = (BYTE)values[1]; blue = (BYTE)values[2];
    return true;
}
static inline bool LoadGuiMapFontSettings(const char* modFolder, std::vector<GuiMapFontSettings>& out) {
    out.clear();
    FILE* f = OpenTtfSettings(modFolder);
    if (!f) return false;
    char line[512];
    bool inBlock = false, awaitingOpen = false;
    GuiMapFontSettings current{};
    bool hasName = false, hasFile = false, valid = true;
    int height = 14;
    auto reset = [&]() {
        current = GuiMapFontSettings{}; hasName = hasFile = false; valid = true; height = 14;
    };
    auto finish = [&]() {
        if (hasName && hasFile && valid && out.size() < 64) {
            bool duplicate = false;
            for (const auto& item : out) if (_stricmp(item.name, current.name) == 0) duplicate = true;
            if (!duplicate) { current.height = height; out.push_back(current); }
        }
        reset();
    };
    while (ReadSettingLine(f, line, sizeof(line))) {
        if (!inBlock) {
            if (awaitingOpen && strchr(line, '{')) {
                inBlock = true; awaitingOpen = false; reset();
                continue;
            }
            constexpr size_t keywordLength = sizeof("gui_font") - 1;
            if (_strnicmp(line, "gui_font", keywordLength) != 0) continue;
            const char next = line[keywordLength];
            if (next && next != ' ' && next != '\t' && next != '=' && next != '{') continue;
            if (strchr(line, '{')) { inBlock = true; reset(); }
            else awaitingOpen = true;
            continue;
        }
        if (!strchr(line, '}')) {
            char* eq = strchr(line, '='); if (!eq) continue;
            *eq++ = 0; TrimSetting(line); TrimSetting(eq);
            if (_stricmp(line, "gui_font_name") == 0 && SafeGuiFontName(eq)) { strcpy_s(current.name, eq); hasName = true; }
            else if (_stricmp(line, "color") == 0) {
                if (!ParseGuiFontColor(eq, current.colorRed, current.colorGreen, current.colorBlue)) valid = false;
            }
            else if (_stricmp(line, "anti_aliasing") == 0) {
                if (!ParseMapFontActive(eq, current.antiAliasing)) valid = false;
            }
            else if (_stricmp(line, "user_ttf_file") == 0) {
                wchar_t path[MAX_PATH] = {};
                if (SettingUtf8ToWide(eq, path, MAX_PATH) && SafeRelativeFontPath(path)) { wcsncpy_s(current.userTtfFile, path, _TRUNCATE); hasFile = true; }
            }
            else if (_stricmp(line, "height") == 0) ParseMapFontHeight(eq, height);
            continue;
        }
        finish(); inBlock = false;
    }
    if (inBlock) finish();
    fclose(f);
    return !out.empty();
}