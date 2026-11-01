#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <imm.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <intrin.h>
#include <string>
#include <utility>
#include <vector>
#include <cwctype>
#include "V2ModAPI.h"
#include "map_country_hooks.h"
#include "map_province_ttf_hooks.h"
#include "main_mapfont_settings.h"
#include "gui_mapfont_settings.h"
static MainMapFontSettings g_mainMapFontSettings;
static ProvinceNameMapFontSettings g_provinceNameMapFontSettings;
static std::vector<GuiMapFontSettings> g_guiMapFontSettings;
static std::string g_guiFontPayload;
struct GuiFontQualityPolicy { char family[LF_FACESIZE] = {}; int height = 0; BYTE quality = 0xFF; DWORD color = 0xFF000000; bool colorAmbiguous = false; };
static std::vector<GuiFontQualityPolicy> g_guiFontQualityPolicies;
static bool BuildGuiMapFontPayload(const char* modFolder);
static bool WriteGuiMapFontGfx(const char* modFolder);


// Diagnostic stages: quoted lexer token, parser token copy, TextBoxType property handler.
static constexpr uint32_t kLexerNextRva = 0x00590930;
static constexpr uint32_t kParserTokenCopyRva = 0x005A1440;
static constexpr uint32_t kTextBoxPropertyRva = 0x0065E200;
static constexpr uint32_t kStringValueRva = 0x005A1750;
// FUN_009AA7C0 parses translated text markup after a localization key has
// already been resolved. Guard UTF-8 continuation byte 0x80 here as well.
static constexpr uint32_t kInternationalTextParserRva = 0x005AA7C0;
// Localization expansion splits text on byte BD. In UTF-8 it is also a
// continuation byte (for example, the final byte of Cyrillic н).
static constexpr uint32_t kLocalizedTextExpandRva = 0x005AB610;
// FUN_00A4AB70: font line-height measurement, called before DrawTextA.
static constexpr uint32_t kFontMeasureHeightRva = 0x0064AB70;
// Ordinary-font construction in Victoria II 3.04 (v2game.exe VA 0x00A49CE0).
static constexpr uint32_t kFontCreateRva = 0x00649CE0;
// EU3 GUI-object factory: decompilation shows type 0x14E constructs
// CEU3BitmapFont. Trace every bounded factory result to identify MapText.
static constexpr uint32_t kGuiObjectFactoryRva = 0x003E0960;
// ProvinceManager initialization (contains the CreateProvinceTexts phase).
static constexpr uint32_t kProvinceManagerInitRva = 0x003ED4D0;
static constexpr int kD3DXFontVtableSlots = 18;
static constexpr int kD3DXDrawTextASlot = 14;
static constexpr int kD3DXDrawTextWSlot = 15;
static const unsigned char kFontCreatePrologue[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 };
static const unsigned char kFontMeasureHeightPrologue[6] = { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C };
static const unsigned char kGuiObjectFactoryPrologue[9] = { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
static const unsigned char kProvinceManagerInitPrologue[5] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };
// GUI font definitions are generated as a regular mod-local .gfx resource.
static unsigned char g_fontCreateTrampoline[64];
static unsigned char g_fontMeasureHeightTrampoline[64];
static unsigned char g_lexerNextTrampoline[64];
static unsigned char g_parserTokenCopyTrampoline[64];
static unsigned char g_textBoxPropertyTrampoline[64];
static unsigned char g_stringValueTrampoline[64];
static unsigned char g_internationalTextParserTrampoline[64];
static unsigned char g_localizedTextExpandTrampoline[64];
static unsigned char g_guiObjectFactoryTrampoline[64];
static unsigned char g_provinceManagerInitTrampoline[64];
static void* g_fontCreateOriginal;
static void* g_fontMeasureHeightOriginal;
static void* g_lexerNextOriginal;
static void* g_parserTokenCopyOriginal;
static void* g_textBoxPropertyOriginal;
static void* g_stringValueOriginal;
static void* g_internationalTextParserOriginal;
static void* g_localizedTextExpandOriginal;
static void* g_guiObjectFactoryOriginal;
static void* g_provinceManagerInitOriginal;
static void* g_originalDrawTextA;
static void* g_originalDrawTextW;
static volatile LONG g_utf8RawFailureLogged;
static volatile LONG g_utf8HexFailureLogged;
static volatile LONG g_utf8RawSuccessLogged;
static volatile LONG g_utf8HexSuccessLogged;
static volatile LONG g_utf8EscapeLogged;
static volatile LONG g_utf8RestoreLogged;
static volatile LONG g_autoEscapeLogged;
static volatile LONG g_autoDrawObjectCount;
static volatile LONG g_autoDrawObjects[32];
static volatile LONG g_mojibakeRepairLogged;
static volatile LONG g_localizedBytesLogged;
static volatile LONG g_localizedEscapeLogged;
static volatile LONG g_localizedColorMarkerLogged;
static volatile LONG g_lexerTokenLogged;
static volatile LONG g_parserTokenLogged;
static volatile LONG g_textBoxPropertyBeforeLogged;
static volatile LONG g_textBoxPropertyAfterLogged;
static volatile LONG g_stringValueBeforeLogged;
static volatile LONG g_stringValueAfterLogged;
static volatile LONG g_unicodeTraceCount;
static volatile LONG g_ttfTextDiagCount;
static volatile LONG g_editboxRenderPhase = 0;
static volatile LONG g_editboxRenderSeenHashes[256] = {};
static volatile LONG g_editboxRenderTraceCount = 0;
static volatile LONG g_editboxRenderSeenCount = 0;
static volatile LONG g_colorRunDrawLogged;
static volatile LONG g_colorRunProbeCount;
struct D3DXFontDescA {
    INT height;
    UINT width;
    UINT weight;
    UINT mipLevels;
    BOOL italic;
    BYTE charset;
    BYTE outputPrecision;
    BYTE quality;
    BYTE pitchAndFamily;
    CHAR faceName[LF_FACESIZE];
};
struct TtfFontDiagnostic { void* object; char face[LF_FACESIZE]; int height; };
static TtfFontDiagnostic g_ttfFontDiagnostics[64];
static volatile LONG g_ttfFontDiagnosticCount;
static volatile LONG g_guiFactoryTraceCount;
static volatile LONG g_guiFactoryTypeSeen[2048];
static volatile LONG g_mapTextFactoryWindow;
static volatile LONG g_mapTextTokenCount;
static volatile LONG g_provinceInitTraceCount;
static const V2ModAPI* g_api;
static DWORD g_privateFontCount;
struct UnicodeFontMap { char logical[128]; char face[128]; };
static UnicodeFontMap g_unicodeFonts[128];
static size_t g_unicodeFontCount;

typedef int (__fastcall* LexerNextFn)(void* lexer);
// FUN_009a1440 ends with RET 4, so it is callee-cleanup (stdcall), despite
// the decompiler omitting the calling convention in its pseudocode.
typedef void (__stdcall* ParserTokenCopyFn)(void* parser);
typedef void (__thiscall* TextBoxPropertyFn)(void* self, void* parser, int propertyId);
typedef void (__stdcall* InternationalTextParserFn)(void* output, void* text);
typedef void (__thiscall* LocalizedTextExpandFn)(void* text, void* output, void* context);
typedef void* (__thiscall* GuiObjectFactoryFn)(void* self, void* context, int typeId);
// The entry reads its manager pointer from [EBP+8], and its epilogue is
// RET 4; preserve the original callee-cleanup (__stdcall) convention.
typedef void (__stdcall* ProvinceManagerInitFn)(void* self);

static DWORD TraceCallerRva(uintptr_t caller)
{
    const uintptr_t imageBase = (uintptr_t)GetModuleHandleW(nullptr);
    return imageBase && caller >= imageBase ? (DWORD)(caller - imageBase) : 0;
}

static void* __fastcall HookGuiObjectFactory(void* self, void*, void* context, int typeId)
{
    void* result = ((GuiObjectFactoryFn)g_guiObjectFactoryOriginal)(self, context, typeId);
    const LONG event = InterlockedIncrement(&g_guiFactoryTraceCount);
    const bool firstType = typeId >= 0 && typeId < (int)_countof(g_guiFactoryTypeSeen) &&
        InterlockedCompareExchange(&g_guiFactoryTypeSeen[typeId], 1, 0) == 0;
    const LONG window = InterlockedCompareExchange(&g_mapTextFactoryWindow, 0, 0);
    if (window > 0) InterlockedDecrement(&g_mapTextFactoryWindow);
    if ((firstType || window > 0) && g_api && g_api->Log) {
        void* vtable = result ? *(void**)result : nullptr;
        const uintptr_t caller = (uintptr_t)_ReturnAddress();
        (void)0;
    }
    return result;
}

static void __stdcall HookProvinceManagerInit(void* self)
{
    const LONG event = InterlockedIncrement(&g_provinceInitTraceCount);
    const uintptr_t caller = (uintptr_t)_ReturnAddress();
    if (event <= 8 && g_api && g_api->Log)
        (void)0;
    ((ProvinceManagerInitFn)g_provinceManagerInitOriginal)(self);
    if (event <= 8 && g_api && g_api->Log)
        (void)0;
}

static bool StartsWithUtf8Probe(const char* text)
{
    return text && memcmp(text, "UTF8:", 5) == 0;
}

static bool StartsWithUnicodeProbe(const char* text)
{
    return text && text[0] == 'U' && text[1] == 'T' && text[2] == 'F' && text[3] == '8' &&
        (text[4] == ':' || (text[4] == 'H' && text[5] == 'E' && text[6] == 'X' &&
            text[7] == ':'));
}

static void LoadUnicodeFontMap(const char* modFolder)
{
    g_unicodeFontCount = 0;
    char path[MAX_PATH] = {};
    if (!modFolder || _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\TTF Unicode Support\\unicode_fonts.txt", modFolder) < 0)
        return;
    FILE* file = nullptr;
    if (fopen_s(&file, path, "rb") != 0 || !file) return;
    char line[300];
    while (g_unicodeFontCount < _countof(g_unicodeFonts) && fgets(line, sizeof(line), file)) {
        char* start = line;
        while (*start == ' ' || *start == '\t') ++start;
        if (!*start || *start == '#' || *start == ';' || *start == '\r' || *start == '\n') continue;
        char* equal = strchr(start, '=');
        if (!equal) continue;
        *equal++ = '\0';
        char* end = start + strlen(start);
        while (end > start && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
        while (*equal == ' ' || *equal == '\t') ++equal;
        end = equal + strlen(equal);
        while (end > equal && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) *--end = '\0';
        if (!*start || !*equal || strlen(start) >= sizeof(g_unicodeFonts[0].logical) ||
            strlen(equal) >= sizeof(g_unicodeFonts[0].face)) continue;
        strcpy_s(g_unicodeFonts[g_unicodeFontCount].logical, start);
        strcpy_s(g_unicodeFonts[g_unicodeFontCount].face, equal);
        ++g_unicodeFontCount;
    }
    fclose(file);
}

static bool LoadPrivateTTFFonts(const char* modFolder)
{
    if (!modFolder || !*modFolder) return false;
    wchar_t wideFolder[MAX_PATH] = {};
    if (!MultiByteToWideChar(CP_ACP, 0, modFolder, -1, wideFolder, MAX_PATH)) return false;
    wchar_t searchPath[MAX_PATH] = {};
    if (_snwprintf_s(searchPath, _countof(searchPath), _TRUNCATE, L"%s\\TTF Unicode Support\\*.ttf", wideFolder) < 0)
        return false;
    WIN32_FIND_DATAW entry = {};
    HANDLE search = FindFirstFileW(searchPath, &entry);
    if (search == INVALID_HANDLE_VALUE) return false;
    bool loadedAny = false;
    do {
        if (entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        wchar_t fontPath[MAX_PATH] = {};
        if (_snwprintf_s(fontPath, _countof(fontPath), _TRUNCATE, L"%s\\TTF Unicode Support\\%s",
            wideFolder, entry.cFileName) < 0) continue;
        const DWORD faces = AddFontResourceExW(fontPath, FR_PRIVATE, 0);
        if (faces) {
            loadedAny = true;
            g_privateFontCount += faces;
            if (g_api && g_api->Log)
                (void)0;
        }
        else if (g_api && g_api->Log) {
            (void)0;
        }
    } while (FindNextFileW(search, &entry));
    FindClose(search);
    return loadedAny;
}

static size_t BoundedTextLength(const char* text, size_t limit)
{
    if (!text) return 0;
    size_t n = 0;
    while (n < limit && text[n]) ++n;
    return n;
}

static size_t SafeBoundedTextLength(const char* text, size_t limit)
{
    if (!text) return 0;
    __try {
        size_t n = 0;
        while (n < limit && text[n]) ++n;
        return n;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

// The game's internationalized-text parser treats byte 0x80 as a markup
// delimiter. Escape only that byte when it is a valid UTF-8 continuation,
// using DEL (which the markup parser does not reserve) as a one-byte marker.
static size_t MapUtf8ContinuationByte(char* text, size_t length,
    unsigned char from, unsigned char to)
{
    if (!text) return 0;
    size_t changed = 0;
    for (size_t i = 0; i < length;) {
        const unsigned char lead = (unsigned char)text[i];
        size_t continuationCount = 0;
        if (lead >= 0xC2 && lead <= 0xDF) continuationCount = 1;
        else if (lead >= 0xE0 && lead <= 0xEF) continuationCount = 2;
        else if (lead >= 0xF0 && lead <= 0xF4) continuationCount = 3;
        else { ++i; continue; }
        if (i + continuationCount >= length) { ++i; continue; }
        bool valid = true;
        for (size_t j = 1; j <= continuationCount; ++j) {
            const unsigned char byte = (unsigned char)text[i + j];
            if (!((byte >= 0x80 && byte <= 0xBF) || byte == from ||
                  byte == 0x7F || byte == 0x1D)) {
                valid = false;
                break;
            }
        }
        if (!valid) { ++i; continue; }
        for (size_t j = 1; j <= continuationCount; ++j) {
            if ((unsigned char)text[i + j] == from) {
                text[i + j] = (char)to;
                ++changed;
            }
        }
        i += continuationCount + 1;
    }
    return changed;
}

static bool IsGameColorCode(unsigned char c)
{
    return c == 'W' || c == 'B' || c == 'G' || c == 'R' || c == 'b' || c == 'g' || c == 'Y' || c == '!';
}

static bool HasGameColorMarker(const char* text, size_t length)
{
    if (!text) return false;
    for (size_t i = 0; i + 1 < length; ++i) {
        if ((unsigned char)text[i] == 0xA7 && IsGameColorCode((unsigned char)text[i + 1]))
            return true;
        if (i + 2 < length && (unsigned char)text[i] == 0xC2 &&
            (unsigned char)text[i + 1] == 0xA7 && IsGameColorCode((unsigned char)text[i + 2]))
            return true;
    }
    return false;
}

static bool HasEventColorProbeMarker(const char* text, size_t length)
{
    // UTF-8 for the first word in CC_TTF_EVENT_DESC ("Жёлтый"). This keeps
    // event-path diagnostics separate from the many colored UI tooltips.
    static const unsigned char marker[] = { 0xD0, 0x96, 0xD1, 0x91, 0xD0, 0xBB };
    if (!text || length < sizeof(marker)) return false;
    for (size_t i = 0; i + sizeof(marker) <= length; ++i)
        if (memcmp(text + i, marker, sizeof(marker)) == 0) return true;
    return false;
}

// The expansion pass runs before the international-text parser. Protect BD
// here, while the full localized string is still available. ASCII GS (1D)
// is only a temporary internal byte; it is restored before UTF-8 decoding.
static void __fastcall HookLocalizedTextExpand(void* text, void*, void* output, void* context)
{
    auto original = (LocalizedTextExpandFn)g_localizedTextExpandOriginal;
    if (!text) { original(text, output, context); return; }
    char* object = (char*)text;
    const size_t length = *(const size_t*)(object + 0x10);
    const size_t capacity = *(const size_t*)(object + 0x14);
    if (length > 8191 || capacity > 0x100000) {
        original(text, output, context);
        return;
    }
    char* data = capacity < 16 ? object : *(char**)object;
    if (!data) { original(text, output, context); return; }
    const size_t changed = MapUtf8ContinuationByte(data, length, 0xBD, 0x1D);
    if (changed && g_api && g_api->Log) {
        static volatile LONG logged = 0;
        if (InterlockedCompareExchange(&logged, 1, 0) == 0)
            (void)0;
    }
    __try {
        original(text, output, context);
    }
    __finally {
        if (changed) MapUtf8ContinuationByte(data, length, 0x1D, 0xBD);
    }
}

// The game's markup parser expects the section-sign marker as one Windows-1252
// byte (A7). UTF-8 localization stores it as C2 A7; passing those two bytes
// through makes the markup parser split color runs inside multibyte text.
static size_t NormalizeUtf8ColorMarkers(char* text, size_t& length)
{
    if (!text) return 0;
    size_t changed = 0;
    for (size_t i = 0; i + 2 < length;) {
        if ((unsigned char)text[i] == 0xC2 && (unsigned char)text[i + 1] == 0xA7 &&
            IsGameColorCode((unsigned char)text[i + 2])) {
            text[i] = (char)0xA7;
            memmove(text + i + 1, text + i + 2, length - i - 2);
            --length;
            ++changed;
        } else {
            ++i;
        }
    }
    text[length] = '\0';
    return changed;
}

static void LogUnicodeTrace(const char* stage, const char* text, int count, uintptr_t caller)
{
    if (!g_api || !g_api->Log || !StartsWithUnicodeProbe(text)) return;
    // Keep one diagnostic run bounded even if the GUI redraws every frame.
    const LONG event = InterlockedIncrement(&g_unicodeTraceCount);
    if (event > 96) return;
    const size_t nulLength = BoundedTextLength(text, 4096);
    size_t sampleLength = nulLength < 64 ? nulLength : 64;
    if (count >= 0 && (size_t)count < sampleLength) sampleLength = (size_t)count;
    char hex[200] = {};
    size_t used = 0;
    for (size_t i = 0; i < sampleLength && used + 4 < sizeof(hex); ++i)
        used += (size_t)_snprintf_s(hex + used, sizeof(hex) - used, _TRUNCATE,
            "%02X ", (unsigned char)text[i]);
    const uintptr_t imageBase = (uintptr_t)GetModuleHandleW(nullptr);
    const DWORD callerRva = imageBase && caller >= imageBase ? (DWORD)(caller - imageBase) : 0;
    (void)0;
}

static void LogProbeBytes(const char* stage, const char* text, int propertyId = -1)
{
    if (!g_api || !g_api->Log || !StartsWithUtf8Probe(text)) return;
    char hex[160] = {};
    size_t used = 0;
    for (size_t i = 0; i < 40 && text[i] && used + 4 < sizeof(hex); ++i)
        used += (size_t)_snprintf_s(hex + used, sizeof(hex) - used, _TRUNCATE,
            "%02X ", (unsigned char)text[i]);
    (void)0;
}

// MSVC x86 std::string layout: 16-byte SSO/pointer union, size, capacity.
// Only read for the known UTF8 probe assignment destination.
static void LogProbeStringObject(const char* stage, const void* object)
{
    if (!g_api || !g_api->Log || !object) return;
    const unsigned char* bytes = (const unsigned char*)object;
    const size_t length = *(const size_t*)(bytes + 0x10);
    const size_t capacity = *(const size_t*)(bytes + 0x14);
    if (length > 4096 || capacity > 0x100000) {
        (void)0;
        return;
    }
    const char* data = capacity < 16 ? (const char*)bytes : *(const char* const*)bytes;
    if (!data) {
        (void)0;
        return;
    }
    char hex[160] = {};
    size_t used = 0;
    const size_t limit = length < 40 ? length : 40;
    for (size_t i = 0; i < limit && used + 4 < sizeof(hex); ++i)
        used += (size_t)_snprintf_s(hex + used, sizeof(hex) - used, _TRUNCATE,
            "%02X ", (unsigned char)data[i]);
    if (length >= 5 && memcmp(data, "UTF8:", 5) == 0)
        (void)0;
    else
        (void)0;
}

static int __fastcall HookLexerNext(void* lexer)
{
    const int result = ((LexerNextFn)g_lexerNextOriginal)(lexer);
    const char* token = lexer ? (const char*)((const unsigned char*)lexer + 0x10) : nullptr;
    if (result && token && (!strcmp(token, "mapTextType") || !strcmp(token, "MapText"))) {
        const LONG hit = InterlockedIncrement(&g_mapTextTokenCount);
        if (hit <= 16 && g_api && g_api->Log)
            (void)0;
        InterlockedExchange(&g_mapTextFactoryWindow, 128);
    }
    if (result && StartsWithUtf8Probe(token) &&
        InterlockedCompareExchange(&g_lexerTokenLogged, 1, 0) == 0)
        LogProbeBytes("lexer-token", token);
    return result;
}

static void __stdcall HookParserTokenCopy(void* parser)
{
    ((ParserTokenCopyFn)g_parserTokenCopyOriginal)(parser);
    const char* token = parser ? (const char*)((const unsigned char*)parser + 0x22C) : nullptr;
    if (StartsWithUtf8Probe(token) &&
        InterlockedCompareExchange(&g_parserTokenLogged, 1, 0) == 0)
        LogProbeBytes("parser-token", token);
}

typedef void (__thiscall* StringValueFn)(void* parser, void* argument, char mode);

static void __fastcall HookStringValue(void* parser, void*, void* argument, char mode)
{
    const char* value = parser ? (const char*)((const unsigned char*)parser + 0x22C) : nullptr;
    const bool isProbe = StartsWithUtf8Probe(value);
    if (isProbe && InterlockedCompareExchange(&g_stringValueBeforeLogged, 1, 0) == 0) {
        LogProbeBytes("string-value-before", value);
        LogProbeStringObject("string-target-before", argument);
    }
    ((StringValueFn)g_stringValueOriginal)(parser, argument, mode);
    value = parser ? (const char*)((const unsigned char*)parser + 0x22C) : nullptr;
    if (isProbe && InterlockedCompareExchange(&g_stringValueAfterLogged, 1, 0) == 0) {
        LogProbeBytes("string-value-after", value);
        LogProbeStringObject("string-target-after", argument);
    }
}

static void __fastcall HookTextBoxProperty(void* self, void*, void* parser, int propertyId)
{
    char* token = parser ? (char*)((unsigned char*)parser + 0x22C) : nullptr;
    // The game routes every TextBoxType text value through the same parser.
    // Escape only 0x80 bytes which are valid UTF-8 continuation bytes; legacy
    // ANSI strings and the game's other markup bytes are left untouched.
    const bool isUtf8Text = propertyId == 0x8C;
    if (StartsWithUtf8Probe(token) &&
        InterlockedCompareExchange(&g_textBoxPropertyBeforeLogged, 1, 0) == 0)
        LogProbeBytes("textbox-property-before", token, propertyId);
    char original[4096];
    size_t originalLength = 0;
    bool escaped = false;
    if (isUtf8Text) {
        originalLength = BoundedTextLength(token, sizeof(original));
        if (originalLength < sizeof(original)) {
            memcpy(original, token, originalLength + 1);
            const size_t changed = MapUtf8ContinuationByte(token, originalLength, 0x80, 0x7F);
            escaped = changed != 0;
            if (escaped && g_api && g_api->Log &&
                InterlockedCompareExchange(&g_autoEscapeLogged, 1, 0) == 0)
                (void)0;
            if (escaped && g_api && g_api->Log &&
                InterlockedCompareExchange(&g_utf8EscapeLogged, 1, 0) == 0)
                (void)0;
        }
    }
    if (escaped) {
        __try {
            ((TextBoxPropertyFn)g_textBoxPropertyOriginal)(self, parser, propertyId);
        }
        __finally {
            memcpy(token, original, originalLength + 1);
        }
    }
    else {
        ((TextBoxPropertyFn)g_textBoxPropertyOriginal)(self, parser, propertyId);
    }
    token = parser ? (char*)((unsigned char*)parser + 0x22C) : nullptr;
    if (StartsWithUtf8Probe(token) &&
        InterlockedCompareExchange(&g_textBoxPropertyAfterLogged, 1, 0) == 0)
        LogProbeBytes("textbox-property-after", token, propertyId);
}

static void __stdcall HookInternationalTextParser(void* output, void* text)
{
    char* object = (char*)text;
    if (!object) {
        ((InternationalTextParserFn)g_internationalTextParserOriginal)(output, text);
        return;
    }
    const size_t length = *(const size_t*)(object + 0x10);
    const size_t capacity = *(const size_t*)(object + 0x14);
    if (length > 8191 || capacity > 0x100000) {
        ((InternationalTextParserFn)g_internationalTextParserOriginal)(output, text);
        return;
    }
    char* data = capacity < 16 ? object : *(char**)object;
    if (!data) {
        ((InternationalTextParserFn)g_internationalTextParserOriginal)(output, text);
        return;
    }
    if (HasEventColorProbeMarker(data, length) && g_api && g_api->Log) {
        static volatile LONG localizedColorTraceCount = 0;
        const LONG trace = InterlockedIncrement(&localizedColorTraceCount);
        if (trace <= 8) {
            char sample[320] = {};
            size_t used = 0;
            const size_t limit = length < 96 ? length : 96;
            for (size_t i = 0; i < limit && used + 4 < sizeof(sample); ++i)
                used += (size_t)_snprintf_s(sample + used, sizeof(sample) - used, _TRUNCATE,
                    "%02X ", (unsigned char)data[i]);
            (void)0;
        }
    }
    if (length >= 12 && memcmp(data, "English 123", 11) == 0 && g_api && g_api->Log &&
        InterlockedCompareExchange(&g_localizedBytesLogged, 1, 0) == 0) {
        char sample[160] = {};
        size_t used = 0;
        const size_t limit = length < 40 ? length : 40;
        for (size_t i = 0; i < limit && used + 4 < sizeof(sample); ++i)
            used += (size_t)_snprintf_s(sample + used, sizeof(sample) - used, _TRUNCATE,
                "%02X ", (unsigned char)data[i]);
        (void)0;
    }
    char original[8192];
    memcpy(original, data, length);
    size_t parserLength = length;
    const size_t colorMarkers = NormalizeUtf8ColorMarkers(data, parserLength);
    if (colorMarkers) {
        *(size_t*)(object + 0x10) = parserLength;
        if (InterlockedCompareExchange(&g_localizedColorMarkerLogged, 1, 0) == 0 && g_api && g_api->Log)
            (void)0;
    }
    const size_t changed = MapUtf8ContinuationByte(data, parserLength, 0x80, 0x7F);
    if (!changed && !colorMarkers) {
        ((InternationalTextParserFn)g_internationalTextParserOriginal)(output, text);
        return;
    }
    if (changed && g_api && g_api->Log && InterlockedCompareExchange(&g_localizedEscapeLogged, 1, 0) == 0)
        (void)0;
    ((InternationalTextParserFn)g_internationalTextParserOriginal)(output, text);
    memcpy(data, original, length);
    *(size_t*)(object + 0x10) = length;
    data[length] = '\0';
}

typedef void (__fastcall* FontCreateFn)(void* fontConfig);
typedef int (__stdcall* DrawTextAFn)(void* self, void* sprite, const char* text,
    int count, RECT* rect, DWORD format, DWORD color);
typedef int (__stdcall* DrawTextWFn)(void* self, void* sprite, const wchar_t* text,
    int count, RECT* rect, DWORD format, DWORD color);

typedef unsigned int (__thiscall* FontMeasureHeightFn)(void* self, const char* text, int count);

static unsigned int __fastcall HookFontMeasureHeight(void* self, void*, const char* text, int count)
{
    LogUnicodeTrace("height-measure-entry", text, count, (uintptr_t)_ReturnAddress());
    return ((FontMeasureHeightFn)g_fontMeasureHeightOriginal)(self, text, count);
}

static int HexValue(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static bool DecodeUtf8Hex(const char* text, size_t length, std::vector<wchar_t>& output)
{
    if (!text) return false;
    std::vector<char> bytes;
    int high = -1;
    for (size_t i = 0; i < length && text[i]; ++i) {
        const char c = text[i];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '-') continue;
        const int value = HexValue(c);
        if (value < 0) return false;
        if (high < 0) high = value;
        else { bytes.push_back((char)((high << 4) | value)); high = -1; }
    }
    if (high >= 0 || bytes.empty()) return false;
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
        (int)bytes.size(), nullptr, 0);
    if (needed <= 0) return false;
    output.resize((size_t)needed + 1);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(),
        (int)bytes.size(), output.data(), needed) != needed) return false;
    output[needed] = L'\0';
    return true;
}

static bool DecodeUtf8(const char* text, size_t length, std::vector<wchar_t>& output)
{
    if (!text || length > INT_MAX) return false;
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text,
        (int)length, nullptr, 0);
    if (needed <= 0) return false;
    output.resize((size_t)needed + 1);
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, (int)length,
        output.data(), needed) != needed) return false;
    output[needed] = L'\0';
    return true;
}

static int MojibakeScore(const wchar_t* text, size_t length)
{
    int score = 0;
    for (size_t i = 0; i < length; ++i)
        if (text[i] == 0x00C2 || text[i] == 0x00C3 || text[i] == 0x00D0 || text[i] == 0x00D1)
            ++score;
    return score;
}

static bool RepairUtf8ReadAsWindows1252(std::vector<wchar_t>& wide)
{
    if (wide.empty()) return false;
    const size_t length = wcslen(wide.data());
    const int before = MojibakeScore(wide.data(), length);
    if (before == 0 || length > INT_MAX) return false;
    // A UTF-8 string may already have been decoded as Windows-1252 by the
    // game's localization path. Bytes 0x80..0x9F then become C1 control code
    // points (e.g. Cyrillic UTF-8 continuation bytes). WideCharToMultiByte
    // cannot round-trip those undefined CP1252 slots and rejects the entire
    // mixed-script string. Preserve those byte values explicitly, while
    // mapping the defined CP1252 extension characters back to their bytes.
    static const wchar_t cp1252Extra[32] = {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178
    };
    std::vector<char> bytes;
    bytes.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        const wchar_t c = wide[i];
        if (c <= 0x00FF) {
            bytes.push_back((char)c);
            continue;
        }
        bool found = false;
        for (size_t j = 0; j < _countof(cp1252Extra); ++j) {
            if (cp1252Extra[j] == c) {
                bytes.push_back((char)(0x80 + j));
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    if (bytes.empty() || bytes.size() > 16384) return false;
    std::vector<wchar_t> repaired;
    if (!DecodeUtf8(bytes.data(), bytes.size(), repaired)) return false;
    const size_t repairedLength = wcslen(repaired.data());
    if (MojibakeScore(repaired.data(), repairedLength) >= before) return false;
    wide.swap(repaired);
    if (g_api && g_api->Log && InterlockedCompareExchange(&g_mojibakeRepairLogged, 1, 0) == 0)
        (void)0;
    return true;
}

static const char* TtfDiagnosticFace(void* object)
{
    const LONG count = InterlockedCompareExchange(&g_ttfFontDiagnosticCount, 0, 0);
    for (LONG i = 0; i < count && i < (LONG)_countof(g_ttfFontDiagnostics); ++i)
        if (g_ttfFontDiagnostics[i].object == object) return g_ttfFontDiagnostics[i].face;
    return nullptr;
}

static int TtfDiagnosticHeight(void* object)
{
    const LONG count = InterlockedCompareExchange(&g_ttfFontDiagnosticCount, 0, 0);
    for (LONG i = 0; i < count && i < (LONG)_countof(g_ttfFontDiagnostics); ++i)
        if (g_ttfFontDiagnostics[i].object == object) return g_ttfFontDiagnostics[i].height;
    return 0;
}

static DWORD ResolveGuiFontDrawColor(void* fontObject, DWORD fallback)
{
    const char* face = TtfDiagnosticFace(fontObject);
    int height = TtfDiagnosticHeight(fontObject);
    if (!face) return fallback;
    if (height < 0) height = -height;
    const GuiFontQualityPolicy* match = nullptr;
    for (const auto& policy : g_guiFontQualityPolicies) {
        if (policy.height != height || _stricmp(policy.family, face) != 0) continue;
        if (policy.colorAmbiguous) return fallback;
        if (match && match->color != policy.color) return fallback;
        match = &policy;
    }
    return match ? match->color : fallback;
}
static void RegisterTtfDiagnosticFont(void* object, const char* face)
{
    if (!object || !face) return;
    const LONG slot = InterlockedIncrement(&g_ttfFontDiagnosticCount) - 1;
    if (slot < 0 || slot >= (LONG)_countof(g_ttfFontDiagnostics)) {
        InterlockedExchange(&g_ttfFontDiagnosticCount, (LONG)_countof(g_ttfFontDiagnostics));
        return;
    }
    g_ttfFontDiagnostics[slot].object = object;
    strncpy_s(g_ttfFontDiagnostics[slot].face, face, _TRUNCATE);
    void** vtable = *(void***)object;
    if (vtable && vtable[4]) {
        using GetDescFn = HRESULT (WINAPI*)(void*, D3DXFontDescA*);
        D3DXFontDescA desc{};
        if (SUCCEEDED(((GetDescFn)vtable[4])(object, &desc)))
            g_ttfFontDiagnostics[slot].height = desc.height;
    }
    if (g_api && g_api->Log)
        (void)0;
}

static void LogArmyEditboxRender(void* object, const char* raw, size_t rawLength,
                                 const wchar_t* wide, const char* stage, int drawResult,
                                 RECT* rect, DWORD format)
{
    const char* face = TtfDiagnosticFace(object);
    if (!g_api || !g_api->Log || !face || _stricmp(face, "Arial") != 0 ||
        TtfDiagnosticHeight(object) != 15 || rawLength > 512 || !stage || (rawLength && !raw)) return;
    bool hasHighByte = false;
    for (size_t i = 0; raw && i < rawLength; ++i)
        if ((unsigned char)raw[i] >= 0x80) { hasHighByte = true; break; }
    bool hasWideNonAscii = false;
    if (wide) for (size_t i = 0; wide[i] && i < 512; ++i)
        if ((uint16_t)wide[i] > 0x7F) { hasWideNonAscii = true; break; }
    if (!hasHighByte && !hasWideNonAscii) return;
    const LONG phase = InterlockedCompareExchange(&g_editboxRenderPhase, 0, 0);
    uint32_t hash = 2166136261u;
    auto mix = [&hash](uint8_t value) { hash = (hash ^ value) * 16777619u; };
    for (size_t i = 0; raw && i < rawLength; ++i) mix((uint8_t)raw[i]);
    if (wide) for (size_t i = 0; wide[i] && i < 512; ++i) {
        const uint16_t unit = (uint16_t)wide[i]; mix((uint8_t)unit); mix((uint8_t)(unit >> 8));
    }
    for (const char* c = stage; *c; ++c) mix((uint8_t)*c);
    mix((uint8_t)phase);
    const uintptr_t fontKey = (uintptr_t)object;
    for (size_t i = 0; i < sizeof(fontKey); ++i) mix((uint8_t)(fontKey >> (i * 8)));
    LONG seen = InterlockedCompareExchange(&g_editboxRenderSeenCount, 0, 0);
    if (seen > 256) seen = 256;
    for (LONG i = 0; i < seen; ++i)
        if (InterlockedCompareExchange(&g_editboxRenderSeenHashes[i], 0, 0) == (LONG)hash) return;
    if (seen >= 256) return;
    InterlockedExchange(&g_editboxRenderSeenHashes[seen], (LONG)hash);
    InterlockedExchange(&g_editboxRenderSeenCount, seen + 1);
    const LONG event = InterlockedIncrement(&g_editboxRenderTraceCount);
    char rawHex[256] = {};
    size_t used = 0;
    for (size_t i = 0; raw && i < rawLength && i < 48 && used + 4 < sizeof(rawHex); ++i)
        used += (size_t)_snprintf_s(rawHex + used, sizeof(rawHex) - used, _TRUNCATE, "%02X ", (unsigned char)raw[i]);
    char codepoints[512] = {};
    used = 0;
    if (wide) for (size_t i = 0; wide[i] && i < 512 && used + 16 < sizeof(codepoints); ++i) {
        uint32_t cp = (uint16_t)wide[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && wide[i + 1] >= 0xDC00 && wide[i + 1] <= 0xDFFF)
            cp = 0x10000 + ((cp - 0xD800) << 10) + ((uint16_t)wide[++i] - 0xDC00);
        used += (size_t)_snprintf_s(codepoints + used, sizeof(codepoints) - used, _TRUNCATE,
            cp <= 0xFFFF ? "U+%04X " : "U+%06X ", (unsigned)cp);
    }
    const char* phaseName = phase == 0 ? "typing" : phase == 1 ? "confirm-call" : "after-confirm";
    const long rectWidth = rect ? (long)(rect->right - rect->left) : -1L;
    const long rectHeight = rect ? (long)(rect->bottom - rect->top) : -1L;
    (void)0;
}
static void LogTtfDrawBytes(void* object, const char* text, size_t length,
                            const wchar_t* decoded, bool repaired, const char* status)
{
    const char* face = TtfDiagnosticFace(object);
    if (!g_api || !g_api->Log || !face || (!text && length)) return;
    bool highByte = false;
    for (size_t i = 0; i < length; ++i) if ((unsigned char)text[i] >= 0x80) { highByte = true; break; }
    if (!highByte && !(text && length >= 11 && memcmp(text, "English 123", 11) == 0)) return;
    const LONG event = InterlockedIncrement(&g_ttfTextDiagCount);
    if (event > 48) return;
    char hex[256] = {};
    size_t used = 0;
    const size_t bytes = length < 56 ? length : 56;
    for (size_t i = 0; i < bytes && used + 4 < sizeof(hex); ++i)
        used += (size_t)_snprintf_s(hex + used, sizeof(hex) - used, _TRUNCATE,
            "%02X ", (unsigned char)text[i]);
    char codepoints[256] = {};
    used = 0;
    if (decoded) {
        for (size_t i = 0; decoded[i] && i < 48 && used + 12 < sizeof(codepoints); ++i)
            used += (size_t)_snprintf_s(codepoints + used, sizeof(codepoints) - used, _TRUNCATE,
                "U+%04X ", (unsigned)decoded[i]);
    }
    (void)0;
}

struct TtfColorRun { std::wstring text; DWORD color; };

static DWORD GameColor(unsigned char code, DWORD fallback)
{
    switch (code) {
    case 'W': return 0xFFFFFFFF;
    case 'B': return 0xFF0000FF;
    case 'G': return 0xFF009F03;
    case 'R': return 0xFFFF3232;
    case 'b': return 0xFF000000;
    case 'g': return 0xFFB0B0B0;
    case 'Y': return 0xFFFFBD00;
    case '!': return fallback;
    default: return fallback;
    }
}

static void LogColorRunProbe(const char* reason, const char* text, size_t length,
                             DWORD format, const RECT* rect, int width, int measured)
{
    if (!g_api || !g_api->Log) return;
    const LONG index = InterlockedIncrement(&g_colorRunProbeCount);
    if (index > 24) return;
    char sample[160] = {};
    size_t used = 0;
    const size_t limit = !text ? 0 : (length < 36 ? length : 36);
    for (size_t i = 0; i < limit && used + 4 < sizeof(sample); ++i)
        used += (size_t)_snprintf_s(sample + used, sizeof(sample) - used, _TRUNCATE,
            "%02X ", (unsigned char)text[i]);
    (void)0;
}

static bool DecodeUtf8Lossy(const char* bytes, size_t length, std::wstring& output)
{
    if (!bytes) return false;
    output.clear();
    for (size_t i = 0; i < length;) {
        const unsigned char a = (unsigned char)bytes[i];
        if (a < 0x80) { output.push_back((wchar_t)a); ++i; continue; }
        size_t count = 0;
        uint32_t cp = 0;
        if (a >= 0xC2 && a <= 0xDF) { count = 2; cp = a & 0x1F; }
        else if (a >= 0xE0 && a <= 0xEF) { count = 3; cp = a & 0x0F; }
        else if (a >= 0xF0 && a <= 0xF4) { count = 4; cp = a & 0x07; }
        else { ++i; continue; }
        if (i + count > length) { ++i; continue; }
        bool valid = true;
        for (size_t j = 1; j < count; ++j) {
            const unsigned char b = (unsigned char)bytes[i + j];
            if (b < 0x80 || b > 0xBF) { valid = false; break; }
            cp = (cp << 6) | (b & 0x3F);
        }
        if (!valid || (count == 3 && ((a == 0xE0 && (unsigned char)bytes[i + 1] < 0xA0) ||
            (a == 0xED && (unsigned char)bytes[i + 1] >= 0xA0))) ||
            (count == 4 && ((a == 0xF0 && (unsigned char)bytes[i + 1] < 0x90) ||
            (a == 0xF4 && (unsigned char)bytes[i + 1] >= 0x90)))) { ++i; continue; }
        if (cp <= 0xFFFF) output.push_back((wchar_t)cp);
        else {
            cp -= 0x10000;
            output.push_back((wchar_t)(0xD800 + (cp >> 10)));
            output.push_back((wchar_t)(0xDC00 + (cp & 0x3FF)));
        }
        i += count;
    }
    return true;
}

static bool DrawUtf8ColorRuns(void* self, void* sprite, const char* text, size_t length,
                              RECT* rect, DWORD format, DWORD baseColor, int* drawResult)
{
    if (!self || !text || !rect || !g_originalDrawTextW || length > 4096) {
        LogColorRunProbe("invalid-arguments", text, length, format, rect, -1, -1);
        return false;
    }
    std::vector<TtfColorRun> runs;
    std::string bytes;
    DWORD currentColor = baseColor;
    bool hasColor = false;
    auto flush = [&]() -> bool {
        if (bytes.empty()) return true;
        MapUtf8ContinuationByte(bytes.data(), bytes.size(), 0x7F, 0x80);
        MapUtf8ContinuationByte(bytes.data(), bytes.size(), 0x1D, 0xBD);
        std::wstring decoded;
        if (!DecodeUtf8Lossy(bytes.data(), bytes.size(), decoded)) return false;
        if (!decoded.empty()) runs.push_back({std::move(decoded), currentColor});
        bytes.clear();
        return true;
    };
    for (size_t i = 0; i < length;) {
        size_t markerLength = 0;
        if ((unsigned char)text[i] == 0xA7) markerLength = 1;
        else if (i + 1 < length && (unsigned char)text[i] == 0xC2 &&
            (unsigned char)text[i + 1] == 0xA7) markerLength = 2;
        if (markerLength && i + markerLength < length && IsGameColorCode((unsigned char)text[i + markerLength])) {
            if (!flush()) return false;
            currentColor = GameColor((unsigned char)text[i + markerLength], baseColor);
            hasColor = true;
            i += markerLength + 1;
        } else {
            bytes.push_back(text[i++]);
        }
    }
    if (!flush()) {
        LogColorRunProbe("utf8-run-decode-failed", text, length, format, rect, -1, -1);
        return false;
    }
    if (!hasColor || runs.empty()) return false;

    void*** objectVtable = (void***)self;
    if (!objectVtable || !*objectVtable || !(*objectVtable)[8]) {
        LogColorRunProbe("font-vtable-no-getdc", text, length, format, rect, -1, -1);
        return false;
    }
    using FontGetDcFn = HDC (__stdcall*)(void*);
    HDC dc = ((FontGetDcFn)(*objectVtable)[8])(self);
    if (!dc) {
        LogColorRunProbe("getdc-failed", text, length, format, rect, -1, -1);
        return false;
    }
    std::vector<std::vector<TtfColorRun>> lines(1);
    auto appendRuns = [](std::vector<TtfColorRun>& destination,
                         const std::vector<TtfColorRun>& source) {
        for (const auto& run : source) {
            if (run.text.empty()) continue;
            if (!destination.empty() && destination.back().color == run.color)
                destination.back().text += run.text;
            else
                destination.push_back(run);
        }
    };
    auto runWidth = [](HDC dc, const std::vector<TtfColorRun>& source, int& width) -> bool {
        width = 0;
        for (const auto& run : source) {
            if (run.text.empty()) continue;
            SIZE extent = {};
            if (!GetTextExtentPoint32W(dc, run.text.data(), (int)run.text.size(), &extent))
                return false;
            width += extent.cx;
        }
        return true;
    };

    // Flatten the colored spans so words can cross color-code boundaries
    // without losing their individual colors during wrapping.
    struct StyledChar { wchar_t value; DWORD color; };
    std::vector<StyledChar> chars;
    for (const auto& run : runs)
        for (wchar_t ch : run.text) chars.push_back({ ch, run.color });

    int available = rect->right - rect->left;
    const bool calcRect = (format & 0x0400) != 0;
    const bool singleLine = (format & 0x0020) != 0;
    const bool wordBreak = (format & 0x0010) != 0 && !singleLine && available > 0;
    int currentLineWidth = 0;
    bool currentLineHasWord = false;
    std::vector<TtfColorRun> pendingWhitespace;
    int pendingWhitespaceWidth = 0;
    for (size_t i = 0; i < chars.size();) {
        const wchar_t ch = chars[i].value;
        if (!singleLine && (ch == L'\r' || ch == L'\n')) {
            appendRuns(lines.back(), pendingWhitespace);
            currentLineWidth += pendingWhitespaceWidth;
            pendingWhitespace.clear();
            pendingWhitespaceWidth = 0;
            if (ch == L'\r' && i + 1 < chars.size() && chars[i + 1].value == L'\n') ++i;
            lines.emplace_back();
            currentLineWidth = 0;
            currentLineHasWord = false;
            ++i;
            continue;
        }
        if (singleLine && (ch == L'\r' || ch == L'\n')) {
            if (pendingWhitespace.empty()) pendingWhitespaceWidth = 0;
            pendingWhitespace.push_back({ std::wstring(1, L' '), chars[i].color });
            ++i;
            continue;
        }
        if (iswspace(ch)) {
            const size_t color = (size_t)chars[i].color;
            if (!pendingWhitespace.empty() && (size_t)pendingWhitespace.back().color == color)
                pendingWhitespace.back().text.push_back(ch);
            else
                pendingWhitespace.push_back({ std::wstring(1, ch), chars[i].color });
            SIZE extent = {};
            if (!GetTextExtentPoint32W(dc, &ch, 1, &extent)) return false;
            pendingWhitespaceWidth += extent.cx;
            ++i;
            continue;
        }

        std::vector<TtfColorRun> word;
        size_t end = i;
        while (end < chars.size() && !iswspace(chars[end].value)) {
            const wchar_t value = chars[end].value;
            if (word.empty() || word.back().color != chars[end].color)
                word.push_back({ std::wstring(1, value), chars[end].color });
            else
                word.back().text.push_back(value);
            ++end;
        }
        int wordWidth = 0;
        if (!runWidth(dc, word, wordWidth)) return false;
        if (wordBreak && currentLineHasWord &&
            currentLineWidth + pendingWhitespaceWidth + wordWidth > available) {
            // Break at the preceding whitespace. Keep the whole word together,
            // matching DT_WORDBREAK even when a single word exceeds the box.
            lines.emplace_back();
            currentLineWidth = 0;
            currentLineHasWord = false;
            pendingWhitespace.clear();
            pendingWhitespaceWidth = 0;
        } else {
            appendRuns(lines.back(), pendingWhitespace);
            currentLineWidth += pendingWhitespaceWidth;
            pendingWhitespace.clear();
            pendingWhitespaceWidth = 0;
        }
        appendRuns(lines.back(), word);
        currentLineWidth += wordWidth;
        currentLineHasWord = true;
        i = end;
    }
    appendRuns(lines.back(), pendingWhitespace);
    TEXTMETRICW metrics = {};
    if (!GetTextMetricsW(dc, &metrics)) {
        LogColorRunProbe("font-metrics-failed", text, length, format, rect, -1, -1);
        return false;
    }
    const int lineHeight = metrics.tmHeight + metrics.tmExternalLeading > 0
        ? metrics.tmHeight + metrics.tmExternalLeading : 1;
    std::vector<int> lineWidths(lines.size(), 0);
    int maxLineWidth = 0;
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        for (const auto& run : lines[lineIndex]) {
            SIZE extent = {};
            if (!GetTextExtentPoint32W(dc, run.text.data(), (int)run.text.size(), &extent)) {
                LogColorRunProbe("run-measure-failed", text, length, format, rect, -1, -1);
                return false;
            }
            lineWidths[lineIndex] += extent.cx;
        }
        if (lineWidths[lineIndex] > maxLineWidth) maxLineWidth = lineWidths[lineIndex];
    }
    available = rect->right - rect->left;
    if (available <= 0 && !calcRect) {
        LogColorRunProbe("empty-rect", text, length, format, rect, available, maxLineWidth);
        return false;
    }

    if (calcRect) {
        // Match D3DX's CALCRECT contract, but use decoded TTF glyph metrics.
        // For a wrapped/multiline block, preserve the supplied width and grow
        // the bottom; a single line also reports its measured width.
        const int totalHeight = lineHeight * (int)lines.size();
        if (lines.size() == 1) rect->right = rect->left + lineWidths[0];
        rect->bottom = rect->top + totalHeight;
        if (drawResult) *drawResult = totalHeight;
        return true;
    }

    int y = rect->top;
    const int availableHeight = rect->bottom - rect->top;
    const int totalHeight = lineHeight * (int)lines.size();
    if ((format & 0x0004) && totalHeight < availableHeight) y += (availableHeight - totalHeight) / 2; // DT_VCENTER
    else if ((format & 0x0008) && totalHeight < availableHeight) y += availableHeight - totalHeight; // DT_BOTTOM
    DWORD segmentFormat = format & ~(0x0001 | 0x0002 | 0x0004 | 0x0008 | 0x0010 | 0x0400); // own line layout
    segmentFormat |= 0x0020; // DT_SINGLELINE
    int spanCount = 0;
    int maxDrawResult = 0;
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        int cursor = rect->left;
        if (format & 0x0002) cursor = rect->right - lineWidths[lineIndex];       // DT_RIGHT
        else if (format & 0x0001) cursor = rect->left + (available - lineWidths[lineIndex]) / 2; // DT_CENTER
        for (const auto& run : lines[lineIndex]) {
            SIZE extent = {};
            if (!GetTextExtentPoint32W(dc, run.text.data(), (int)run.text.size(), &extent)) {
                LogColorRunProbe("run-measure-failed", text, length, format, rect, available, maxLineWidth);
                return false;
            }
            RECT segment = *rect;
            segment.left = cursor;
            segment.right = cursor + extent.cx + 2;
            if (segment.right > rect->right) segment.right = rect->right;
            segment.top = y + (int)lineIndex * lineHeight;
            segment.bottom = segment.top + lineHeight;
            if (segment.left < segment.right && segment.top < rect->bottom) {
                const int drawn = ((DrawTextWFn)g_originalDrawTextW)(self, sprite, run.text.c_str(), -1,
                    &segment, segmentFormat, run.color);
                if (drawn > maxDrawResult) maxDrawResult = drawn;
                ++spanCount;
            }
            cursor += extent.cx;
        }
    }
    if (g_api && g_api->Log && InterlockedCompareExchange(&g_colorRunDrawLogged, 1, 0) == 0)
        (void)0;
    LogColorRunProbe("rendered", text, length, format, rect, available, maxLineWidth);
    if (drawResult) *drawResult = maxDrawResult;
    return true;
}

static int __stdcall HookDrawTextW(void* self, void* sprite, const wchar_t* text,
    int count, RECT* rect, DWORD format, DWORD color)
{
    DrawTextWFn original = (DrawTextWFn)g_originalDrawTextW;
    if (!original) return 0;
    color = ResolveGuiFontDrawColor(self, color);
    const int result = original(self, sprite, text, count, rect, format, color);
    if (text) {
        const size_t length = count < 0 ? wcslen(text) : (size_t)count;
        if (length <= 512) LogArmyEditboxRender(self, nullptr, 0, text, "direct-DrawTextW", result, rect, format);
    }
    return result;
}
static int __stdcall HookDrawTextA(void* self, void* sprite, const char* text,
    int count, RECT* rect, DWORD format, DWORD color)
{
    color = ResolveGuiFontDrawColor(self, color);
    size_t textLength = 0;
    if (text) textLength = count < 0 ? strlen(text) : (size_t)count;
    if (text && textLength <= 512)
        LogArmyEditboxRender(self, text, textLength, nullptr, "DrawTextA-input", -1, rect, format);
    if (text && count >= 0 && count <= 256 && g_api && g_api->Log) {
        const size_t accessibleLength = SafeBoundedTextLength(text, 4096);
        if (accessibleLength && HasEventColorProbeMarker(text, accessibleLength)) {
            static volatile LONG drawColorTraceCount = 0;
            const LONG trace = InterlockedIncrement(&drawColorTraceCount);
            if (trace <= 8) {
                char sample[320] = {};
                size_t used = 0;
                const size_t limit = accessibleLength < 96 ? accessibleLength : 96;
                for (size_t i = 0; i < limit && used + 4 < sizeof(sample); ++i)
                    used += (size_t)_snprintf_s(sample + used, sizeof(sample) - used, _TRUNCATE,
                        "%02X ", (unsigned char)text[i]);
                (void)0;
            }
        }
    }
    LogUnicodeTrace("DrawTextA-entry", text, count, (uintptr_t)_ReturnAddress());
    int colorRunResult = 0;
    if (text && textLength && DrawUtf8ColorRuns(self, sprite, text, textLength, rect, format, color, &colorRunResult)) {
        LogArmyEditboxRender(self, text, textLength, nullptr, "DrawTextA-color-runs", colorRunResult, rect, format);
        return colorRunResult;
    }
    if (text && textLength >= 8 && memcmp(text, "UTF8HEX:", 8) == 0) {
        size_t offset = 8;
        while (offset < textLength && (text[offset] == ' ' || text[offset] == '\t')) ++offset;
        size_t payloadLength = textLength - offset;
        if (payloadLength && text[offset + payloadLength - 1] == '\0') --payloadLength;
        std::vector<wchar_t> wide;
        if (DecodeUtf8Hex(text + offset, payloadLength, wide) && g_originalDrawTextW) {
            LogUnicodeTrace("UTF8HEX-to-DrawTextW", text, count, (uintptr_t)_ReturnAddress());
            if (g_api && g_api->Log && InterlockedCompareExchange(&g_utf8HexSuccessLogged, 1, 0) == 0)
                (void)0;
            const int drawResult = ((DrawTextWFn)g_originalDrawTextW)(self, sprite, wide.data(), -1, rect, format, color);
            LogArmyEditboxRender(self, text, textLength, wide.data(), "UTF8HEX-to-DrawTextW", drawResult, rect, format);
            return drawResult;
        }
        if (g_api && g_api->Log && InterlockedCompareExchange(&g_utf8HexFailureLogged, 1, 0) == 0) {
            char sample[160] = {};
            size_t used = 0;
            const size_t limit = payloadLength < 48 ? payloadLength : 48;
            for (size_t i = 0; i < limit && used + 4 < sizeof(sample); ++i)
                used += (size_t)_snprintf_s(sample + used, sizeof(sample) - used, _TRUNCATE,
                    "%02X ", (unsigned char)text[offset + i]);
            (void)0;
        }
        const int ansiResult = ((DrawTextAFn)g_originalDrawTextA)(self, sprite, text, count, rect, format, color);
    LogArmyEditboxRender(self, text, textLength, nullptr, "DrawTextA-original-ANSI", ansiResult, rect, format);
    return ansiResult;
    }
    // Unmarked UTF-8 is the normal path for the selected Unicode TTF. The
    // parser workaround uses DEL for any 0x80 continuation byte; restore it
    // only when it forms a valid UTF-8 sequence. Invalid legacy ANSI remains
    // on the original DrawTextA path.
    if (text && textLength && !StartsWithUnicodeProbe(text) && g_originalDrawTextW) {
        std::vector<char> restored(text, text + textLength);
        MapUtf8ContinuationByte(restored.data(), restored.size(), 0x7F, 0x80);
        MapUtf8ContinuationByte(restored.data(), restored.size(), 0x1D, 0xBD);
        bool hasHighByte = false;
        for (char c : restored) if ((unsigned char)c >= 0x80) { hasHighByte = true; break; }
        std::vector<wchar_t> wide;
        if (hasHighByte) {
            if (DecodeUtf8(restored.data(), restored.size(), wide)) {
                const bool repaired = RepairUtf8ReadAsWindows1252(wide);
                LogTtfDrawBytes(self, text, textLength, wide.data(), repaired, "valid-utf8");
                LogUnicodeTrace("automatic-UTF8-to-DrawTextW", text, count, (uintptr_t)_ReturnAddress());
                if (g_api && g_api->Log) {
                    const LONG fontObject = (LONG)(uintptr_t)self;
                    bool alreadyLogged = false;
                    for (size_t i = 0; i < _countof(g_autoDrawObjects); ++i) {
                        LONG seen = InterlockedCompareExchange(&g_autoDrawObjects[i], 0, 0);
                        if (seen == fontObject) { alreadyLogged = true; break; }
                        if (seen == 0 && InterlockedCompareExchange(&g_autoDrawObjects[i], fontObject, 0) == 0) {
                            InterlockedIncrement(&g_autoDrawObjectCount);
                            (void)0;
                            alreadyLogged = true;
                            break;
                        }
                    }
                    (void)alreadyLogged;
                }
                const int drawResult = ((DrawTextWFn)g_originalDrawTextW)(self, sprite, wide.data(), -1, rect, format, color);
                LogArmyEditboxRender(self, text, textLength, wide.data(), "valid-UTF8-to-DrawTextW", drawResult, rect, format);
                return drawResult;
            }
            LogTtfDrawBytes(self, text, textLength, nullptr, false, "invalid-utf8-kept-as-ansi");
        }
    }
    if (text && textLength >= 5 && memcmp(text, "UTF8:", 5) == 0) {
        size_t offset = 5;
        while (offset < textLength && (text[offset] == ' ' || text[offset] == '\t')) ++offset;
        size_t payloadLength = textLength - offset;
        if (payloadLength && text[offset + payloadLength - 1] == '\0') --payloadLength;
        std::vector<char> restored;
        restored.assign(text + offset, text + offset + payloadLength);
        const size_t restoredCount = MapUtf8ContinuationByte(restored.data(), restored.size(), 0x7F, 0x80);
        MapUtf8ContinuationByte(restored.data(), restored.size(), 0x1D, 0xBD);
        std::vector<wchar_t> wide;
        if (DecodeUtf8(restored.data(), restored.size(), wide) && g_originalDrawTextW) {
            LogUnicodeTrace("UTF8-to-DrawTextW", text, count, (uintptr_t)_ReturnAddress());
            if (restoredCount && g_api && g_api->Log &&
                InterlockedCompareExchange(&g_utf8RestoreLogged, 1, 0) == 0)
                (void)0;
            if (g_api && g_api->Log && InterlockedCompareExchange(&g_utf8RawSuccessLogged, 1, 0) == 0)
                (void)0;
            const int drawResult = ((DrawTextWFn)g_originalDrawTextW)(self, sprite, wide.data(), -1, rect, format, color);
            LogArmyEditboxRender(self, text, textLength, wide.data(), "UTF8-tag-to-DrawTextW", drawResult, rect, format);
            return drawResult;
        }
        if (g_api && g_api->Log && InterlockedCompareExchange(&g_utf8RawFailureLogged, 1, 0) == 0) {
            char hex[256] = {};
            size_t used = 0;
            const size_t limit = payloadLength < 64 ? payloadLength : 64;
            for (size_t i = 0; i < limit && used + 4 < sizeof(hex); ++i)
                used += (size_t)_snprintf_s(hex + used, sizeof(hex) - used, _TRUNCATE,
                    "%02X ", (unsigned char)text[offset + i]);
            const uintptr_t caller = (uintptr_t)_ReturnAddress();
            const uintptr_t imageBase = (uintptr_t)GetModuleHandleW(nullptr);
            const DWORD callerRva = imageBase && caller >= imageBase ? (DWORD)(caller - imageBase) : 0;
            (void)0;
        }
    }
    // Experimental edit-box probe: this unique 15px Arial font is assigned
    // only to unitpanel/unitname. Russian keyboard input may reach the
    // ANSI control as CP1251; decode that byte string to UTF-16 for TTF draw.
    if (text && textLength && textLength <= 512 &&
        _stricmp(TtfDiagnosticFace(self) ? TtfDiagnosticFace(self) : "", "Arial") == 0 &&
        TtfDiagnosticHeight(self) == 15 && g_originalDrawTextW) {
        bool hasHighByte = false;
        for (size_t i = 0; i < textLength; ++i)
            if ((unsigned char)text[i] >= 0x80) { hasHighByte = true; break; }
        if (hasHighByte) {
            const int required = MultiByteToWideChar(1251, 0, text, (int)textLength, nullptr, 0);
            if (required > 0 && required <= 512) {
                std::vector<wchar_t> wide((size_t)required + 1, L'\0');
                if (MultiByteToWideChar(1251, 0, text, (int)textLength, wide.data(), required) == required) {
                    bool hasCyrillic = false;
                    for (int i = 0; i < required; ++i) {
                        const wchar_t ch = wide[(size_t)i];
                        if ((ch >= 0x0400 && ch <= 0x052F) || (ch >= 0x2DE0 && ch <= 0x2DFF) ||
                            (ch >= 0xA640 && ch <= 0xA69F)) { hasCyrillic = true; break; }
                    }
                    if (hasCyrillic) {
                        static volatile LONG editBoxFallbackLogged = 0;
                        if (g_api && g_api->Log && InterlockedCompareExchange(&editBoxFallbackLogged, 1, 0) == 0) {
                            char sample[128] = {};
                            size_t used = 0;
                            const size_t limit = textLength < 24 ? textLength : 24;
                            for (size_t i = 0; i < limit && used + 4 < sizeof(sample); ++i)
                                used += (size_t)_snprintf_s(sample + used, sizeof(sample) - used, _TRUNCATE,
                                    "%02X ", (unsigned char)text[i]);
                            const DWORD callerRva = TraceCallerRva((uintptr_t)_ReturnAddress());
                            (void)0;
                        }
                        const int drawResult = ((DrawTextWFn)g_originalDrawTextW)(self, sprite, wide.data(), -1, rect, format, color);
                        LogArmyEditboxRender(self, text, textLength, wide.data(), "CP1251-to-DrawTextW", drawResult, rect, format);
                        return drawResult;
                    }
                }
            }
        }
    }
    const int ansiResult = ((DrawTextAFn)g_originalDrawTextA)(self, sprite, text, count, rect, format, color);
    LogArmyEditboxRender(self, text, textLength, nullptr, "DrawTextA-original-ANSI", ansiResult, rect, format);
    return ansiResult;
}

static void InstallUtf8OnFontObject(void* fontConfig)
{
    // The engine stores ID3DXFont* at +0x30 and the face name at +0x34.
    const char* face = (const char*)((const unsigned char*)fontConfig + 0x34);
    bool isUnicodeFace = false;
    for (size_t i = 0; i < g_unicodeFontCount; ++i)
        if (_stricmp(face, g_unicodeFonts[i].face) == 0) { isUnicodeFace = true; break; }
    if (!isUnicodeFace) return;
    void* fontObject = *(void**)((unsigned char*)fontConfig + 0x30);
    if (!fontObject) return;
    void*** objectVtable = (void***)fontObject;
    void** originalVtable = *objectVtable;
    if (!originalVtable || !originalVtable[kD3DXDrawTextASlot] || !originalVtable[kD3DXDrawTextWSlot]) return;
    void** privateVtable = (void**)HeapAlloc(GetProcessHeap(), 0,
        sizeof(void*) * kD3DXFontVtableSlots);
    if (!privateVtable) return;
    memcpy(privateVtable, originalVtable, sizeof(void*) * kD3DXFontVtableSlots);
    g_originalDrawTextA = privateVtable[kD3DXDrawTextASlot];
    g_originalDrawTextW = privateVtable[kD3DXDrawTextWSlot];
    privateVtable[kD3DXDrawTextASlot] = (void*)&HookDrawTextA;
    privateVtable[kD3DXDrawTextWSlot] = (void*)&HookDrawTextW;
    DWORD oldProtect = 0;
    if (!VirtualProtect(objectVtable, sizeof(void*), PAGE_READWRITE, &oldProtect)) {
        HeapFree(GetProcessHeap(), 0, privateVtable);
        return;
    }
    *objectVtable = privateVtable;
    DWORD ignored = 0;
    VirtualProtect(objectVtable, sizeof(void*), oldProtect, &ignored);
    RegisterTtfDiagnosticFont(fontObject, face);
    if (g_api && g_api->Log)
        (void)0;
}

static bool ApplyGuiFontQualityPolicy(void* fontConfig)
{
    if (!fontConfig) return false;
    void** slot = (void**)((unsigned char*)fontConfig + 0x30);
    void* font = *slot;
    if (!font) return false;
    void** vtable = *(void***)font;
    if (!vtable || !vtable[2] || !vtable[4]) return false;
    using GetDescFn = HRESULT (WINAPI*)(void*, D3DXFontDescA*);
    D3DXFontDescA desc{};
    if (FAILED(((GetDescFn)vtable[4])(font, &desc))) return false;

    const char* configFace = (const char*)((const unsigned char*)fontConfig + 0x34);
    const char* face = desc.faceName[0] ? desc.faceName : configFace;
    const int height = desc.height < 0 ? -desc.height : desc.height;
    const GuiFontQualityPolicy* match = nullptr;
    bool policyMatched = false;
    for (const auto& policy : g_guiFontQualityPolicies) {
        if (policy.height != height || _stricmp(policy.family, face) != 0) continue;
        policyMatched = true;
        if (policy.quality == 0xFF || match) return true; // Ambiguous: preserve engine quality and suppress legacy fallback.
        match = &policy;
    }
    if (!policyMatched || !match) return false;
    if (desc.quality == match->quality) return true;

    HMODULE d3dx = GetModuleHandleW(L"d3dx9_41.dll");
    using CreateFn = HRESULT (WINAPI*)(void*, const D3DXFontDescA*, void**);
    using ReleaseFn = ULONG (WINAPI*)(void*);
    auto create = d3dx ? (CreateFn)GetProcAddress(d3dx, "D3DXCreateFontIndirectA") : nullptr;
    if (!create) return true;
    desc.quality = match->quality;
    void* replacement = nullptr;
    if (SUCCEEDED(create(*(void**)((unsigned char*)fontConfig + 0x28), &desc, &replacement)) && replacement) {
        *slot = replacement;
        ((ReleaseFn)vtable[2])(font);
    }
    return true;
}
static void __fastcall HookFontCreate(void* fontConfig, void*)
{
    ((FontCreateFn)g_fontCreateOriginal)(fontConfig);
    const char* face = (const char*)((unsigned char*)fontConfig + 0x34);
    const bool guiQualityPolicyApplied = ApplyGuiFontQualityPolicy(fontConfig);
    if (!guiQualityPolicyApplied && _stricmp(face, "Times NR Bold") == 0) {
        struct FontDesc { int height; unsigned width, weight, mipLevels; BOOL italic; BYTE charset, outputPrecision, quality, pitchAndFamily; char faceName[32]; } desc = {};
        void** slot = (void**)((unsigned char*)fontConfig + 0x30);
        void* font = *slot;
        HMODULE d3dx = GetModuleHandleW(L"d3dx9_41.dll");
        using CreateFn = HRESULT (WINAPI*)(void*, const FontDesc*, void**);
        auto create = d3dx ? (CreateFn)GetProcAddress(d3dx, "D3DXCreateFontIndirectA") : nullptr;
        if (font && create) {
            void** vtable = *(void***)font;
            using GetDescFn = HRESULT (WINAPI*)(void*, FontDesc*);
            using ReleaseFn = ULONG (WINAPI*)(void*);
            if (SUCCEEDED(((GetDescFn)vtable[4])(font, &desc))) {
                const BYTE originalQuality = desc.quality;
                desc.weight = 700;
                desc.italic = FALSE;
                // Experimental UI-only ClearType comparison. Keep this easy
                // to revert because subpixel color fringes may show under scaling.
                desc.quality = CLEARTYPE_QUALITY;
                strcpy_s(desc.faceName, "Times New Roman");
                void* replacement = nullptr;
                if (SUCCEEDED(create(*(void**)((unsigned char*)fontConfig + 0x28), &desc, &replacement)) && replacement) {
                    *slot = replacement;
                    ((ReleaseFn)vtable[2])(font);
                    if (g_api && g_api->Log)
                        (void)0;
                }
            }
        }
    }
    InstallUtf8OnFontObject(fontConfig);
}

// CC EDITBOX UNICODE input hooks: subclass the ANSI game window with a Unicode WndProc.
static constexpr uint32_t kEditInputWndProcRva = 0x005DFC70;
static constexpr uint32_t kEditInputGateVtableRva = 0x00A43854;
static constexpr uint32_t kEditInputGateRva = 0x00679AF0;
static constexpr int kEditInputGateSlot = 12; // +0x30: MOV AL,[ECX+0x51]; RET
static constexpr uint32_t kGameEventQueueVtableRva = 0x00A0A34C;
static constexpr uint32_t kGameEventEnqueueRva = 0x0026A580;
static constexpr int kGameEventEnqueueSlot = 8; // +0x20: append event node
static constexpr uint32_t kGameInputPassRva = 0x002592F0; // FUN_006592f0: adjacent snapshot point, not a known queue consumer
static constexpr uint32_t kGameInputConsumerRva = 0x00257B60; // FUN_00657b60: queued event consumer
static const unsigned char kGameInputPassPrologue[5] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };
static constexpr uint32_t kEditInputStringRva = 0x00588E80;
static const unsigned char kEditInputWndProcPrologue[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 };
static const unsigned char kEditInputStringPrologue[9] = { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
static constexpr uint32_t kEditInsertTextRva = 0x005F5640;
static constexpr uint32_t kEditControlDispatchRva = 0x005F5320;
// Army panel FUN_00798a50 sets the named unitname editor through virtual +0x40.
static constexpr uint32_t kEditControlSetTextRva = 0x005F5290;
// FUN_009f7cb0 removes one byte at an edit-control byte offset. Its callers
// are the vanilla Backspace and Delete paths; v26 checks call return addresses
// and expands valid UTF-8 deletes
// only for the captured army-name edit control.
static constexpr uint32_t kEditUtf8ByteDeleteRva = 0x005F7CB0;
// The MOV EAX, FS:[0] instruction spans bytes 5..10. Stealing 9 bytes
// would split that instruction in the trampoline and crash on the first call.
static const unsigned char kEditUtf8ByteDeletePrologue[11] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
// Runtime v23 trace confirmed the unitpanel's unitname setter call returns here.
static constexpr uint32_t kUnitnameSetterReturnRva = 0x00398F20;
// FUN_009b6010: editbox keyboard-event dispatcher; compares a narrowed char
// against fixed and per-control reserved-byte lists before calling the editor.
static constexpr uint32_t kEditboxReservedFilterRva = 0x005B6010;
static const unsigned char kEditboxReservedFilterPrologue[5] = { 0x55, 0x8B, 0xEC, 0x53, 0x56 };
// Temporary probe: U+0440 narrows to ASCII '@' in FUN_009b6010's reserved-byte check.
// U+F032 keeps the same accepted low byte as U+0432 but remains distinguishable
// when the game's queued editor callback runs after the WndProc route returns.
static constexpr uint32_t kEditboxReservedCollisionScalar = 0x0440;
static constexpr uint32_t kEditboxReservedCollisionProbeScalar = 0xF032;
static const unsigned char kEditInsertTextPrologue[11] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x2C, 0x56, 0x57, 0x8B, 0x7D, 0x08 };
static const unsigned char kEditControlDispatchPrologue[9] = { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
static const unsigned char kEditControlSetTextPrologue[9] = { 0x55, 0x8B, 0xEC, 0x51, 0x56, 0x8B, 0xF1, 0x6A, 0x00 };
static unsigned char g_editInputWndProcTrampoline[64];
static unsigned char g_editInputStringTrampoline[64];
static unsigned char g_editInsertTextTrampoline[64];
static unsigned char g_editControlDispatchTrampoline[64];
static unsigned char g_editControlSetTextTrampoline[64];
static unsigned char g_editUtf8ByteDeleteTrampoline[64];
static unsigned char g_editboxReservedFilterTrampoline[64];
static void* g_editInputWndProcOriginal = nullptr;
static void* g_editInputGateOriginal = nullptr;
static void* g_gameEventEnqueueOriginal = nullptr;
static volatile LONG g_editInputGateTraceCount = 0;
static volatile LONG g_gameEventQueueTraceCount = 0;
static unsigned char g_gameInputPassTrampoline[64];
static unsigned char g_gameInputConsumerTrampoline[64];
static void* g_gameInputPassOriginal = nullptr;
static void* g_gameInputConsumerOriginal = nullptr;
static volatile LONG g_gameInputPassTraceCount = 0;
static volatile LONG g_gameInputConsumerTraceCount = 0;
static volatile LONG g_gameInputFlowRecordCount = 0;
static void* g_editInputStringOriginal = nullptr;
static void* g_editInsertTextOriginal = nullptr;
static void* g_editControlDispatchOriginal = nullptr;
static void* g_editControlSetTextOriginal = nullptr;
static void* g_editUtf8ByteDeleteOriginal = nullptr;
static void* volatile g_armyNameEditControl = nullptr;
static volatile LONG g_armyEditTraceCount = 0;
static volatile LONG g_editControlSetTextTraceCount = 0;
static volatile LONG g_editUtf8DeleteTraceCount = 0;
static volatile LONG g_editUtf8DeleteCallerTraceCount = 0;
static void* volatile g_editUtf8DeleteDispatchSelf = nullptr;
static volatile LONG g_editUtf8DeleteExtraLeft = 0;
static void* g_editboxReservedFilterOriginal = nullptr;
typedef LRESULT (WINAPI *EditInputWndProcFn)(HWND, UINT, WPARAM, LPARAM);
typedef char* (__stdcall *EditInputStringFn)(char*, uint32_t);
static WNDPROC g_editUnicodePreviousProc = nullptr;
static HWND g_editUnicodeSubclassedHwnd = nullptr;
static volatile LONG g_editUnicodeSubclassAttempted = 0;
static volatile LONG g_editUnicodeDispatchActive = 0;
static volatile LONG g_editUnicodeAltGrDispatchActive = 0;
static volatile LONG g_editUnicodeAltGrShiftActive = 0;
static volatile LONG g_editInputTraceActive = 0;
static volatile LONG g_editInsertTraceCount = 0;
static volatile LONG g_editStringCtorTraceCount = 0;
static volatile uint32_t g_editUnicodePendingScalar = 0;
static volatile LONG g_editUnicodeCharLogCount = 0;
static volatile LONG g_editUnicodeKeyLogCount = 0;
static volatile LONG g_editKeyboardRouteLogCount = 0;
static volatile LONG g_editEditorEventLogCount = 0;
static volatile LONG g_editInputImeLogged = 0;
static volatile LONG g_editInputUnicodeLogged = 0;
static volatile LONG g_editInputEncodeLogged = 0;
static uint16_t g_editPendingHighSurrogate = 0;
struct EditKeyboardScalarProbe { uint32_t scalar; uint8_t scan; uint8_t extended; uint8_t altGr; uint8_t shift; };
static EditKeyboardScalarProbe g_editKeyboardScalarQueue[64] = {};
static volatile LONG g_editKeyboardScalarRead = 0;
static volatile LONG g_editKeyboardScalarWrite = 0;

static volatile LONG g_editKeyboardQueueTraceCount = 0;
static void QueueEditKeyboardScalar(uint32_t scalar, uint8_t scan, uint8_t extended, uint8_t altGr, uint8_t shift)
{
    LONG write = InterlockedCompareExchange(&g_editKeyboardScalarWrite, 0, 0);
    LONG read = InterlockedCompareExchange(&g_editKeyboardScalarRead, 0, 0);
    LONG dropped = 0;
    if (write - read >= 64) {
        read = write - 63;
        InterlockedExchange(&g_editKeyboardScalarRead, read);
        dropped = 1;
    }
    g_editKeyboardScalarQueue[write & 63] = { scalar, scan, extended, altGr, shift };
    MemoryBarrier();
    InterlockedExchange(&g_editKeyboardScalarWrite, write + 1);
    LONG trace = InterlockedIncrement(&g_editKeyboardQueueTraceCount);
    if (trace <= 512 && g_api && g_api->Log)
        (void)0;
}

static bool TakeEditKeyboardScalar(uint8_t scan, uint8_t extended, EditKeyboardScalarProbe* result)
{
    LONG read = InterlockedCompareExchange(&g_editKeyboardScalarRead, 0, 0);
    LONG write = InterlockedCompareExchange(&g_editKeyboardScalarWrite, 0, 0);
    LONG trace = InterlockedIncrement(&g_editKeyboardQueueTraceCount);
    if (read >= write) {
        if (trace <= 512 && g_api && g_api->Log)
            (void)0;
        return false;
    }
    EditKeyboardScalarProbe candidate = g_editKeyboardScalarQueue[read & 63];
    if (candidate.scan != scan || candidate.extended != extended) {
        if (trace <= 512 && g_api && g_api->Log)
            (void)0;
        return false;
    }
    if (result) *result = candidate;
    MemoryBarrier();
    InterlockedExchange(&g_editKeyboardScalarRead, read + 1);
    if (trace <= 512 && g_api && g_api->Log)
        (void)0;
    return true;
}

static bool DispatchLegacyByteToGame(HWND hwnd, uint8_t ch, LPARAM lParam)
{
    if (!g_editInputWndProcOriginal) return false;
    LONG previousTrace = InterlockedExchange(&g_editInputTraceActive, 1);
    ((EditInputWndProcFn)g_editInputWndProcOriginal)(hwnd, WM_CHAR, (WPARAM)ch, lParam);
    InterlockedExchange(&g_editInputTraceActive, previousTrace);
    return true;
}

static LRESULT WINAPI HookEditInputWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
static void LogEditRouteObjects(const char* phase, uint32_t scalar, HWND hwnd)
{
    uintptr_t root = 0, focusSlot = 0, focus = 0, focusVtable = 0;
    uintptr_t focus28 = 0, focus34 = 0, inputSlot = 0, inputTable = 0;
    uintptr_t input10 = 0, input30 = 0, eventSlot = 0, eventTable = 0, event20 = 0;
    unsigned inputGateByte = 0xFF;
    LONG gameQueueCount = -1;
    uintptr_t gameQueueHead = 0, gameQueueTail = 0;
    int readable = 0;
    uintptr_t imageBase = (uintptr_t)GetModuleHandleW(nullptr);
    __try {
        root = *(const uintptr_t*)(imageBase + 0x00B20C38);
        if (root) {
            focusSlot = *(const uintptr_t*)(root + 0x80);
            if (focusSlot) {
                focus = *(const uintptr_t*)focusSlot;
                if (focus) {
                    focusVtable = *(const uintptr_t*)focus;
                    if (focusVtable) { focus28 = *(const uintptr_t*)(focusVtable + 0x28); focus34 = *(const uintptr_t*)(focusVtable + 0x34); }
                }
            }
            inputSlot = *(const uintptr_t*)(root + 0xE8);
            if (inputSlot) {
                inputTable = *(const uintptr_t*)inputSlot;
                if (inputTable) { input10 = *(const uintptr_t*)(inputTable + 0x10); input30 = *(const uintptr_t*)(inputTable + 0x30); }
            inputGateByte = *(const uint8_t*)(inputSlot + 0x51);
            }
            eventSlot = *(const uintptr_t*)(root + 0x84);
            if (eventSlot) {
                eventTable = *(const uintptr_t*)eventSlot;
                if (eventTable) event20 = *(const uintptr_t*)(eventTable + 0x20);
            gameQueueHead = *(const uintptr_t*)(eventSlot + 0x1CF8);
            gameQueueTail = *(const uintptr_t*)(eventSlot + 0x1CFC);
            gameQueueCount = *(const LONG*)(eventSlot + 0x1D00);
            }
            readable = 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { readable = -1; }
    if (g_api && g_api->Log)
        (void)0;
}
typedef uint8_t (__thiscall *EditInputGateFn)(void*);
static uint8_t __fastcall HookEditInputGate(void* self, void*)
{
    EditInputGateFn original = (EditInputGateFn)g_editInputGateOriginal;
    if (!original) return 0;
    const LONG routeActive = InterlockedCompareExchange(&g_editUnicodeDispatchActive, 0, 0);
    const LONG altGrActive = InterlockedCompareExchange(&g_editUnicodeAltGrDispatchActive, 0, 0);
    const LONG altGrShiftActive = InterlockedCompareExchange(&g_editUnicodeAltGrShiftActive, 0, 0);
    const bool bypassAltGrGate = routeActive != 0 && altGrActive != 0;
    const uint8_t result = bypassAltGrGate ? 0 : original(self);
    if (!routeActive) return result;
    const LONG trace = InterlockedIncrement(&g_editInputGateTraceCount);
    if (trace <= 512 && g_api && g_api->Log) {
        uint8_t stateByte = 0xFF;
        __try { stateByte = *(const volatile uint8_t*)((const unsigned char*)self + 0x51); }
        __except (EXCEPTION_EXECUTE_HANDLER) { stateByte = 0xFE; }
        (void)0;
    }
    return result;
}

typedef void (__thiscall *GameEventEnqueueFn)(void*, const uint32_t*);
static void __fastcall HookGameEventEnqueue(void* self, void*, const uint32_t* eventData)
{
    GameEventEnqueueFn original = (GameEventEnqueueFn)g_gameEventEnqueueOriginal;
    if (!original) return;
    uint32_t wParam = 0, message = 0;
    LONG countBefore = -1, countAfter = -1;
    uintptr_t headBefore = 0, tailBefore = 0, headAfter = 0, tailAfter = 0;
    __try {
        if (eventData) { wParam = eventData[0]; message = eventData[1]; }
        countBefore = *(const volatile LONG*)((const unsigned char*)self + 0x1D00);
        headBefore = *(const uintptr_t*)((const unsigned char*)self + 0x1CF8);
        tailBefore = *(const uintptr_t*)((const unsigned char*)self + 0x1CFC);
    } __except (EXCEPTION_EXECUTE_HANDLER) { countBefore = -2; }
    original(self, eventData);
    __try {
        countAfter = *(const volatile LONG*)((const unsigned char*)self + 0x1D00);
        headAfter = *(const uintptr_t*)((const unsigned char*)self + 0x1CF8);
        tailAfter = *(const uintptr_t*)((const unsigned char*)self + 0x1CFC);
    } __except (EXCEPTION_EXECUTE_HANDLER) { countAfter = -2; }
    if (InterlockedCompareExchange(&g_editUnicodeDispatchActive, 0, 0) == 0 || message != WM_CHAR) return;
    const LONG trace = InterlockedIncrement(&g_gameEventQueueTraceCount);
    if (trace <= 1024 && g_api && g_api->Log)
        (void)0;
}

static int InstallEditboxDispatchDiagnostics(const V2ModAPI* api)
{
    if (!api || !api->PatchSlot || !api->gameBase) return 0;
    uintptr_t gateSlot = (uintptr_t)api->gameBase + kEditInputGateVtableRva + kEditInputGateSlot * sizeof(uintptr_t);
    uintptr_t queueSlot = (uintptr_t)api->gameBase + kGameEventQueueVtableRva + kGameEventEnqueueSlot * sizeof(uintptr_t);
    const uintptr_t expectedGate = (uintptr_t)api->gameBase + kEditInputGateRva;
    const uintptr_t expectedQueue = (uintptr_t)api->gameBase + kGameEventEnqueueRva;
    uintptr_t actualGate = 0, actualQueue = 0;
    __try { actualGate = *(const uintptr_t*)gateSlot; actualQueue = *(const uintptr_t*)queueSlot; }
    __except (EXCEPTION_EXECUTE_HANDLER) { actualGate = actualQueue = 0; }
    const int gateBytesMatch = actualGate == expectedGate;
    const int queueBytesMatch = actualQueue == expectedQueue;
    int gateHook = 0, queueHook = 0;
    if (gateBytesMatch)
        gateHook = api->PatchSlot(kEditInputGateVtableRva, kEditInputGateSlot,
            (void*)&HookEditInputGate, &g_editInputGateOriginal);
    if (queueBytesMatch)
        queueHook = api->PatchSlot(kGameEventQueueVtableRva, kGameEventEnqueueSlot,
            (void*)&HookGameEventEnqueue, &g_gameEventEnqueueOriginal);
    (void)0;
    return (gateHook ? 1 : 0) | (queueHook ? 2 : 0);
}
static void TraceGameInputQueue(const char* stage, void* gameState, LONG trace)
{
    if (!g_api || !g_api->Log || !gameState) return;
    uintptr_t node = 0;
    LONG count = -1;
    int readable = 1;
    __try {
        node = *(const uintptr_t*)((const unsigned char*)gameState + 0x1CF8);
        count = *(const volatile LONG*)((const unsigned char*)gameState + 0x1D00);
    } __except (EXCEPTION_EXECUTE_HANDLER) { readable = 0; }
    if (!readable) {
        (void)0;
        return;
    }
    uintptr_t current = node;
    unsigned visited = 0, events = 0, chars = 0;
    while (current && visited < 256) {
        uint32_t wParam = 0, message = 0, type = 0;
        uint32_t route18 = 0;
        uint8_t handled = 0xFF, tailFlag = 0xFF;
        uintptr_t next = 0;
        int ok = 1;
        __try {
            const unsigned char* eventNode = (const unsigned char*)current;
            wParam = *(const uint32_t*)(eventNode + 0x00);
            message = *(const uint32_t*)(eventNode + 0x04);
            type = *(const uint32_t*)(eventNode + 0x08);
            route18 = *(const uint32_t*)(eventNode + 0x18);
            handled = *(const uint8_t*)(eventNode + 0x18);
            next = *(const uintptr_t*)(eventNode + 0x20);
            tailFlag = *(const uint8_t*)(eventNode + 0x24);
        } __except (EXCEPTION_EXECUTE_HANDLER) { ok = 0; }
        if (!ok) break;
        if (message == WM_CHAR || message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP) {
            ++events;
            if (message == WM_CHAR) ++chars;
            const LONG record = InterlockedIncrement(&g_gameInputFlowRecordCount);
            if (record <= 2048)
                (void)0;
        }
        ++visited;
        current = next;
    }
    if (events)
        (void)0;
}

typedef void (__stdcall *GameInputPassFn)(void*);
static void __stdcall HookGameInputPass(void* gameState)
{
    GameInputPassFn original = (GameInputPassFn)g_gameInputPassOriginal;
    if (!original) return;
    const LONG trace = InterlockedIncrement(&g_gameInputPassTraceCount);
    TraceGameInputQueue("pre-006592f0", gameState, trace);
    original(gameState);
    TraceGameInputQueue("post-006592f0", gameState, trace);
}

typedef void (__stdcall *GameInputConsumerFn)(void*);
static void __stdcall HookGameInputConsumer(void* gameState)
{
    GameInputConsumerFn original = (GameInputConsumerFn)g_gameInputConsumerOriginal;
    if (!original) return;
    const LONG trace = InterlockedIncrement(&g_gameInputConsumerTraceCount);
    TraceGameInputQueue("pre-00657b60", gameState, trace);
    original(gameState);
    TraceGameInputQueue("post-00657b60", gameState, trace);
}

static int InstallGameInputFlowDiagnostics(const V2ModAPI* api)
{
    if (!api || !api->StealToTrampoline) return 0;
    const int passHook = api->StealToTrampoline(kGameInputPassRva, sizeof(kGameInputPassPrologue),
        g_gameInputPassTrampoline, sizeof(g_gameInputPassTrampoline), kGameInputPassPrologue,
        (void*)&HookGameInputPass, &g_gameInputPassOriginal, "event queue snapshot around adjacent FUN_006592f0");
    const int consumerHook = api->StealToTrampoline(kGameInputConsumerRva, sizeof(kGameInputPassPrologue),
        g_gameInputConsumerTrampoline, sizeof(g_gameInputConsumerTrampoline), kGameInputPassPrologue,
        (void*)&HookGameInputConsumer, &g_gameInputConsumerOriginal, "WM_CHAR queue before/after FUN_00657b60");
    (void)0;
    return (passHook ? 1 : 0) | (consumerHook ? 2 : 0);
}
static bool DispatchUnicodeToGame(HWND hwnd, uint32_t scalar, LPARAM lParam = 0, bool neutralizeAltGr = false, bool altGrShift = false)
{
    if (!g_editInputWndProcOriginal || scalar > 0x10FFFF || scalar < 0x20 ||
        (scalar >= 0xD800 && scalar <= 0xDFFF)) return false;
    BYTE priorKeyboardState[256] = {}, inputKeyboardState[256] = {};
    BYTE keyboardDuring[256] = {}, keyboardAfterCall[256] = {}, keyboardAfterRestore[256] = {};
    bool keyboardStateChanged = false;
    BOOL havePriorKeyboardState = GetKeyboardState(priorKeyboardState);
    BOOL haveKeyboardDuring = FALSE, haveKeyboardAfterCall = FALSE, haveKeyboardAfterRestore = FALSE;
    BOOL setKeyboardStateResult = TRUE, restoreKeyboardStateResult = TRUE;
    DWORD setKeyboardStateError = 0, restoreKeyboardStateError = 0;
    SHORT ctrlBefore = GetKeyState(VK_CONTROL), altBefore = GetKeyState(VK_MENU);
    if (neutralizeAltGr && havePriorKeyboardState) {
        memcpy(inputKeyboardState, priorKeyboardState, sizeof(inputKeyboardState));
        static const BYTE textModifiers[] = { VK_CONTROL, VK_LCONTROL, VK_RCONTROL, VK_MENU, VK_LMENU, VK_RMENU };
        for (size_t i = 0; i < sizeof(textModifiers) / sizeof(textModifiers[0]); ++i)
            inputKeyboardState[textModifiers[i]] &= 0x7F;
        if (memcmp(inputKeyboardState, priorKeyboardState, sizeof(inputKeyboardState)) != 0) {
            setKeyboardStateResult = SetKeyboardState(inputKeyboardState);
            if (!setKeyboardStateResult) setKeyboardStateError = GetLastError();
            keyboardStateChanged = setKeyboardStateResult != FALSE;
        }
    }
    haveKeyboardDuring = GetKeyboardState(keyboardDuring);
    SHORT ctrlDuring = GetKeyState(VK_CONTROL), altDuring = GetKeyState(VK_MENU);
    LONG previous = InterlockedExchange(&g_editUnicodeDispatchActive, 1);
    LONG previousTrace = InterlockedExchange(&g_editInputTraceActive, 1);
    uint32_t previousScalar = g_editUnicodePendingScalar;
    LONG previousAltGr = InterlockedExchange(&g_editUnicodeAltGrDispatchActive, neutralizeAltGr ? 1 : 0);
    LONG previousAltGrShift = InterlockedExchange(&g_editUnicodeAltGrShiftActive, (neutralizeAltGr && altGrShift) ? 1 : 0);
    g_editUnicodePendingScalar = scalar;
    LogEditRouteObjects("before", scalar, hwnd);
    LRESULT dispatchResult = HookEditInputWndProc(hwnd, WM_CHAR, (WPARAM)scalar, lParam);
    volatile LONG immediateRouteResult = (LONG)dispatchResult;
    if (g_api && g_api->Log)
        (void)0;
    LogEditRouteObjects("after", scalar, hwnd);
    haveKeyboardAfterCall = GetKeyboardState(keyboardAfterCall);
    SHORT ctrlDuringAfter = GetKeyState(VK_CONTROL), altDuringAfter = GetKeyState(VK_MENU);
    g_editUnicodePendingScalar = previousScalar;
    InterlockedExchange(&g_editUnicodeAltGrDispatchActive, previousAltGr);
    InterlockedExchange(&g_editUnicodeAltGrShiftActive, previousAltGrShift);
    InterlockedExchange(&g_editInputTraceActive, previousTrace);
    InterlockedExchange(&g_editUnicodeDispatchActive, previous);
    if (keyboardStateChanged) {
        restoreKeyboardStateResult = SetKeyboardState(priorKeyboardState);
        if (!restoreKeyboardStateResult) restoreKeyboardStateError = GetLastError();
    }
    haveKeyboardAfterRestore = GetKeyboardState(keyboardAfterRestore);
    SHORT ctrlAfter = GetKeyState(VK_CONTROL), altAfter = GetKeyState(VK_MENU);
    LONG routeCount = InterlockedIncrement(&g_editKeyboardRouteLogCount);
    if (routeCount <= 256 && g_api && g_api->Log)
        (void)0;
    if (routeCount <= 256 && g_api && g_api->Log)
        (void)0;
    return true;
}static bool DispatchScalarToGame(HWND hwnd, uint32_t scalar, LPARAM lParam = 0)
{
    if (scalar <= 0x7F)
        return DispatchLegacyByteToGame(hwnd, (uint8_t)scalar, lParam);
    return DispatchUnicodeToGame(hwnd, scalar, lParam);
}
static bool DispatchUtf16Unit(HWND hwnd, uint16_t unit)
{
    if (unit >= 0xD800 && unit <= 0xDBFF) {
        g_editPendingHighSurrogate = unit;
        return true;
    }
    if (unit >= 0xDC00 && unit <= 0xDFFF) {
        if (!g_editPendingHighSurrogate) return true;
        uint32_t scalar = 0x10000 + (((uint32_t)g_editPendingHighSurrogate - 0xD800) << 10) +
            ((uint32_t)unit - 0xDC00);
        g_editPendingHighSurrogate = 0;
        return DispatchScalarToGame(hwnd, scalar);
    }
    g_editPendingHighSurrogate = 0;
    return DispatchScalarToGame(hwnd, unit, 0);
}

static bool DispatchImeResult(HWND hwnd, LPARAM lParam)
{
    if (!(lParam & GCS_RESULTSTR)) return false;
    HMODULE imm = GetModuleHandleW(L"imm32.dll");
    typedef HIMC (WINAPI *GetContextFn)(HWND);
    typedef BOOL (WINAPI *ReleaseContextFn)(HWND, HIMC);
    typedef LONG (WINAPI *GetCompositionStringWFn)(HIMC, DWORD, LPVOID, DWORD);
    GetContextFn getContext = imm ? (GetContextFn)GetProcAddress(imm, "ImmGetContext") : nullptr;
    ReleaseContextFn releaseContext = imm ? (ReleaseContextFn)GetProcAddress(imm, "ImmReleaseContext") : nullptr;
    GetCompositionStringWFn getString = imm ? (GetCompositionStringWFn)GetProcAddress(imm, "ImmGetCompositionStringW") : nullptr;
    if (!getContext || !releaseContext || !getString) return false;
    HIMC context = getContext(hwnd);
    if (!context) return false;
    LONG bytes = getString(context, GCS_RESULTSTR, nullptr, 0);
    if (bytes <= 0 || bytes > 4096 || (bytes % (LONG)sizeof(wchar_t)) != 0) {
        releaseContext(hwnd, context);
        return false;
    }
    std::vector<wchar_t> result((size_t)bytes / sizeof(wchar_t));
    LONG copied = getString(context, GCS_RESULTSTR, result.data(), (DWORD)bytes);
    releaseContext(hwnd, context);
    if (copied != bytes) return false;
    for (size_t i = 0; i < result.size(); ++i) {
        uint16_t unit = (uint16_t)result[i];
        if (unit >= 0xD800 && unit <= 0xDBFF && i + 1 < result.size()) {
            uint16_t low = (uint16_t)result[i + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                DispatchUtf16Unit(hwnd, unit);
                DispatchUtf16Unit(hwnd, low);
                ++i;
                continue;
            }
        }
        DispatchUtf16Unit(hwnd, unit);
    }
    g_editPendingHighSurrogate = 0;
    if (g_api && g_api->Log && InterlockedCompareExchange(&g_editInputImeLogged, 1, 0) == 0)
        (void)0;
    return true;
}

static LRESULT CALLBACK HookUnicodeEditWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    const bool editboxConfirmKey = message == WM_KEYDOWN && wParam == VK_RETURN &&
        InterlockedCompareExchange(&g_editboxRenderTraceCount, 0, 0) > 0;
    if (editboxConfirmKey) {
        InterlockedExchange(&g_editboxRenderPhase, 1);
        if (g_api && g_api->Log)
            (void)0;
    }
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
        LONG keyCount = InterlockedIncrement(&g_editUnicodeKeyLogCount);
        if (keyCount <= 512 && g_api && g_api->Log) {
            BYTE keyboardState[256] = {};
            WCHAR mapped[8] = {};
            BOOL haveKeyboardState = GetKeyboardState(keyboardState);
            UINT scanCode = (UINT)((lParam >> 16) & 0xFF);
            HKL layout = GetKeyboardLayout(0);
            int mappedCount = haveKeyboardState
                ? ToUnicodeEx((UINT)wParam, scanCode, keyboardState, mapped, 8, 0x4, layout)
                : 0;
            if (mappedCount == 1 && mapped[0] > 0x7F && !(mapped[0] >= 0xD800 && mapped[0] <= 0xDFFF)) {
                const uint8_t ctrlDown = (keyboardState[VK_CONTROL] & 0x80) != 0;
                const uint8_t rightAltDown = (keyboardState[VK_RMENU] & 0x80) != 0;
                const uint8_t shiftDown = (keyboardState[VK_SHIFT] & 0x80) != 0;
                const uint8_t altGr = (uint8_t)(ctrlDown && rightAltDown);
                const uint8_t altGrShift = (uint8_t)(altGr && shiftDown);
                QueueEditKeyboardScalar((uint32_t)mapped[0], (uint8_t)scanCode,
                    (uint8_t)((lParam >> 24) & 1), altGr, shiftDown);
                (void)0;
            }
            (void)0;
        }
    }
    if (message == WM_CHAR || message == WM_DEADCHAR || message == WM_SYSCHAR ||
        message == WM_SYSDEADCHAR || message == WM_UNICHAR) {
        LONG charCount = InterlockedIncrement(&g_editUnicodeCharLogCount);
        if (charCount <= 512 && g_api && g_api->Log) {
            const char* kind = message == WM_CHAR ? "WM_CHAR" :
                message == WM_DEADCHAR ? "WM_DEADCHAR" :
                message == WM_SYSCHAR ? "WM_SYSCHAR" :
                message == WM_SYSDEADCHAR ? "WM_SYSDEADCHAR" : "WM_UNICHAR";
            if (message == WM_UNICHAR)
                (void)0;
            else
                (void)0;
        }
    }
    if (message == WM_CHAR) {
        const uint8_t scanCode = (uint8_t)((lParam >> 16) & 0xFF);
        const uint8_t extended = (uint8_t)((lParam >> 24) & 1);
        EditKeyboardScalarProbe keyScalar = {};
        if (TakeEditKeyboardScalar(scanCode, extended, &keyScalar)) {
            const uint16_t rawUnit = (uint16_t)wParam;
            const bool rawWasSubstituted = rawUnit != (uint16_t)keyScalar.scalar;
            if (rawWasSubstituted || keyScalar.altGr) {
                if (g_api && g_api->Log)
                    (void)0;
                if (DispatchUnicodeToGame(hwnd, keyScalar.scalar, lParam, keyScalar.altGr != 0, keyScalar.shift != 0)) return 0;
            } else if (DispatchUnicodeToGame(hwnd, keyScalar.scalar, lParam)) {
                return 0;
            }
        }
        uint16_t unit = (uint16_t)wParam;
        if (unit >= 0xD800 && unit <= 0xDBFF) { g_editPendingHighSurrogate = unit; return 0; }
        if (unit >= 0xDC00 && unit <= 0xDFFF) {
            if (g_editPendingHighSurrogate) {
                uint32_t scalar = 0x10000 + (((uint32_t)g_editPendingHighSurrogate - 0xD800) << 10) +
                    ((uint32_t)unit - 0xDC00);
                g_editPendingHighSurrogate = 0;
                if (DispatchUnicodeToGame(hwnd, scalar)) return 0;
            }
            return 0;
        }
        g_editPendingHighSurrogate = 0;
        if (unit > 0x7F && DispatchScalarToGame(hwnd, unit, lParam)) return 0;
    }
    if (message == WM_UNICHAR) {
        if (wParam == UNICODE_NOCHAR) return TRUE;
        if (DispatchScalarToGame(hwnd, (uint32_t)wParam, lParam)) return 0;
    }
    if (message == WM_IME_COMPOSITION && DispatchImeResult(hwnd, lParam)) return 0;
    LRESULT result = g_editUnicodePreviousProc
        ? CallWindowProcA(g_editUnicodePreviousProc, hwnd, message, wParam, lParam)
        : DefWindowProcW(hwnd, message, wParam, lParam);
    if (editboxConfirmKey) {
        InterlockedExchange(&g_editboxRenderPhase, 2);
        if (g_api && g_api->Log)
            (void)0;
    }
    return result;
}

static void TryInstallUnicodeEditSubclass(HWND hwnd, UINT message)
{
    if (!hwnd || g_editUnicodeSubclassedHwnd || message == WM_NCCREATE || message == WM_CREATE) return;
    // Wait until the main window has completed construction, then install once on its first active message.
    if (message != WM_SIZE && message != WM_SHOWWINDOW && message != WM_SETFOCUS &&
        message != WM_ACTIVATE && message != WM_PAINT) return;
    if (InterlockedCompareExchange(&g_editUnicodeSubclassAttempted, 1, 0) != 0) return;
    LONG_PTR previous = GetWindowLongPtrW(hwnd, GWLP_WNDPROC);
    if (!previous || previous == (LONG_PTR)&HookUnicodeEditWndProc) {
        InterlockedExchange(&g_editUnicodeSubclassAttempted, 0);
        return;
    }
    SetLastError(0);
    LONG_PTR replaced = SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)&HookUnicodeEditWndProc);
    DWORD error = GetLastError();
    if (!replaced && error) {
        InterlockedExchange(&g_editUnicodeSubclassAttempted, 0);
        if (g_api && g_api->Log)
            (void)0;
        return;
    }
    g_editUnicodePreviousProc = (WNDPROC)replaced;
    g_editUnicodeSubclassedHwnd = hwnd;
    if (g_api && g_api->Log)
        (void)0;
}

static LRESULT WINAPI HookEditInputWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    EditInputWndProcFn original = (EditInputWndProcFn)g_editInputWndProcOriginal;
    if (!original) return DefWindowProcA(hwnd, message, wParam, lParam);
    TryInstallUnicodeEditSubclass(hwnd, message);
    if (message == WM_UNICHAR) {
        if (wParam == UNICODE_NOCHAR) return TRUE;
        if (DispatchScalarToGame(hwnd, (uint32_t)wParam, lParam)) {
            if (g_api && g_api->Log && InterlockedCompareExchange(&g_editInputUnicodeLogged, 1, 0) == 0)
                (void)0;
            return 0;
        }
        return 0;
    }
    if (message == WM_IME_COMPOSITION && DispatchImeResult(hwnd, lParam)) return 0;
    const bool traceRoute = message == WM_CHAR && InterlockedCompareExchange(&g_editUnicodeDispatchActive, 0, 0) != 0;
    const uint32_t traceScalar = g_editUnicodePendingScalar;
    WPARAM gameWParam = wParam;
    if (traceRoute && traceScalar == kEditboxReservedCollisionScalar &&
        (uint32_t)wParam == kEditboxReservedCollisionScalar) {
        gameWParam = (WPARAM)kEditboxReservedCollisionProbeScalar;
        if (g_api && g_api->Log)
            (void)0;
    }
    if (traceRoute && g_api && g_api->Log)
        (void)0;
    LRESULT result = original(hwnd, message, gameWParam, lParam);
    if (traceRoute && g_api && g_api->Log)
        (void)0;
    return result;
}

typedef void (__thiscall *EditControlDispatchFn)(void*, uint32_t, uint32_t);
static volatile LONG g_editControlUnicodeLogCount = 0;
static volatile LONG g_editboxReservedFilterLogCount = 0;
static volatile LONG g_editboxReservedListLogged = 0;
typedef void (__thiscall *EditboxReservedFilterFn)(void*, uint32_t*, void*);
static void __fastcall HookEditboxReservedFilter(void* self, void*, uint32_t* event, void* context)
{
    EditboxReservedFilterFn original = (EditboxReservedFilterFn)g_editboxReservedFilterOriginal;
    if (!original) return;

    uint32_t scalar = 0, message = 0, eventType = 0;
    int eventReadable = 0;
    __try {
        if (event) {
            scalar = event[0];
            message = event[1];
            eventType = event[2];
            eventReadable = 1;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) { eventReadable = 0; }

    if (eventReadable && message == WM_CHAR && eventType == 1) {
        static const unsigned char fixedReserved[] = { 0x22, 0x40, 0x7B, 0x7D, 0xA3, 0xA4, 0xA7 };
        const unsigned char lowByte = (unsigned char)scalar;
        int fixedMatch = 0;
        for (size_t i = 0; i < sizeof(fixedReserved); ++i)
            if (fixedReserved[i] == lowByte) fixedMatch = 1;

        uintptr_t listHead = 0, current = 0;
        unsigned char dynamicBytes[256] = {};
        char dynamicText[768] = {};
        unsigned listCount = 0;
        int listReadable = 1, dynamicMatch = 0;
        __try {
            listHead = *(const uintptr_t*)((const unsigned char*)self + 0x194);
            current = listHead;
            while (current && listCount < sizeof(dynamicBytes)) {
                const unsigned char value = *(const unsigned char*)current;
                dynamicBytes[listCount++] = value;
                if (value == lowByte) dynamicMatch = 1;
                current = *(const uintptr_t*)(current + 0x08);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) { listReadable = 0; }

        if (listReadable && InterlockedCompareExchange(&g_editboxReservedListLogged, 1, 0) == 0 && g_api && g_api->Log) {
            size_t used = 0;
            for (unsigned i = 0; i < listCount && used + 4 < sizeof(dynamicText); ++i)
                used += (size_t)sprintf_s(dynamicText + used, sizeof(dynamicText) - used,
                    "%s%02X", i ? "," : "", (unsigned)dynamicBytes[i]);
            (void)0;
        }

        const LONG trace = InterlockedIncrement(&g_editboxReservedFilterLogCount);
        if (trace <= 512 && g_api && g_api->Log)
            (void)0;
    }

    original(self, event, context);
}

typedef void (__thiscall *EditControlSetTextFn)(void*, const char*);
typedef int (__thiscall *EditLineColumnToByteOffsetFn)(void*, int, int);
typedef void (__thiscall *EditUtf8ByteDeleteFn)(uint32_t, void*);

static bool DecodeUtf8ScalarAt(const unsigned char* data, uint32_t length, uint32_t start, uint32_t* width)
{
    if (!data || !width || start >= length) return false;
    const unsigned char lead = data[start];
    uint32_t count = 0;
    uint32_t scalar = 0;
    if (lead <= 0x7F) { *width = 1; return true; }
    if (lead >= 0xC2 && lead <= 0xDF) { count = 2; scalar = lead & 0x1F; }
    else if (lead >= 0xE0 && lead <= 0xEF) { count = 3; scalar = lead & 0x0F; }
    else if (lead >= 0xF0 && lead <= 0xF4) { count = 4; scalar = lead & 0x07; }
    else return false;
    if (start + count > length) return false;
    for (uint32_t i = 1; i < count; ++i) {
        const unsigned char next = data[start + i];
        if ((next & 0xC0) != 0x80) return false;
        scalar = (scalar << 6) | (next & 0x3F);
    }
    if ((count == 2 && scalar < 0x80) || (count == 3 && scalar < 0x800) ||
        (count == 4 && scalar < 0x10000) || scalar > 0x10FFFF ||
        (scalar >= 0xD800 && scalar <= 0xDFFF)) return false;
    *width = count;
    return true;
}

static bool FindUtf8ScalarForByteDelete(void* editor, uint32_t offset, uint32_t* startOut, uint32_t* widthOut)
{
    if (!editor || !startOut || !widthOut) return false;
    __try {
        const unsigned char* self = (const unsigned char*)editor;
        const uint32_t length = *(const uint32_t*)(self + 0x28);
        const uint32_t capacity = *(const uint32_t*)(self + 0x2C);
        const unsigned char* stringObject = self + 0x18;
        if (length == 0 || length > 0x10000 || offset >= length) return false;
        const unsigned char* bytes = capacity > 15 ? *(const unsigned char* const*)stringObject : stringObject;
        if (!bytes) return false;
        uint32_t start = offset;
        for (uint32_t back = 0; back < 3 && start > 0 && (bytes[start] & 0xC0) == 0x80; ++back) --start;
        uint32_t width = 0;
        if (!DecodeUtf8ScalarAt(bytes, length, start, &width) || offset >= start + width) return false;
        if (width <= 1) return false;
        *startOut = start;
        *widthOut = width;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static void __fastcall HookEditUtf8ByteDelete(uint32_t offset, void*, void* editor)
{
    EditUtf8ByteDeleteFn original = (EditUtf8ByteDeleteFn)g_editUtf8ByteDeleteOriginal;
    if (!original) return;
    const uintptr_t caller = (uintptr_t)_ReturnAddress();
    const uintptr_t base = g_api ? (uintptr_t)g_api->gameBase : 0;
    // The Ghidra xrefs name the CALL instruction addresses. _ReturnAddress()
    // observes the instruction immediately after each five-byte E8 call.
    const uintptr_t backspaceCallerA = base + 0x005F6A4C;
    const uintptr_t backspaceCallerB = base + 0x005F6A70;
    const uintptr_t deleteCaller = base + 0x005F6B61;
    const bool backspace = caller == backspaceCallerA || caller == backspaceCallerB;
    const bool isDelete = caller == deleteCaller;
    const bool armyEditor = editor == InterlockedCompareExchangePointer(&g_armyNameEditControl, nullptr, nullptr);
    if (armyEditor) {
        const LONG callerTrace = InterlockedIncrement(&g_editUtf8DeleteCallerTraceCount);
        if (callerTrace <= 96 && g_api && g_api->Log)
            (void)0;
    }
    const bool scoped = armyEditor &&
        (backspace || isDelete);
    uint32_t start = 0, width = 0;
    if (!scoped || !FindUtf8ScalarForByteDelete(editor, offset, &start, &width)) {
        original(offset, editor);
        return;
    }

    // Keep the engine's own string splice/allocator path; call it once per
    // byte inside this hook so the outer edit operation still notifies once.
    if (backspace) {
        for (uint32_t i = width; i > 0; --i) original(start + i - 1, editor);
        if (InterlockedCompareExchangePointer(&g_editUtf8DeleteDispatchSelf, nullptr, nullptr) == editor) {
            const LONG extra = (LONG)(offset - start);
            InterlockedExchange(&g_editUtf8DeleteExtraLeft, extra > 3 ? 3 : extra);
        }
    } else {
        for (uint32_t i = 0; i < width; ++i) original(start, editor);
    }
    const LONG trace = InterlockedIncrement(&g_editUtf8DeleteTraceCount);
    if (trace <= 64 && g_api && g_api->Log)
        (void)0;
}

static void __fastcall HookEditControlSetText(void* self, void*, const char* text)
{
    EditControlSetTextFn original = (EditControlSetTextFn)g_editControlSetTextOriginal;
    if (!original) return;
    void* caller = _ReturnAddress();
    const uintptr_t expectedCaller = g_api ? (uintptr_t)g_api->gameBase + kUnitnameSetterReturnRva : 0;
    const uintptr_t callerAddress = (uintptr_t)caller;
    const uintptr_t unitnameSetupStart = g_api ? (uintptr_t)g_api->gameBase + 0x00398A50 : 0;
    const uintptr_t unitnameSetupEnd = g_api ? (uintptr_t)g_api->gameBase + 0x0039A000 : 0;
    const bool fromUnitnameSetup = callerAddress >= unitnameSetupStart && callerAddress < unitnameSetupEnd;
    LONG trace = InterlockedIncrement(&g_editControlSetTextTraceCount);
    if (g_api && g_api->Log && (trace <= 64 || fromUnitnameSetup)) {
        uintptr_t vtable = 0;
        __try { vtable = *(const uintptr_t*)self; } __except (EXCEPTION_EXECUTE_HANDLER) { vtable = 0; }
        (void)0;
    }
    if (caller == (void*)expectedCaller) {
        InterlockedExchangePointer(&g_armyNameEditControl, self);
        if (g_api && g_api->Log)
            (void)0;
    }
    original(self, text);
}

static bool IsArmyEditTraceEvent(uint32_t character, uint32_t message)
{
    if (message == WM_CHAR && character == 8) return true;
    if (message == WM_KEYDOWN && (character == VK_LEFT || character == VK_RIGHT || character == VK_DELETE || character == VK_BACK)) return true;
    return false;
}

static void LogArmyEditboxState(void* self, const char* phase, uint32_t character, uint32_t message)
{
    if (!g_api || !g_api->Log) return;
    LONG trace = InterlockedIncrement(&g_armyEditTraceCount);
    if (trace > 160) return;

    uint32_t length = 0, packedPosition = 0;
    int byteOffset = -1;
    char bytesHex[96] = "unavailable";
    __try {
        const unsigned char* stringObject = (const unsigned char*)self + 0x18;
        length = *(const uint32_t*)((const unsigned char*)self + 0x28);
        packedPosition = *(const uint32_t*)((const unsigned char*)self + 0x38);
        int line = (int)(packedPosition >> 16);
        int column = (int)(packedPosition & 0xFFFF);
        EditLineColumnToByteOffsetFn toByteOffset = (EditLineColumnToByteOffsetFn)(g_api->gameBase + 0x005F7C00);
        byteOffset = toByteOffset(self, line, column);

        if (length <= 0x10000) {
            const uint32_t capacity = *(const uint32_t*)((const unsigned char*)self + 0x2C);
            const unsigned char* data = capacity > 15 ? *(const unsigned char* const*)stringObject : stringObject;
            if (data) {
                uint32_t begin = byteOffset > 4 ? (uint32_t)byteOffset - 4 : 0;
                if (begin > length) begin = length;
                uint32_t end = (uint32_t)byteOffset + 5;
                if (end > length) end = length;
                size_t used = 0;
                bytesHex[0] = 0;
                for (uint32_t i = begin; i < end && used + 4 < sizeof(bytesHex); ++i)
                    used += (size_t)sprintf_s(bytesHex + used, sizeof(bytesHex) - used, "%s%02X", i == begin ? "" : " ", (unsigned)data[i]);
                if (used == 0) strcpy_s(bytesHex, "<empty-range>");
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        strcpy_s(bytesHex, "read-fault");
    }
    (void)0;
}

static void FinishArmyUtf8Backspace(void* self)
{
    const LONG extra = InterlockedExchange(&g_editUtf8DeleteExtraLeft, 0);
    InterlockedExchangePointer(&g_editUtf8DeleteDispatchSelf, nullptr);
    if (extra <= 0 || extra > 3) return;
    __try {
        void** vtable = *(void***)self;
        typedef void (__thiscall *MoveLeftFn)(void*);
        MoveLeftFn moveLeft = (MoveLeftFn)vtable[0x6C / sizeof(void*)];
        for (LONG i = 0; i < extra; ++i) moveLeft(self);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        if (g_api && g_api->Log) (void)0;
    }
}

static void __fastcall HookEditControlDispatch(void* self, void*, uint32_t character, uint32_t message)
{
    EditControlDispatchFn original = (EditControlDispatchFn)g_editControlDispatchOriginal;
    if (!original) return;
    const bool traceArmyEdit = self == InterlockedCompareExchangePointer(&g_armyNameEditControl, nullptr, nullptr) &&
        IsArmyEditTraceEvent(character, message);
    const bool trackArmyBackspace = traceArmyEdit && ((message == WM_CHAR && character == VK_BACK) ||
        (message == WM_KEYDOWN && character == VK_BACK));
    if (trackArmyBackspace) {
        InterlockedExchange(&g_editUtf8DeleteExtraLeft, 0);
        InterlockedExchangePointer(&g_editUtf8DeleteDispatchSelf, self);
    }
    if (traceArmyEdit) LogArmyEditboxState(self, "before", character, message);
    if (message == WM_CHAR && character == kEditboxReservedCollisionProbeScalar) {
        if (g_api && g_api->Log)
            (void)0;
        character = kEditboxReservedCollisionScalar;
    }
    if (message == WM_CHAR) {
        LONG allEventCount = InterlockedIncrement(&g_editEditorEventLogCount);
        if (allEventCount <= 512 && g_api && g_api->Log)
            { uintptr_t editorVtable = 0; void* returnAddress = _ReturnAddress(); __try { editorVtable = *(const uintptr_t*)self; } __except (EXCEPTION_EXECUTE_HANDLER) { editorVtable = 0; } (void)0; }
    }
    if (message == WM_CHAR && InterlockedCompareExchange(&g_editUnicodeDispatchActive, 0, 0) != 0) {
        LONG eventCount = InterlockedIncrement(&g_editEditorEventLogCount);
        if (eventCount <= 256 && g_api && g_api->Log)
            (void)0;
    }    if (message == WM_CHAR && character > 0x7F && character <= 0x10FFFF &&
        !(character >= 0xD800 && character <= 0xDFFF)) {
        LONG previousActive = InterlockedExchange(&g_editUnicodeDispatchActive, 1);
        LONG previousTrace = InterlockedExchange(&g_editInputTraceActive, 1);
        uint32_t previousScalar = g_editUnicodePendingScalar;
        g_editUnicodePendingScalar = character;
        LONG count = InterlockedIncrement(&g_editControlUnicodeLogCount);
        if (g_api && g_api->Log && count <= 96)
            (void)0;
        original(self, character, message);
        if (trackArmyBackspace) FinishArmyUtf8Backspace(self);
        if (traceArmyEdit) LogArmyEditboxState(self, "after", character, message);
        g_editUnicodePendingScalar = previousScalar;
        InterlockedExchange(&g_editInputTraceActive, previousTrace);
        InterlockedExchange(&g_editUnicodeDispatchActive, previousActive);
        return;
    }
    original(self, character, message);
    if (trackArmyBackspace) FinishArmyUtf8Backspace(self);
    if (traceArmyEdit) LogArmyEditboxState(self, "after", character, message);
}
typedef void (__thiscall *EditInsertTextFn)(void*, const char*);
static void __fastcall HookEditInsertText(void* self, void*, const char* text)
{
    if (text && InterlockedCompareExchange(&g_editInputTraceActive, 0, 0) != 0) {
        LONG count = InterlockedIncrement(&g_editInsertTraceCount);
        uint32_t length = *(const uint32_t*)(text + 0x10);
        const unsigned char* bytes = length > 15 ? *(const unsigned char* const*)text : (const unsigned char*)text;
        char hex[48] = {};
        size_t used = 0;
        uint32_t shown = length < 12 ? length : 12;
        for (uint32_t i = 0; i < shown && used + 4 < sizeof(hex); ++i)
            used += (size_t)sprintf_s(hex + used, sizeof(hex) - used, "%s%02X", i ? " " : "", bytes[i]);
        if (g_api && g_api->Log && count <= 128)
            (void)0;
    }
    if (g_editInsertTextOriginal)
        ((EditInsertTextFn)g_editInsertTextOriginal)(self, text);
}
static char* __stdcall HookEditInputString(char* out, uint32_t value)
{
    bool fromUnicodeMessage = InterlockedCompareExchange(&g_editUnicodeDispatchActive, 0, 0) != 0;
    if (out && InterlockedCompareExchange(&g_editInputTraceActive, 0, 0) != 0) {
        LONG traceCount = InterlockedIncrement(&g_editStringCtorTraceCount);
        if (g_api && g_api->Log && traceCount <= 128)
            (void)0;
    }
    uint32_t unicodeScalar = g_editUnicodePendingScalar;
    if (out && fromUnicodeMessage && unicodeScalar > 0x7F && unicodeScalar <= 0x10FFFF &&
        !(unicodeScalar >= 0xD800 && unicodeScalar <= 0xDFFF)) {
        char encoded[4];
        size_t length = 0;
        if (unicodeScalar <= 0x7FF) {
            encoded[length++] = (char)(0xC0 | (unicodeScalar >> 6));
            encoded[length++] = (char)(0x80 | (unicodeScalar & 0x3F));
        } else if (unicodeScalar <= 0xFFFF) {
            encoded[length++] = (char)(0xE0 | (unicodeScalar >> 12));
            encoded[length++] = (char)(0x80 | ((unicodeScalar >> 6) & 0x3F));
            encoded[length++] = (char)(0x80 | (unicodeScalar & 0x3F));
        } else {
            encoded[length++] = (char)(0xF0 | (unicodeScalar >> 18));
            encoded[length++] = (char)(0x80 | ((unicodeScalar >> 12) & 0x3F));
            encoded[length++] = (char)(0x80 | ((unicodeScalar >> 6) & 0x3F));
            encoded[length++] = (char)(0x80 | (unicodeScalar & 0x3F));
        }
        memset(out, 0, 16);
        memcpy(out, encoded, length);
        *(uint32_t*)(out + 0x10) = (uint32_t)length;
        *(uint32_t*)(out + 0x14) = 15;
        if (g_api && g_api->Log && InterlockedCompareExchange(&g_editInputEncodeLogged, 1, 0) == 0)
            (void)0;
        return out;
    }
    return ((EditInputStringFn)g_editInputStringOriginal)(out, value);
}
static bool IsTtfUnicodeSupportEnabled(const char* modFolder)
{
    // Keep existing Submod installs enabled unless the mod-local Core settings explicitly disable this feature.
    if (!modFolder || !*modFolder) return true;
    char path[MAX_PATH] = {};
    if (_snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\creatinecore_settings.ini", modFolder) < 0) return true;
    FILE* file = nullptr;
    if (fopen_s(&file, path, "rb") != 0 || !file) return true;
    char line[512] = {};
    bool enabled = true;
    while (fgets(line, sizeof(line), file)) {
        if ((unsigned char)line[0] == 0xEF && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
            memmove(line, line + 3, strlen(line + 3) + 1);
        char* comment = strpbrk(line, "#;");
        if (comment) *comment = 0;
        TrimSetting(line);
        if (!*line) continue;
        char* equals = strchr(line, '=');
        if (!equals) continue;
        *equals++ = 0;
        TrimSetting(line);
        TrimSetting(equals);
        if (_stricmp(line, "TTF_UNICODE_SUPPORT") != 0) continue;
        enabled = (strcmp(equals, "1") == 0 || _stricmp(equals, "yes") == 0 || _stricmp(equals, "true") == 0);
        break;
    }
    fclose(file);
    return enabled;
}
extern "C" __declspec(dllexport) int __cdecl V2Mod_Init(const V2ModAPI* api)
{
    if (!api || api->size < sizeof(V2ModAPI) || api->apiVersion != V2MOD_API_VERSION || !api->Log)
        return 0;
    if (!IsTtfUnicodeSupportEnabled(api->modFolder)) return 1;
    g_api = api;
    LoadUnicodeFontMap(api->modFolder);
    LoadMainMapFontSettings(api->modFolder, g_mainMapFontSettings);
    LoadProvinceNameMapFontSettings(api->modFolder, g_provinceNameMapFontSettings);
    LoadGuiMapFontSettings(api->modFolder, g_guiMapFontSettings);
    (void)0;
    if (!LoadPrivateTTFFonts(api->modFolder)) {
        (void)0;
        return 0;
    }
    const bool guiFontPayloadReady = BuildGuiMapFontPayload(api->modFolder);
    const bool guiFontGfxReady = guiFontPayloadReady && WriteGuiMapFontGfx(api->modFolder);
    (void)0;
    if (!api->StealToTrampoline || !api->StealToTrampoline(kFontCreateRva,
        sizeof(kFontCreatePrologue), g_fontCreateTrampoline, sizeof(g_fontCreateTrampoline),
        kFontCreatePrologue, (void*)&HookFontCreate, &g_fontCreateOriginal,
        "TTF UTF-8 font creation")) {
        (void)0;
        return 0;
    }
    const int measureHeightHook = api->StealToTrampoline(kFontMeasureHeightRva,
        sizeof(kFontMeasureHeightPrologue), g_fontMeasureHeightTrampoline, sizeof(g_fontMeasureHeightTrampoline),
        kFontMeasureHeightPrologue, (void*)&HookFontMeasureHeight, &g_fontMeasureHeightOriginal,
        "UTF-8 height-measure trace");
    const unsigned char lexerPrologue[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x08 };
    const unsigned char parserCopyPrologue[7] = { 0x55, 0x8B, 0xEC, 0x53, 0x8B, 0x5D, 0x08 };
    const unsigned char textBoxPrologue[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF8 };
    const unsigned char stringValuePrologue[5] = { 0x55, 0x8B, 0xEC, 0x6A, 0xFF };
    const unsigned char internationalTextParserPrologue[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0 };
    const unsigned char localizedTextExpandPrologue[9] = { 0x55, 0x8B, 0xEC, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00 };
    // The first nine bytes are two whole instructions (push ebp; mov ebp,esp;
    // mov eax,fs:[0]). Do not split the six-byte FS load in the trampoline.
    int lexerHook = 0;
    int parserHook = 0;
    int textBoxHook = 0;
    int stringValueHook = 0;
    int internationalTextParserHook = 0;
    int localizedTextExpandHook = 0;
    if (api->StealToTrampoline) {
        // Preserve the game's exact stack-cleanup conventions at each hook site.
        lexerHook = api->StealToTrampoline(kLexerNextRva, 6, g_lexerNextTrampoline, sizeof(g_lexerNextTrampoline),
            lexerPrologue, (void*)&HookLexerNext, &g_lexerNextOriginal, "UTF-8 lexer trace");
        parserHook = api->StealToTrampoline(kParserTokenCopyRva, 7, g_parserTokenCopyTrampoline, sizeof(g_parserTokenCopyTrampoline),
            parserCopyPrologue, (void*)&HookParserTokenCopy, &g_parserTokenCopyOriginal, "UTF-8 parser trace");
        textBoxHook = api->StealToTrampoline(kTextBoxPropertyRva, 6, g_textBoxPropertyTrampoline, sizeof(g_textBoxPropertyTrampoline),
            textBoxPrologue, (void*)&HookTextBoxProperty, &g_textBoxPropertyOriginal, "UTF-8 TextBoxType trace");
        stringValueHook = api->StealToTrampoline(kStringValueRva, 5, g_stringValueTrampoline, sizeof(g_stringValueTrampoline),
            stringValuePrologue, (void*)&HookStringValue, &g_stringValueOriginal, "UTF-8 string assignment trace");
        internationalTextParserHook = api->StealToTrampoline(kInternationalTextParserRva, 6,
            g_internationalTextParserTrampoline, sizeof(g_internationalTextParserTrampoline),
            internationalTextParserPrologue, (void*)&HookInternationalTextParser,
            &g_internationalTextParserOriginal, "UTF-8 localized-text markup parser");
        localizedTextExpandHook = api->StealToTrampoline(kLocalizedTextExpandRva, 9,
            g_localizedTextExpandTrampoline, sizeof(g_localizedTextExpandTrampoline),
            localizedTextExpandPrologue, (void*)&HookLocalizedTextExpand,
            &g_localizedTextExpandOriginal, "UTF-8 localization expansion");
    }
    (void)0;
    const int editboxDispatchDiagnostics=0;
    const int gameInputFlowDiagnostics=0;
    const int editInputWndProcHook = api->StealToTrampoline(kEditInputWndProcRva,
        sizeof(kEditInputWndProcPrologue), g_editInputWndProcTrampoline, sizeof(g_editInputWndProcTrampoline),
        kEditInputWndProcPrologue, (void*)&HookEditInputWndProc, &g_editInputWndProcOriginal,
        "Unicode edit-box window input");
    (void)0;
    const int editControlDispatchHook = api->StealToTrampoline(kEditControlDispatchRva,
        sizeof(kEditControlDispatchPrologue), g_editControlDispatchTrampoline, sizeof(g_editControlDispatchTrampoline),
        kEditControlDispatchPrologue, (void*)&HookEditControlDispatch, &g_editControlDispatchOriginal,
        "UTF-8 edit-box event before byte narrowing");
    const int editControlSetTextHook = api->StealToTrampoline(kEditControlSetTextRva,
        sizeof(kEditControlSetTextPrologue), g_editControlSetTextTrampoline, sizeof(g_editControlSetTextTrampoline),
        kEditControlSetTextPrologue, (void*)&HookEditControlSetText, &g_editControlSetTextOriginal,
        "army unitname editbox identity capture");
    const int editUtf8ByteDeleteHook = api->StealToTrampoline(kEditUtf8ByteDeleteRva,
        sizeof(kEditUtf8ByteDeletePrologue), g_editUtf8ByteDeleteTrampoline, sizeof(g_editUtf8ByteDeleteTrampoline),
        kEditUtf8ByteDeletePrologue, (void*)&HookEditUtf8ByteDelete, &g_editUtf8ByteDeleteOriginal,
        "UTF-8 scalar deletion in army unitname editbox");
    (void)0;
    (void)0;
    const int editInputStringHook = api->StealToTrampoline(kEditInputStringRva,
        sizeof(kEditInputStringPrologue), g_editInputStringTrampoline, sizeof(g_editInputStringTrampoline),
        kEditInputStringPrologue, (void*)&HookEditInputString, &g_editInputStringOriginal,
        "UTF-8 edit-box character string");
    (void)0;
    (void)0;
    (void)0;
    (void)0;
    const bool countryTtfHooks = map_install_country_hooks(api);
    (void)0;
    const bool provinceTtfHooks = map_install_province_ttf_hooks(api);
    api->Log("[ttf] country_font: %s", countryTtfHooks ? "ok" : "error");
    api->Log("[ttf] province_font: %s", provinceTtfHooks ? "ok" : "error");
    api->Log("[ttf] gui_font: %s", (guiFontPayloadReady && guiFontGfxReady) ? "ok" : "error");
    return 1;
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) DisableThreadLibraryCalls(module);
    return TRUE;
}


#include "map_ttf_atlas.h"
#include <limits>
#include <algorithm>
#include <cmath>

static uint16_t TtfReadU16(const std::vector<uint8_t>& b, size_t p) {
    return (uint16_t(b[p]) << 8) | uint16_t(b[p+1]);
}
static uint32_t TtfReadU32(const std::vector<uint8_t>& b, size_t p) {
    return (uint32_t(b[p]) << 24) | (uint32_t(b[p+1]) << 16) | (uint32_t(b[p+2]) << 8) | uint32_t(b[p+3]);
}
static bool TtfFamilyName(const std::vector<uint8_t>& b, std::wstring& face) {
    if (b.size() < 12 || memcmp(b.data(), "ttcf", 4) == 0) return false;
    const uint16_t tables=TtfReadU16(b,4); size_t nameOff=0, nameLen=0;
    if (tables > 256 || 12ull + 16ull*tables > b.size()) return false;
    for (uint16_t i=0;i<tables;++i) {
        const size_t r=12+16ull*i;
        if (memcmp(b.data()+r,"name",4)==0) { nameOff=TtfReadU32(b,r+8); nameLen=TtfReadU32(b,r+12); break; }
    }
    if (!nameOff || nameLen<6 || nameOff+nameLen>b.size()) return false;
    const uint16_t count=TtfReadU16(b,nameOff+2), strings=TtfReadU16(b,nameOff+4);
    if (count>4096 || 6ull+12ull*count>nameLen || strings>nameLen) return false;
    int best=-1; size_t bestOff=0; uint16_t bestLen=0;
    for(uint16_t i=0;i<count;++i) {
        const size_t r=nameOff+6+12ull*i;
        const uint16_t platform=TtfReadU16(b,r), language=TtfReadU16(b,r+4), id=TtfReadU16(b,r+6);
        const uint16_t len=TtfReadU16(b,r+8), off=TtfReadU16(b,r+10);
        if ((id!=16 && id!=1) || (platform!=0 && platform!=3) || !len || (len&1)) continue;
        const size_t pos=nameOff+strings+off;
        if (pos+len>nameOff+nameLen) continue;
        int score=(id==16?100:50)+(platform==3?20:0)+(language==0x0409?5:0);
        if(score>best){best=score;bestOff=pos;bestLen=len;}
    }
    if(best<0 || bestLen/2>=LF_FACESIZE) return false;
    face.clear(); face.reserve(bestLen/2);
    for(size_t i=0;i<bestLen;i+=2) face.push_back(wchar_t(TtfReadU16(b,bestOff+i)));
    return !face.empty();
}

static bool TtfHasMappedGlyph(const std::vector<uint8_t>& b, uint32_t codepoint) {
    auto has = [&](size_t p, size_t n) { return p <= b.size() && n <= b.size() - p; };
    if (codepoint > 0x10FFFF || !has(0, 12)) return false;
    const uint16_t tableCount = TtfReadU16(b, 4);
    if (tableCount > 256 || !has(12, size_t(tableCount) * 16)) return false;
    size_t cmapOffset = 0, cmapLength = 0;
    for (uint16_t i = 0; i < tableCount; ++i) {
        const size_t record = 12 + size_t(i) * 16;
        if (memcmp(b.data() + record, "cmap", 4) == 0) {
            cmapOffset = TtfReadU32(b, record + 8);
            cmapLength = TtfReadU32(b, record + 12);
            break;
        }
    }
    if (!cmapOffset || cmapLength < 4 || !has(cmapOffset, cmapLength)) return false;
    const uint16_t encodingCount = TtfReadU16(b, cmapOffset + 2);
    if (encodingCount > 256 || 4ull + 8ull * encodingCount > cmapLength) return false;
    for (uint16_t i = 0; i < encodingCount; ++i) {
        const size_t record = cmapOffset + 4 + size_t(i) * 8;
        const uint16_t platform = TtfReadU16(b, record);
        const uint16_t encoding = TtfReadU16(b, record + 2);
        if (platform != 0 && !(platform == 3 && (encoding == 1 || encoding == 10))) continue;
        const size_t sub = cmapOffset + TtfReadU32(b, record + 4);
        if (!has(sub, 2) || sub < cmapOffset || sub - cmapOffset >= cmapLength) continue;
        const uint16_t format = TtfReadU16(b, sub);
        if (format == 4 && codepoint <= 0xFFFF) {
            if (!has(sub, 14)) continue;
            const size_t length = TtfReadU16(b, sub + 2);
            const uint16_t segCountX2 = TtfReadU16(b, sub + 6);
            if ((segCountX2 & 1) || !segCountX2 || length < 16 || !has(sub, length) ||
                length > cmapLength - (sub - cmapOffset)) continue;
            const size_t segCount = segCountX2 / 2;
            const size_t endBase = sub + 14;
            const size_t startBase = endBase + 2 * segCount + 2;
            const size_t deltaBase = startBase + 2 * segCount;
            const size_t rangeBase = deltaBase + 2 * segCount;
            if (rangeBase + 2 * segCount > sub + length) continue;
            for (size_t s = 0; s < segCount; ++s) {
                const uint16_t end = TtfReadU16(b, endBase + 2 * s);
                if (codepoint > end) continue;
                const uint16_t start = TtfReadU16(b, startBase + 2 * s);
                if (codepoint < start) break;
                const int16_t delta = int16_t(TtfReadU16(b, deltaBase + 2 * s));
                const uint16_t range = TtfReadU16(b, rangeBase + 2 * s);
                uint16_t glyph = 0;
                if (!range) glyph = uint16_t(codepoint + delta);
                else {
                    const size_t glyphAddress = rangeBase + 2 * s + range + 2 * (size_t(codepoint) - start);
                    if (!has(glyphAddress, 2) || glyphAddress + 2 > sub + length) break;
                    glyph = TtfReadU16(b, glyphAddress);
                    if (glyph) glyph = uint16_t(glyph + delta);
                }
                if (glyph) return true;
                break;
            }
        } else if (format == 12 || format == 13) {
            if (!has(sub, 16)) continue;
            const size_t length = TtfReadU32(b, sub + 4);
            const uint32_t groups = TtfReadU32(b, sub + 12);
            if (length < 16 || !has(sub, length) || length > cmapLength - (sub - cmapOffset) ||
                groups > (length - 16) / 12) continue;
            size_t lo = 0, hi = groups;
            while (lo < hi) {
                const size_t mid = lo + (hi - lo) / 2;
                const size_t group = sub + 16 + mid * 12;
                const uint32_t first = TtfReadU32(b, group), last = TtfReadU32(b, group + 4);
                if (codepoint < first) hi = mid;
                else if (codepoint > last) lo = mid + 1;
                else {
                    const uint32_t firstGlyph = TtfReadU32(b, group + 8);
                    const uint64_t glyph = format == 12 ? uint64_t(firstGlyph) + codepoint - first : firstGlyph;
                    if (glyph) return true;
                    break;
                }
            }
        }
    }
    return false;
}
static bool LoadTtfFile(const wchar_t* path,std::vector<uint8_t>& bytes) {
    if(!path||!*path)return false;
    HANDLE input=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(input==INVALID_HANDLE_VALUE)return false;
    LARGE_INTEGER length={};bool ok=GetFileSizeEx(input,&length)&&length.QuadPart>0&&length.QuadPart<=32*1024*1024;
    bytes.assign(ok?size_t(length.QuadPart):0,0);DWORD got=0;
    ok=ok&&ReadFile(input,bytes.data(),DWORD(bytes.size()),&got,nullptr)&&got==bytes.size();CloseHandle(input);
    if(!ok)bytes.clear();return ok;
}

static bool BuildGuiMapFontPayload(const char* modFolder) {
    if (!modFolder || !*modFolder) return false;
    g_guiFontPayload.clear();
    g_guiFontQualityPolicies.clear();
    g_unicodeFontCount = 0;
    wchar_t wideModFolder[MAX_PATH] = {};
    if (!MultiByteToWideChar(CP_ACP, 0, modFolder, -1, wideModFolder, MAX_PATH)) return false;

    for (const GuiMapFontSettings& setting : g_guiMapFontSettings) {
        if (!setting.name[0] || !setting.userTtfFile[0] || setting.height < 4 || setting.height > 256) continue;
        wchar_t fontPath[MAX_PATH] = {};
        if (_snwprintf_s(fontPath, _countof(fontPath), _TRUNCATE,
            L"%s\\TTF Unicode Support\\%s", wideModFolder, setting.userTtfFile) < 0) continue;
        std::vector<uint8_t> fontBytes;
        std::wstring family;
        if (!LoadTtfFile(fontPath, fontBytes) || !TtfFamilyName(fontBytes, family) || family.empty()) continue;
        BOOL usedDefault = FALSE;
        char familyAnsi[LF_FACESIZE * 2] = {};
        if (!WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, family.c_str(), -1,
            familyAnsi, (int)sizeof(familyAnsi), nullptr, &usedDefault) || usedDefault) continue;
        if (strchr(familyAnsi, '"') || strchr(familyAnsi, '\\') || strchr(familyAnsi, '\r') || strchr(familyAnsi, '\n')) continue;

        char block[512] = {};
        const int written = _snprintf_s(block, sizeof(block), _TRUNCATE,
            "\tfont = {\r\n\t\tname = \"%s\"\r\n\t\tfontname = \"%s\"\r\n\t\theight = %d\r\n\t\tcharset = \"ANSI\"\r\n\t\tcolor = 0x%08X\r\n\t}\r\n",
            setting.name, familyAnsi, setting.height,
            static_cast<unsigned>(0xFF000000u | (unsigned(setting.colorRed) << 16) | (unsigned(setting.colorGreen) << 8) | unsigned(setting.colorBlue)));
        if (written < 0) continue;
        GuiFontQualityPolicy policy{};
        strcpy_s(policy.family, familyAnsi);
        policy.height = setting.height;
        policy.quality = setting.antiAliasing ? CLEARTYPE_QUALITY : ANTIALIASED_QUALITY;
        policy.color = 0xFF000000u | (DWORD(setting.colorRed) << 16) | (DWORD(setting.colorGreen) << 8) | DWORD(setting.colorBlue);
        bool merged = false;
        for (auto& existing : g_guiFontQualityPolicies) {
            if (existing.height == policy.height && _stricmp(existing.family, policy.family) == 0) {
                if (existing.quality != policy.quality) existing.quality = 0xFF;
                if (existing.color != policy.color) existing.colorAmbiguous = true;
                merged = true;
                break;
            }
        }
        if (!merged) g_guiFontQualityPolicies.push_back(policy);
        if (g_guiFontPayload.empty())
            g_guiFontPayload = "\r\n# ttf_unicode_support: generated GUI TTF font resources\r\nfonts = {\r\n";
        g_guiFontPayload += block;
        if (g_unicodeFontCount < _countof(g_unicodeFonts)) {
            strcpy_s(g_unicodeFonts[g_unicodeFontCount].logical, setting.name);
            strcpy_s(g_unicodeFonts[g_unicodeFontCount].face, familyAnsi);
            ++g_unicodeFontCount;
        }
    }
    if (!g_guiFontPayload.empty()) g_guiFontPayload += "}\r\n";
    return true;
}

static bool WriteGuiMapFontGfx(const char* modFolder) {
    if (!modFolder || !*modFolder) return false;

    wchar_t wideModFolder[MAX_PATH] = {};
    if (!MultiByteToWideChar(CP_ACP, 0, modFolder, -1, wideModFolder, MAX_PATH)) return false;

    wchar_t interfaceDir[MAX_PATH] = {};
    wchar_t outputPath[MAX_PATH] = {};
    wchar_t tempPath[MAX_PATH] = {};
    if (_snwprintf_s(interfaceDir, _countof(interfaceDir), _TRUNCATE, L"%s\\interface", wideModFolder) < 0 ||
        _snwprintf_s(outputPath, _countof(outputPath), _TRUNCATE, L"%s\\ttf_unicode_support.gfx", interfaceDir) < 0 ||
        _snwprintf_s(tempPath, _countof(tempPath), _TRUNCATE, L"%s\\ttf_unicode_support.gfx.tmp", interfaceDir) < 0)
        return false;

    if (!CreateDirectoryW(interfaceDir, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    const DWORD interfaceAttributes = GetFileAttributesW(interfaceDir);
    if (interfaceAttributes == INVALID_FILE_ATTRIBUTES || !(interfaceAttributes & FILE_ATTRIBUTE_DIRECTORY)) return false;

    const std::string generatedMarker = "# ttf_unicode_support: generated GUI TTF font resources";
    std::string desired = g_guiFontPayload;
    if (desired.empty()) desired = "\r\n" + generatedMarker + "\r\nfonts = {\r\n}\r\n";

    HANDLE existing = CreateFileW(outputPath, GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (existing != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size = {};
        bool canCompare = GetFileSizeEx(existing, &size) && size.QuadPart >= 0 && size.QuadPart <= 1024 * 1024;
        std::string current;
        if (canCompare) {
            current.resize((size_t)size.QuadPart);
            DWORD read = 0;
            canCompare = current.empty() ||
                (ReadFile(existing, current.data(), (DWORD)current.size(), &read, nullptr) && read == current.size());
        }
        CloseHandle(existing);
        if (!canCompare) return false;
        if (current == desired) return true;
        if (current.find(generatedMarker) == std::string::npos) return false;
    } else if (GetLastError() != ERROR_FILE_NOT_FOUND && GetLastError() != ERROR_PATH_NOT_FOUND) {
        return false;
    }

    HANDLE output = CreateFileW(tempPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool writeOk = WriteFile(output, desired.data(), (DWORD)desired.size(), &written, nullptr) &&
        written == desired.size() && FlushFileBuffers(output);
    CloseHandle(output);
    if (!writeOk) { DeleteFileW(tempPath); return false; }
    if (!MoveFileExW(tempPath, outputPath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tempPath);
        return false;
    }
    return true;
}
static bool LoadTtfFaceToDc(const wchar_t* path,int size,HDC& dc,HFONT& font,HGDIOBJ& old,HANDLE& resource,std::wstring& family) {
    std::vector<uint8_t> bytes;if(!LoadTtfFile(path,bytes)||!TtfFamilyName(bytes,family))return false;
    DWORD count=0;resource=AddFontMemResourceEx(bytes.data(),DWORD(bytes.size()),nullptr,&count);
    if(!resource||!count)return false;
    dc=CreateCompatibleDC(nullptr);if(!dc)return false;
    font=CreateFontW(-size,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_TT_ONLY_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,family.c_str());
    if(!font)return false;
    old=SelectObject(dc,font);if(!old||old==HGDI_ERROR){old=nullptr;return false;}
    wchar_t selected[LF_FACESIZE]={};
    return GetTextFaceW(dc,LF_FACESIZE,selected)&&_wcsicmp(selected,family.c_str())==0;
}

MapTtfAtlas::~MapTtfAtlas() {
    if(reserve_dc_&&reserve_old_)SelectObject(reserve_dc_,reserve_old_);
    if(reserve_font_)DeleteObject(reserve_font_);
    if(reserve_dc_)DeleteDC(reserve_dc_);
    if(reserve_font_resource_)RemoveFontMemResourceEx(reserve_font_resource_);
    if(dc_&&old_)SelectObject(dc_,old_);
    if(font_)DeleteObject(font_);
    if(dc_)DeleteDC(dc_);
    if(font_resource_)RemoveFontMemResourceEx(font_resource_);
}

bool MapTtfAtlas::initialize(const wchar_t* file,const wchar_t* reserve_file,int size,unsigned w,unsigned h) {
    if(dc_||!file||size<=0||w<4||h<4||w>4096||h>4096)return false;
    std::wstring face;
    if(!LoadTtfFaceToDc(file,size,dc_,font_,old_,font_resource_,face))return false;
    if(reserve_file&&*reserve_file){
        std::wstring reserveFace;
        if(!LoadTtfFaceToDc(reserve_file,size,reserve_dc_,reserve_font_,reserve_old_,reserve_font_resource_,reserveFace))return false;
    }
    DWORD pairs_count=GetKerningPairsW(dc_,0,nullptr);
    if(pairs_count){std::vector<KERNINGPAIR> pairs(pairs_count);pairs_count=GetKerningPairsW(dc_,pairs_count,pairs.data());for(DWORD i=0;i<pairs_count;++i)kerning_[(uint32_t(pairs[i].wFirst)<<16)|pairs[i].wSecond]=pairs[i].iKernAmount;}
    width_=w;height_=h;pixels_.assign(size_t(w)*h,0);return true;
}

bool MapTtfAtlas::glyph(wchar_t ch, MapGlyph& out) {
    const auto it=glyphs_.find(ch); if(it!=glyphs_.end()){out=it->second;return true;}
    if(!dc_||(ch>=0xD800&&ch<=0xDFFF))return false;
    if(ch==L' '){SIZE size={};if(!GetTextExtentPoint32W(dc_,&ch,1,&size))return false;out={0,0,0,0,0,0,size.cx};glyphs_.emplace(ch,out);return true;}
    HDC glyphDc=dc_;WORD index=0xffff;
    if(GetGlyphIndicesW(glyphDc,&ch,1,&index,GGI_MARK_NONEXISTING_GLYPHS)==GDI_ERROR||index==0xffff){
        glyphDc=reserve_dc_;index=0xffff;
        if(!glyphDc||GetGlyphIndicesW(glyphDc,&ch,1,&index,GGI_MARK_NONEXISTING_GLYPHS)==GDI_ERROR||index==0xffff)return false;
        reserve_glyphs_.insert(ch);
    }
    MAT2 identity={{0,1},{0,0},{0,0},{0,1}}; GLYPHMETRICS gm={};
    const DWORD bytes=GetGlyphOutlineW(glyphDc,ch,GGO_GRAY8_BITMAP,&gm,0,nullptr,&identity); if(bytes==GDI_ERROR)return false;
    const unsigned w=gm.gmBlackBoxX,h=gm.gmBlackBoxY; if(w+2>width_||h+2>height_)return false;
    unsigned x=x_,y=y_,row=row_; if(x+w+1>width_){x=1;y+=row+2;row=0;} if(y+h+1>height_)return false;
    std::vector<uint8_t> bitmap(bytes);
    if(bytes&&GetGlyphOutlineW(glyphDc,ch,GGO_GRAY8_BITMAP,&gm,bytes,bitmap.data(),&identity)==GDI_ERROR)return false;
    const unsigned stride=(w+3)&~3u; if(size_t(stride)*h>bitmap.size())return false;
    for(unsigned j=0;j<h;++j)for(unsigned i=0;i<w;++i)pixels_[size_t(y+j)*width_+x+i]=uint8_t(std::min(64u,unsigned(bitmap[j*stride+i]))*255/64);
    out={x,y,w,h,gm.gmptGlyphOrigin.x,gm.gmptGlyphOrigin.y,gm.gmCellIncX}; glyphs_.emplace(ch,out);
    if(w&&h)++generation_; x_=x+w+2;y_=y;row_=std::max(row,h);return true;
}

bool MapTtfAtlas::layout_utf8(const char* text, std::vector<MapVertex>& vertices, float& advance) {
    if (!text || !dc_) return false;
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (!count) return false;
    std::vector<wchar_t> wide(count);
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, wide.data(), count)) return false;
    std::vector<MapVertex> result;
    float pen = 0;
    for (int i = 0; i < count - 1; ++i) {
        MapGlyph g = {};
        if (!glyph(wide[i], g)) return false;
        if (wide[i] != L' ' && (!g.width || !g.height)) return false;
        if (i > 0 && !reserve_glyphs_.count(wide[i-1]) && !reserve_glyphs_.count(wide[i])) { auto k = kerning_.find((uint32_t(wide[i-1]) << 16) | uint32_t(wide[i])); if (k != kerning_.end()) pen += k->second; }
        if (g.width && g.height) {
            // GDI reports the black-box origin relative to its baseline. The
            // game's bitmap-font builder uses a bottom-left point with V=1,
            // followed by a top-left point with V=0, in the same upward-facing
            // local coordinate system.
            const float x0 = pen + g.left, x1 = x0 + g.width;
            const float y1 = static_cast<float>(g.top), y0 = y1 - g.height;
            const float u0 = float(g.x)/width_, u1 = float(g.x+g.width)/width_;
            const float v0 = float(g.y)/height_, v1 = float(g.y+g.height)/height_;
            const MapVertex q[] = {{x0,y0,0,u0,v1},{x1,y0,0,u1,v1},{x0,y1,0,u0,v0},
                                   {x1,y1,0,u1,v0},{x1,y0,0,u1,v1},{x0,y1,0,u0,v0}};
            result.insert(result.end(), q, q+6);
        }
        pen += g.advance;
    }
    vertices.swap(result); advance = pen; return true;
}

bool map_place_vertices(std::vector<MapVertex>& vertices, float advance, const float origin[3],
                        const float horizontal[3], const float vertical[3], float scale) {
    if (!origin || !horizontal || !vertical || !std::isfinite(advance) || !std::isfinite(scale) || scale <= 0) return false;
    // FUN_00A26D00 scales each bitmap-font pixel by 0.05 * param_7 before
    // applying the supplied horizontal/vertical basis vectors.
    const float gameScale = scale * 0.05f;
    if (!std::isfinite(gameScale) || gameScale <= 0) return false;
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(origin[i]) || !std::isfinite(horizontal[i]) || !std::isfinite(vertical[i])) return false;
    for (auto& vertex : vertices) {
        const float x = (vertex.x - advance / 2.0f) * gameScale;
        const float y = vertex.y * gameScale;
        vertex.x = origin[0] + horizontal[0]*x + vertical[0]*y;
        vertex.y = origin[1] + horizontal[1]*x + vertical[1]*y;
        vertex.z = origin[2] + horizontal[2]*x + vertical[2]*y;
    }
    return true;
}

bool map_prepare_label(const char* utf8, std::string& uppercase_utf8, std::string& curve_mask) {
    if (!utf8) return false;
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, nullptr, 0);
    if (!n) return false;
    std::vector<wchar_t> input(n);
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, input.data(), n)) return false;
    int cap = LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, input.data(), -1, nullptr, 0, nullptr, nullptr, 0);
    if (!cap) return false;
    std::vector<wchar_t> upper(cap);
    if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_UPPERCASE, input.data(), -1, upper.data(), cap, nullptr, nullptr, 0)) return false;
    std::string mask;
    for (int i=0; i<cap-1; ++i) {
        wchar_t ch = upper[i];
        if (ch >= 0xd800 && ch <= 0xdfff) return false;
        if (ch < 0x20 || ch == 0x7f) return false;
        mask.push_back(ch == L' ' ? ' ' : 'A');
    }
    int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, upper.data(), -1, nullptr, 0, nullptr, nullptr);
    if (!bytes) return false;
    std::vector<char> encoded(bytes);
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, upper.data(), -1, encoded.data(), bytes, nullptr, nullptr)) return false;
    std::string result(encoded.data(), bytes-1);
    uppercase_utf8.swap(result); curve_mask.swap(mask);
    return true;
}

bool MapTtfAtlas::build_country_label(const char* text, std::string& curve_mask,
                                      std::vector<MapVertex>& vertices, float& advance) {
    std::string upper, mask;
    std::vector<MapVertex> result;
    float width = 0;
    if (!map_prepare_label(text, upper, mask) || mask.size() < 2 ||
        !layout_utf8(upper.c_str(), result, width)) return false;
    size_t visible = 0;
    for (char ch : mask) if (ch != ' ') ++visible;
    if (result.size() != visible * 6 || visible == 0 || width <= 0) return false;
    curve_mask.swap(mask); vertices.swap(result); advance = width;
    return true;
}

bool MapTtfAtlas::measure_country_label(const char* text, int padding_x, int padding_y, short (&result)[2]) {
    std::string mask;
    std::vector<MapVertex> vertices;
    float advance = 0;
    if (padding_x < 0 || padding_y < 0 || padding_x > 10000 || padding_y > 10000 ||
        !build_country_label(text, mask, vertices, advance)) return false;
    TEXTMETRICW metrics = {};
    if (!GetTextMetricsW(dc_, &metrics)) return false;
    const double w = std::ceil(double(advance)) + 2.0 * padding_x;
    const double h = double(metrics.tmHeight) + 2.0 * padding_y;
    if (w < 1 || h < 1 || w > 32767 || h > 32767) return false;
    result[0] = static_cast<short>(w);
    result[1] = static_cast<short>(h);
    return true;
}


#include "map_atlas_texture.h"
#include <cstdint>

void MapAtlasTexture::reset() {
    if (texture_) texture_->Release();
    if (device_) device_->Release();
    texture_ = nullptr; device_ = nullptr; width_ = height_ = 0; generation_ = 0;
}

bool MapAtlasTexture::update(IDirect3DDevice9* device, const MapTtfAtlas& atlas) {
    const unsigned width = atlas.width(), height = atlas.height();
    if (!device || !width || !height || atlas.pixels().size() != size_t(width) * height) return false;
    IDirect3DTexture9* target = texture_;
    const bool replace = !target || device_ != device || width_ != width || height_ != height;
    if (!replace && generation_ == atlas.generation()) return true;
    if (replace && FAILED(device->CreateTexture(width, height, 1, 0, D3DFMT_A8R8G8B8,
                                                 D3DPOOL_MANAGED, &target, nullptr))) return false;
    D3DLOCKED_RECT lock = {};
    if (FAILED(target->LockRect(0, &lock, nullptr, 0))) {
        if (replace) target->Release();
        return false;
    }
    for (unsigned y = 0; y < height; ++y) {
        auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(lock.pBits) + size_t(y) * lock.Pitch);
        for (unsigned x = 0; x < width; ++x) {
            const uint32_t alpha = atlas.pixels()[size_t(y) * width + x];
            row[x] = (alpha << 24) | (uint32_t(g_mainMapFontSettings.colorR) << 16) | (uint32_t(g_mainMapFontSettings.colorG) << 8) | uint32_t(g_mainMapFontSettings.colorB); // Preserve glyph coverage and apply the configured RGB tint.
        }
    }
    const HRESULT unlocked = target->UnlockRect(0);
    if (FAILED(unlocked)) { if (replace) target->Release(); return false; }
    if (replace) {
        device->AddRef();
        reset();
        device_ = device; texture_ = target; width_ = width; height_ = height;
    }
    generation_ = atlas.generation();
    return true;
}


#include "map_country_bridge.h"
#include <climits>
#include <cmath>
static_assert(sizeof(void*) == 4, "Victoria II requires Win32");
static_assert(sizeof(std::string) == 24, "Unexpected MSVC x86 release string ABI");

bool map_apply_game_curve(MapGameCurveFn curve, const std::string& mask,
                          std::vector<MapVertex>& vertices, const MapCurveArguments& a) {
    if (!curve || mask.size() < 2 || mask.size() > INT_MAX || vertices.size() > INT_MAX ||
        a.map_width <= 0 || a.map_height <= 0 || !std::isfinite(a.x1) || !std::isfinite(a.x2) ||
        !std::isfinite(a.y1) || !std::isfinite(a.y2) || !std::isfinite(a.origin_x) || !std::isfinite(a.origin_y)) return false;
    size_t visible = 0;
    for (char c : mask) {
        if (c == 'A') ++visible;
        else if (c != ' ') return false;
    }
    if (!visible || visible > size_t(INT_MAX)/6 || vertices.size() != visible*6) return false;
    curve(a.context1, a.context2, &mask, a.context4, a.context5, vertices.data(),
          static_cast<int>(vertices.size()), a.x1,a.y1,a.x2,a.y2,a.origin_x,a.origin_y,
          a.map_width,a.map_height);
    return true;
}


#include "map_country_resources.h"
#include <cmath>
#include <climits>

bool MapCountryResources::initialize(IDirect3DDevice9* device, const wchar_t* path, const wchar_t* reserve_path,
                                     int large_height, int small_height) {
    if (!device || device_ || large_height < 4 || large_height > 256 || small_height < 4 || small_height > 256) return false;
    if (!large_.initialize(path,reserve_path,large_height,1024,1024) ||
        !small_.initialize(path,reserve_path,small_height,512,512)) return false;
    if (!large_texture_.update(device,large_) || !small_texture_.update(device,small_)) return false;
    device_ = device;
    return true;
}

bool MapCountryResources::build(const char* utf8, bool use_small, MapCountryLabel& out) {
    if (!device_) return false;
    auto& atlas = use_small ? small_ : large_;
    auto& texture = use_small ? small_texture_ : large_texture_;
    MapCountryLabel result;
    if (!atlas.build_country_label(utf8,result.mask,result.vertices,result.advance) ||
        !atlas.measure_country_label(utf8,0,0,result.measured) || !texture.update(device_,atlas)) return false;
    result.texture = texture.get();
    out = std::move(result);
    return true;
}

bool MapCountryResources::space_advance(bool use_small, float& out) {
    MapGlyph space = {};
    if (!(use_small ? small_ : large_).glyph(L' ', space) || space.advance <= 0) return false;
    out = static_cast<float>(space.advance);
    return true;
}

bool map_add_tracking(MapCountryLabel& label, unsigned count, float space_advance) {
    if (count > 64 || !std::isfinite(space_advance) || space_advance < 0 || label.mask.empty()) return false;
    size_t visible = 0;
    for (char c : label.mask) { if (c == 'A') ++visible; else if (c != ' ') return false; }
    if (label.vertices.size() != 6 * visible || label.mask.size() > 4096 ||
        (label.mask.size()-1) * size_t(count) > 4096 - label.mask.size()) return false;
    const double extra = double(label.mask.size()-1) * count * space_advance;
    if (!std::isfinite(extra) || label.measured[0] + extra > SHRT_MAX) return false;
    std::string expanded;
    expanded.reserve(label.mask.size() + (label.mask.size()-1)*count);
    std::vector<MapVertex> adjusted = label.vertices;
    size_t glyph_index = 0;
    for (size_t i=0; i<label.mask.size(); ++i) {
        if (i) expanded.append(count, ' ');
        expanded.push_back(label.mask[i]);
        if (label.mask[i] == 'A') {
            float shift = static_cast<float>(double(i)*count*space_advance);
            for (size_t j=0;j<6;++j) adjusted[glyph_index*6+j].x += shift;
            ++glyph_index;
        }
    }
    label.mask.swap(expanded);
    label.vertices.swap(adjusted);
    label.advance += static_cast<float>(extra);
    label.measured[0] = static_cast<short>(std::ceil(label.measured[0] + extra));
    return true;
}


#include "map_country_pipeline.h"
#include <utility>

bool map_prepare_game_country(MapCountryResources& resources, const char* utf8,
                              bool use_small, MapGameCurveFn curve,
                              const MapCurveArguments& curve_args,
                              const float origin[3], const float horizontal[3],
                              const float vertical[3], float scale,
                              MapCountryLabel& out) {
    MapCountryLabel result;
    if (!resources.build(utf8, use_small, result) ||
        !map_place_vertices(result.vertices, result.advance, origin, horizontal, vertical, scale) ||
        !map_apply_game_curve(curve, result.mask, result.vertices, curve_args)) return false;
    out = std::move(result);
    return true;
}

bool map_commit_game_vertices(const MapCountryLabel& label, MapVertex* destination, size_t capacity) {
    if (!destination || label.vertices.empty() || label.vertices.size() > capacity || capacity > 65536) return false;
    // The engine may draw its original byte-based count. Zero spare vertices so
    // these triangles have zero area rather than consuming uninitialised data.
    for (size_t i = 0; i < label.vertices.size(); ++i) destination[i] = label.vertices[i];
    for (size_t i = label.vertices.size(); i < capacity; ++i) destination[i] = {};
    return true;
}


#include "map_label_transaction.h"
#include <utility>

bool MapLabelTransaction::begin(MapCountryResources& resources, const char* text, bool use_small) {
    clear();
    MapCountryLabel candidate;
    if (!resources.build(text, use_small, candidate)) return false;
    label_ = std::move(candidate); state_ = Prepared; return true;
}

bool MapLabelTransaction::add_tracking(unsigned count, float space_advance) {
    return state_ == Prepared && map_add_tracking(label_, count, space_advance);
}

bool MapLabelTransaction::place_and_curve(MapGameCurveFn curve, const MapCurveArguments& args,
                                           const float origin[3], const float horizontal[3],
                                           const float vertical[3], float scale) {
    if (state_ != Prepared) return false;
    MapCountryLabel candidate = label_;
    if (!map_place_vertices(candidate.vertices, candidate.advance, origin, horizontal, vertical, scale) ||
        !map_apply_game_curve(curve, candidate.mask, candidate.vertices, args)) return false;
    label_ = std::move(candidate); state_ = Ready; return true;
}

bool MapLabelTransaction::commit(MapVertex* destination, size_t capacity) const {
    return state_ == Ready && map_commit_game_vertices(label_, destination, capacity);
}

void MapLabelTransaction::clear() {
    state_ = Empty; label_ = MapCountryLabel{};
}

// Snapshot the game MSVC x86 string before CharUpperBuffA modifies it. A bad
// pointer or a legacy/invalid UTF-8 value makes the caller use the original path.
static bool snapshot_game_string(const void* object, char (&output)[8193]) {
    if (!object) return false;
    __try {
        const unsigned char* raw = static_cast<const unsigned char*>(object);
        const uint32_t length = *reinterpret_cast<const uint32_t*>(raw + 16);
        const uint32_t capacity = *reinterpret_cast<const uint32_t*>(raw + 20);
        if (length > 8192 || capacity < length || capacity > 0x100000) return false;
        const char* text = capacity < 16 ? reinterpret_cast<const char*>(raw) : *reinterpret_cast<const char* const*>(raw);
        if (!text) return false;
        for (uint32_t i=0; i<length; ++i) { if (!text[i]) return false; output[i] = text[i]; }
        output[length] = 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool MapLabelTransaction::begin_game_string(MapCountryResources& resources, const void* game_string, bool use_small) {
    char buffer[8193] = {};
    if (!snapshot_game_string(game_string, buffer)) { clear(); return false; }
    return begin(resources, buffer, use_small);
}


#include "map_call_patch.h"
#include <cstring>
#include <limits>

MapCallPatch::~MapCallPatch() { remove(); }

static bool patch_bytes(uint8_t* site, const uint8_t expected[5], const uint8_t next[5]) {
    DWORD old = 0;
    if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = std::memcmp(site, expected, 5) == 0;
    if (ok) { std::memcpy(site,next,5); FlushInstructionCache(GetCurrentProcess(),site,5); }
    DWORD ignored = 0;
    VirtualProtect(site,5,old,&ignored);
    return ok;
}

bool MapCallPatch::install(void* call_site, const uint8_t expected[5], void* replacement) {
    if (site_ || !call_site || !expected || expected[0] != 0xe8 || !replacement) return false;
    auto* site = static_cast<uint8_t*>(call_site);
    int32_t old_relative = 0; std::memcpy(&old_relative, expected+1,4);
    const int64_t offset = int64_t(reinterpret_cast<uintptr_t>(replacement)) - (int64_t(reinterpret_cast<uintptr_t>(site)) + 5);
    if (offset < INT32_MIN || offset > INT32_MAX) return false;
    uint8_t jump[5] = {0xe8}; const int32_t displacement=static_cast<int32_t>(offset);
    std::memcpy(jump+1,&displacement,4);
    if (!patch_bytes(site,expected,jump)) return false;
    std::memcpy(original_,expected,5); std::memcpy(installed_,jump,5);
    original_target_ = reinterpret_cast<void*>(static_cast<uintptr_t>(int64_t(reinterpret_cast<uintptr_t>(site)) + 5 + old_relative));
    site_ = site;
    return true;
}

bool MapCallPatch::remove() {
    if (!site_) return true;
    if (!patch_bytes(site_,installed_,original_)) return false;
    site_ = nullptr; original_target_ = nullptr;
    return true;
}


#include <windows.h>
#include <d3d9.h>
#include <intrin.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include "map_country_hooks.h"
#include "map_country_resources.h"
#include "map_country_bridge.h"
#include "map_call_patch.h"


namespace map_country_detail {
// Addresses below are RVAs derived from the 3.04 v2game.exe disassembly.
constexpr uint32_t kMeasureRva = 0x006279A0;       // FUN_00A279A0
constexpr uint32_t kBuilderCallRva = 0x003DEBE2;  // call FUN_00A26D00
constexpr uint32_t kCurveCallRva = 0x003DEC52;    // call FUN_007DC320
constexpr uint32_t kPrimitiveSetupCallRva = 0x003DEB80; // call FUN_009F9630
constexpr uint32_t kCountryDrawCallRva = 0x003DF0AA;    // call FUN_009F9D50
constexpr uint32_t kCharUpperIatRva = 0x0088A2CC;
constexpr uint32_t kGraphicsGlobalRva = 0x00E58A74;
constexpr uint32_t kLargeMeasureReturnRva = 0x003DE382;
constexpr uint32_t kSmallMeasureReturnRva = 0x003DE5FD;
constexpr uint32_t kUpperMeasureReturnRva = 0x003DE6F4;
constexpr uint32_t kFitMeasureReturnRva = 0x003DE85C;

using MeasureFn = void (__thiscall*)(void*, const std::string*, unsigned, unsigned, int*, short*);
using BuilderFn = int (__stdcall*)(void*, MapVertex*, const std::string*, const float*,
                                  const float*, const float*, float, int);
using CurveFn = void (__cdecl*)(void*, void*, const void*, void*, void*, MapVertex*, int,
                                float, float, float, float, float, float, int, int);
using PrimitiveSetupFn = void (__stdcall*)(void*, void*, uint32_t, uint32_t, uint32_t, uint32_t);
using CharUpperFn = DWORD (WINAPI*)(LPSTR, DWORD);

const V2ModAPI* g_api;
void* g_measureOriginal;
void* g_builderOriginal;
void* g_curveOriginal;
void* g_primitiveSetupOriginal;
void* g_countryDrawOriginal;
CharUpperFn g_charUpperOriginal;
MapCallPatch g_builderPatch;
MapCallPatch g_curvePatch;
MapCallPatch g_primitiveSetupPatch;
MapCallPatch g_countryDrawPatch;
MapCountryResources g_resources;

IDirect3DDevice9* g_device;
bool g_resourceAttempted;
bool g_mapHooksInstalled;
unsigned char g_measureTrampoline[64];
volatile LONG g_deviceLogOnce;
volatile LONG g_measureLogOnce;
volatile LONG g_builderLogOnce;
volatile LONG g_curveLogOnce;
volatile LONG g_measureCalls;
volatile LONG g_builderEntryOnce;
volatile LONG g_curveEntryOnce;
volatile LONG g_ttfDrawCalls;
volatile LONG g_bitmapDrawCalls;
volatile LONG g_countryNameDiagCount;

struct PendingLabel {
    bool active = false;
    MapVertex* destination = nullptr;
    size_t capacity = 0;
    void* primitive = nullptr;
    MapCountryLabel label;
};
thread_local PendingLabel g_pending;
thread_local void* g_currentPrimitive;
thread_local IDirect3DBaseTexture9* g_restoreTexture;
thread_local IDirect3DDevice9* g_restoreDevice;
enum class LabelTexture : uint8_t { Bitmap, TtfLarge, TtfSmall };
SRWLOCK g_labelTextureLock = SRWLOCK_INIT;
std::unordered_map<void*, LabelTexture> g_labelTextures;
thread_local std::string g_rawCountryName;
thread_local size_t g_baseGameLength;
thread_local unsigned g_fitIteration;

uintptr_t GameAddress(uint32_t rva) {
    const uintptr_t base = g_api ? static_cast<uintptr_t>(g_api->gameBase) : 0;
    return base ? base + rva : 0;
}

DWORD CallerRvaAt(uintptr_t caller) {
    const uintptr_t base = g_api ? static_cast<uintptr_t>(g_api->gameBase) : 0;
    return base && caller >= base ? static_cast<DWORD>(caller - base) : 0;
}

extern "C" void __cdecl BeforeCountryDraw(void* primitive, IDirect3DDevice9* device);
extern "C" void __cdecl AfterCountryDraw();

bool SetLabelTexture(void* primitive, LabelTexture texture) {
    if (!primitive) return false;
    AcquireSRWLockExclusive(&g_labelTextureLock);
    bool ok = true;
    try { g_labelTextures[primitive] = texture; }
    catch (...) { ok = false; }
    ReleaseSRWLockExclusive(&g_labelTextureLock);
    return ok;
}

LabelTexture GetLabelTexture(void* primitive) {
    if (!primitive) return LabelTexture::Bitmap;
    AcquireSRWLockShared(&g_labelTextureLock);
    LabelTexture texture = LabelTexture::Bitmap;
    const auto it = g_labelTextures.find(primitive);
    if (it != g_labelTextures.end()) texture = it->second;
    ReleaseSRWLockShared(&g_labelTextureLock);
    return texture;
}

// MSVC x86 std::string layout: 16-byte SSO storage, then size and capacity.
bool SnapshotGameString(const void* object, char* output, size_t capacity, size_t* length) {
    if (!object || !output || capacity < 2 || !length) return false;
    __try {
        const unsigned char* raw = static_cast<const unsigned char*>(object);
        const uint32_t size = *reinterpret_cast<const uint32_t*>(raw + 16);
        const uint32_t reserved = *reinterpret_cast<const uint32_t*>(raw + 20);
        if (size == 0 || size >= capacity || reserved < size || reserved > 0x100000) return false;
        const char* data = reserved < 16 ? reinterpret_cast<const char*>(raw) :
            *reinterpret_cast<const char* const*>(raw);
        if (!data) return false;
        for (uint32_t i = 0; i < size; ++i) {
            if (!data[i]) return false;
            output[i] = data[i];
        }
        output[size] = 0;
        *length = size;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

unsigned TrackingLevel(const void* gameString) {
    char bytes[8193] = {};
    size_t currentLength = 0;
    if (g_baseGameLength < 2 || !SnapshotGameString(gameString, bytes, sizeof(bytes), &currentLength) ||
        currentLength < g_baseGameLength) return 0;
    const size_t gaps = g_baseGameLength - 1;
    const size_t added = currentLength - g_baseGameLength;
    // The engine inserts the same number of ASCII spaces between every two
    // original bytes. Values above 64 are capped before reaching our atlas.
    return static_cast<unsigned>((added / gaps) > 64 ? 65 : (added / gaps));
}

bool IsValidUtf8(const char* text) {
    if (!text || !*text) return false;
    const int chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    return chars > 1;
}

std::string EffectiveCountryName(const std::string& captured, const void* gameString) {
    char buffer[8193] = {};
    size_t length = 0;
    std::string text;
    // The CharUpperBuffA hook can be called more than once while the engine
    // prepares a composite map label (for example, an owner adjective plus a
    // colonial region). In that case the last captured fragment is shorter
    // than the complete, tracked string passed to the glyph builder. Recover
    // the builder's original string by removing the per-byte tracking spaces
    // before choosing between the two sources.
    std::string untracked;
    if (SnapshotGameString(gameString, buffer, sizeof(buffer), &length)) {
        const unsigned tracking = TrackingLevel(gameString);
        if (tracking && g_baseGameLength > 0) {
            untracked.reserve(g_baseGameLength);
            size_t i = 0;
            while (i < length && untracked.size() < g_baseGameLength) {
                untracked.push_back(buffer[i++]);
                if (untracked.size() < g_baseGameLength) {
                    unsigned skipped = 0;
                    while (i < length && skipped < tracking && buffer[i] == ' ') {
                        ++i;
                        ++skipped;
                    }
                    // Keep the candidate only if every expected tracking
                    // space was present; otherwise it is not a safe decode.
                    if (skipped != tracking) {
                        untracked.clear();
                        break;
                    }
                }
            }
            if (untracked.size() != g_baseGameLength || !IsValidUtf8(untracked.c_str()))
                untracked.clear();
        }
        if (untracked.empty() && IsValidUtf8(buffer)) untracked.assign(buffer, length);
    }
    const bool capturedValid = !captured.empty() && IsValidUtf8(captured.c_str());
    if (!untracked.empty() && (!capturedValid || untracked.size() > captured.size()))
        text = untracked;
    else if (capturedValid)
        text = captured;
    else if (!untracked.empty())
        text = untracked;
    if (text.size() >= 4) {
        const char* p = text.data();
        if ((p[0] == 'T' || p[0] == 't') && (p[1] == 'H' || p[1] == 'h') &&
            (p[2] == 'E' || p[2] == 'e') && p[3] == ' ')
            text.erase(0, 4);
    }
    return text;
}

void LogCountryNameInput(const std::string& effective, const std::string& captured,
                         const void* gameString) {
    if (!g_api || !g_api->Log) return;
    const LONG event = InterlockedIncrement(&g_countryNameDiagCount);
    if (event > 24) return;
    auto hexBytes = [](const char* data, size_t length, char (&out)[193]) {
        size_t used = 0;
        const size_t limit = length < 48 ? length : 48;
        for (size_t i = 0; i < limit && used + 4 < sizeof(out); ++i)
            used += (size_t)_snprintf_s(out + used, sizeof(out) - used, _TRUNCATE,
                "%02X ", (unsigned char)data[i]);
        if (length > limit && used + 4 < sizeof(out))
            _snprintf_s(out + used, sizeof(out) - used, _TRUNCATE, "...");
    };
    char effectiveHex[193] = {}, capturedHex[193] = {}, gameHex[193] = {};
    hexBytes(effective.data(), effective.size(), effectiveHex);
    hexBytes(captured.data(), captured.size(), capturedHex);
    char gameBytes[8193] = {};
    size_t gameLength = 0;
    const bool haveGameString = SnapshotGameString(gameString, gameBytes, sizeof(gameBytes), &gameLength);
    if (haveGameString) hexBytes(gameBytes, gameLength, gameHex);
    const int wideCount = effective.empty() ? 0 : MultiByteToWideChar(CP_UTF8,
        MB_ERR_INVALID_CHARS, effective.data(), (int)effective.size(), nullptr, 0);
    char codepoints[256] = {};
    if (wideCount > 0 && wideCount < 128) {
        std::vector<wchar_t> wide((size_t)wideCount);
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, effective.data(),
            (int)effective.size(), wide.data(), wideCount) == wideCount) {
            size_t used = 0;
            for (int i = 0; i < wideCount && used + 12 < sizeof(codepoints); ++i)
                used += (size_t)_snprintf_s(codepoints + used, sizeof(codepoints) - used, _TRUNCATE,
                    "U+%04X ", (unsigned)wide[i]);
        }
    }
    (void)0;
}

bool GetPadding(int* padding, int& x, int& y) {
    x = y = 0;
    if (!padding) return true;
    __try {
        x = padding[0]; y = padding[1];
        return x >= 0 && y >= 0 && x < 10000 && y < 10000;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        x = y = 0;
        return false;
    }
}

bool IsSmallFont(const void* font) {
    if (!font) return false;
    __try {
        const int height = *reinterpret_cast<const int*>(static_cast<const unsigned char*>(font) + 0x494);
        return height > 0 && height <= 30;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

bool InitializeForFont(void* font) {
    if (!g_mapHooksInstalled || !g_api || !g_api->modFolder || !g_mainMapFontSettings.active || !g_mainMapFontSettings.userTtfFile[0]) return false;
    const uintptr_t global = GameAddress(kGraphicsGlobalRva);
    if (!global) return false;
    void* graphics = nullptr;
    __try { graphics = *reinterpret_cast<void**>(global); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!graphics) return false;
    IDirect3DDevice9* device = nullptr;
    __try { device = *reinterpret_cast<IDirect3DDevice9**>(static_cast<unsigned char*>(graphics) + 0x64); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    if (!device) return false;
    if (InterlockedCompareExchange(&g_deviceLogOnce, 1, 0) == 0 && g_api->Log)
        (void)0;

    if (!g_resourceAttempted) {
        g_resourceAttempted = true;
        wchar_t modFolder[MAX_PATH] = {};
        wchar_t ttfPath[MAX_PATH] = {};
        wchar_t reservePath[MAX_PATH] = {};
        if (!MultiByteToWideChar(CP_ACP, 0, g_api->modFolder, -1, modFolder, MAX_PATH) ||
            _snwprintf_s(ttfPath, _countof(ttfPath), _TRUNCATE,
                L"%s\\TTF Unicode Support\\%s", modFolder, g_mainMapFontSettings.userTtfFile) < 0 ||
            (g_mainMapFontSettings.reserveFontFile[0] && _snwprintf_s(reservePath, _countof(reservePath), _TRUNCATE,
                L"%s\\TTF Unicode Support\\%s", modFolder, g_mainMapFontSettings.reserveFontFile) < 0) ||
            !g_resources.initialize(device, ttfPath, g_mainMapFontSettings.reserveFontFile[0] ? reservePath : nullptr,
                g_mainMapFontSettings.height, g_mainMapFontSettings.smallHeight)) {
            if (g_api->Log) (void)0;
            return false;
        }
        g_device = device;
    }
    if (device != g_device || !g_device) return false;
    return true;
}

extern "C" void __cdecl BeforeCountryDraw(void* primitive, IDirect3DDevice9* device) {
    g_restoreTexture = nullptr;
    g_restoreDevice = nullptr;
    const LabelTexture mode = GetLabelTexture(primitive);
    if (mode == LabelTexture::Bitmap || !device || device != g_device) {
        const LONG count = InterlockedIncrement(&g_bitmapDrawCalls);
        if ((count == 1 || count == 100 || count == 1000) && g_api && g_api->Log)
            (void)0;
        return;
    }
    IDirect3DTexture9* replacement = g_resources.texture(mode == LabelTexture::TtfSmall);
    IDirect3DBaseTexture9* previous = nullptr;
    if (!replacement || FAILED(device->GetTexture(0, &previous))) return;
    const HRESULT hr = device->SetTexture(0, replacement);
    if (FAILED(hr)) {
        previous->Release();
        return;
    }
    g_restoreDevice = device;
    g_restoreTexture = previous;
    const LONG count = InterlockedIncrement(&g_ttfDrawCalls);
    if ((count == 1 || count == 100 || count == 1000) && g_api && g_api->Log)
        (void)0;
}

extern "C" void __cdecl AfterCountryDraw() {
    if (g_restoreDevice && g_restoreTexture) {
        g_restoreDevice->SetTexture(0, g_restoreTexture);
        g_restoreTexture->Release();
    }
    g_restoreDevice = nullptr;
    g_restoreTexture = nullptr;
}

void __stdcall HookPrimitiveSetup(void* primitive, void* device, uint32_t count,
                                  uint32_t format, uint32_t flags, uint32_t dynamic) {
    g_currentPrimitive = primitive;
    SetLabelTexture(primitive, LabelTexture::Bitmap);
    reinterpret_cast<PrimitiveSetupFn>(g_primitiveSetupOriginal)(primitive, device,
        count, format, flags, dynamic);
}

DWORD WINAPI HookCharUpperBuffA(LPSTR text, DWORD length) {
    const DWORD caller = CallerRvaAt(reinterpret_cast<uintptr_t>(_ReturnAddress()));
    if (caller == 0x003DE163 && text && length > 0 && length < 8192) {
        try { g_rawCountryName.assign(text, length); }
        catch (...) { g_rawCountryName.clear(); }
    }
    return g_charUpperOriginal ? g_charUpperOriginal(text, length) : 0;
}

void __fastcall HookMeasure(void* self, void*, const std::string* text, unsigned maxWidth,
                            unsigned flags, int* padding, short* result) {
    const DWORD caller = CallerRvaAt(reinterpret_cast<uintptr_t>(_ReturnAddress()));
    if ((caller == kLargeMeasureReturnRva || caller == kSmallMeasureReturnRva) && text) {
        char bytes[8193] = {};
        size_t length = 0;
        g_baseGameLength = SnapshotGameString(text, bytes, sizeof(bytes), &length) ? length : 0;
        g_fitIteration = 0;
    }
    const bool countryCall = caller == kLargeMeasureReturnRva || caller == kSmallMeasureReturnRva ||
        caller == kUpperMeasureReturnRva || caller == kFitMeasureReturnRva;
    if (countryCall) {
        const LONG call = InterlockedIncrement(&g_measureCalls);
        if ((call == 100 || call == 1000 || call == 10000) && g_api && g_api->Log)
            (void)0;
    }
    if (countryCall && result && InitializeForFont(self)) {
        int padX = 0, padY = 0;
        std::string effective = EffectiveCountryName(g_rawCountryName, text);
        MapCountryLabel label;
        const bool useSmall = IsSmallFont(self);
        if (GetPadding(padding, padX, padY) && g_resources.build(effective.c_str(), useSmall, label)) {
            if (caller == kFitMeasureReturnRva) {
                if (g_fitIteration < 65) ++g_fitIteration;
                const unsigned fromLength = TrackingLevel(text);
                const unsigned tracking = fromLength > g_fitIteration ? fromLength : g_fitIteration;
                float spaceAdvance = 0;
                if (tracking > 64) {
                    result[0] = 32767;
                    result[1] = label.measured[1];
                    return;
                }
                if (tracking && (!g_resources.space_advance(useSmall, spaceAdvance) ||
                    !map_add_tracking(label, tracking, spaceAdvance))) {
                    result[0] = 32767;
                    result[1] = label.measured[1];
                    return;
                }
            }
            const int w = int(label.measured[0]) + 2 * padX;
            const int h = int(label.measured[1]) + 2 * padY;
            if (w > 0 && h > 0 && w <= 32767 && h <= 32767) {
                result[0] = static_cast<short>(w);
                result[1] = static_cast<short>(h);
                if (InterlockedCompareExchange(&g_measureLogOnce, 1, 0) == 0 && g_api && g_api->Log)
                    (void)0;
                return;
            }
        }
    }
    reinterpret_cast<MeasureFn>(g_measureOriginal)(self, text, maxWidth, flags, padding, result);
}

int __stdcall HookGlyphBuilder(void* font, MapVertex* destination, const std::string* gameString,
                              const float* origin, const float* horizontal, const float* vertical,
                              float scale, int maxWidth) {
    if (InterlockedCompareExchange(&g_builderEntryOnce, 1, 0) == 0 && g_api && g_api->Log)
        (void)0;
    g_pending.active = false;
    if (g_currentPrimitive) SetLabelTexture(g_currentPrimitive, LabelTexture::Bitmap);
    if (!destination || !gameString || !origin || !horizontal || !vertical ||
        !InitializeForFont(font)) {
        return reinterpret_cast<BuilderFn>(g_builderOriginal)(font, destination, gameString,
            origin, horizontal, vertical, scale, maxWidth);
    }
    std::string effective = EffectiveCountryName(g_rawCountryName, gameString);
    LogCountryNameInput(effective, g_rawCountryName, gameString);
    MapCountryLabel label;
    const bool useSmall = IsSmallFont(font);
    const unsigned fromLength = TrackingLevel(gameString);
    const unsigned tracking = fromLength > g_fitIteration ? fromLength : g_fitIteration;
    float spaceAdvance = 0;
    if (effective.empty() || !g_resources.build(effective.c_str(), useSmall, label) ||
        tracking > 64 ||
        (tracking && (!g_resources.space_advance(useSmall, spaceAdvance) ||
            !map_add_tracking(label, tracking, spaceAdvance))) ||
        !map_place_vertices(label.vertices, label.advance, origin, vertical, horizontal, scale)) {
        g_rawCountryName.clear();
        return reinterpret_cast<BuilderFn>(g_builderOriginal)(font, destination, gameString,
            origin, horizontal, vertical, scale, maxWidth);
    }
    char bytes[8193] = {};
    size_t byteCount = 0;
    size_t capacity = 0;
    if (!SnapshotGameString(gameString, bytes, sizeof(bytes), &byteCount)) {
        g_rawCountryName.clear();
        return reinterpret_cast<BuilderFn>(g_builderOriginal)(font, destination, gameString,
            origin, horizontal, vertical, scale, maxWidth);
    }
    for (size_t i = 0; i < byteCount; ++i) if (bytes[i] != ' ') ++capacity;
    capacity *= 6;
    if (!capacity || label.vertices.size() > capacity || capacity > 65536) {
        g_rawCountryName.clear();
        return reinterpret_cast<BuilderFn>(g_builderOriginal)(font, destination, gameString,
            origin, horizontal, vertical, scale, maxWidth);
    }
    if (!g_currentPrimitive || !SetLabelTexture(g_currentPrimitive,
            useSmall ? LabelTexture::TtfSmall : LabelTexture::TtfLarge)) {
        g_rawCountryName.clear();
        return reinterpret_cast<BuilderFn>(g_builderOriginal)(font, destination, gameString,
            origin, horizontal, vertical, scale, maxWidth);
    }
    for (size_t i = 0; i < label.vertices.size(); ++i) destination[i] = label.vertices[i];
    for (size_t i = label.vertices.size(); i < capacity; ++i) destination[i] = {};
    g_pending.active = true;
    g_pending.destination = destination;
    g_pending.capacity = capacity;
    g_pending.primitive = g_currentPrimitive;
    g_pending.label = std::move(label);
    if (InterlockedCompareExchange(&g_builderLogOnce, 1, 0) == 0 && g_api && g_api->Log)
        (void)0;
    g_rawCountryName.clear();
    return static_cast<int>(g_pending.label.vertices.size());
}

void __cdecl HookCountryCurve(void* context1, void* context2, const void* gameString,
                              void* context4, void* context5, MapVertex* destination,
                              int vertexCount, float x1, float y1, float x2, float y2,
                              float originX, float originY, int mapWidth, int mapHeight) {
    if (InterlockedCompareExchange(&g_curveEntryOnce, 1, 0) == 0 && g_api && g_api->Log)
        (void)0;
    const auto original = reinterpret_cast<CurveFn>(g_curveOriginal);
    if (g_pending.active && destination == g_pending.destination &&
        g_pending.label.vertices.size() <= g_pending.capacity) {
        const MapCurveArguments args{context1, context2, context4, context5,
            x1, y1, x2, y2, originX, originY, mapWidth, mapHeight};
        if (map_apply_game_curve(original, g_pending.label.mask, g_pending.label.vertices, args)) {
            for (size_t i = 0; i < g_pending.label.vertices.size(); ++i)
                destination[i] = g_pending.label.vertices[i];
            for (size_t i = g_pending.label.vertices.size(); i < g_pending.capacity; ++i)
                destination[i] = {};
            if (InterlockedCompareExchange(&g_curveLogOnce, 1, 0) == 0 && g_api && g_api->Log)
                (void)0;
            g_pending.active = false;
            g_currentPrimitive = nullptr;
            return;
        }
    }
    if (g_pending.primitive) SetLabelTexture(g_pending.primitive, LabelTexture::Bitmap);
    else if (g_currentPrimitive) SetLabelTexture(g_currentPrimitive, LabelTexture::Bitmap);
    g_pending.active = false;
    original(context1, context2, gameString, context4, context5, destination, vertexCount,
        x1, y1, x2, y2, originX, originY, mapWidth, mapHeight);
    g_currentPrimitive = nullptr;
}

bool PatchCharUpperIat() {
    const uintptr_t slotAddress = GameAddress(kCharUpperIatRva);
    if (!slotAddress) return false;
    auto* slot = reinterpret_cast<void**>(slotAddress);
    __try {
        // Preserve the pointer currently installed in the executable's named
        // CharUpperBuffA IAT slot. CreatineCore/Windows may route it through a
        // local import thunk, so comparing it with GetProcAddress is brittle.
        auto original = reinterpret_cast<CharUpperFn>(*slot);
        if (!original || reinterpret_cast<void*>(original) == reinterpret_cast<void*>(&HookCharUpperBuffA)) return false;
        DWORD old = 0;
        if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &old)) return false;
        g_charUpperOriginal = original;
        *slot = reinterpret_cast<void*>(&HookCharUpperBuffA);
        DWORD ignored = 0;
        VirtualProtect(slot, sizeof(void*), old, &ignored);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
} // namespace

extern "C" __declspec(naked) void HookCountryDraw() {
    using namespace map_country_detail;
    __asm {
        pushad
        push esi
        push eax
        call BeforeCountryDraw
        add esp, 8
        popad
        call dword ptr [g_countryDrawOriginal]
        pushad
        call AfterCountryDraw
        popad
        ret
    }
}

bool map_install_country_hooks(const V2ModAPI* api) {
    using namespace map_country_detail;
    using map_country_detail::g_api;
    if (!api || !api->Log || !api->StealToTrampoline || !api->gameBase) return false;
    g_api = api;
    const unsigned char builderBytes[5] = {0xE8,0x19,0x81,0x24,0x00};
    const unsigned char curveBytes[5] = {0xE8,0xC9,0xD6,0xFF,0xFF};
    const unsigned char primitiveSetupBytes[5] = {0xE8,0xAB,0xAA,0x21,0x00};
    const unsigned char countryDrawBytes[5] = {0xE8,0xA1,0xAC,0x21,0x00};
    const uintptr_t base = api->gameBase;
    if (!g_primitiveSetupPatch.install(reinterpret_cast<void*>(base + kPrimitiveSetupCallRva),
            primitiveSetupBytes, reinterpret_cast<void*>(&HookPrimitiveSetup))) {
        (void)0;
        return false;
    }
    g_primitiveSetupOriginal = g_primitiveSetupPatch.original_target();
    if (!g_countryDrawPatch.install(reinterpret_cast<void*>(base + kCountryDrawCallRva),
            countryDrawBytes, reinterpret_cast<void*>(&HookCountryDraw))) {
        g_primitiveSetupPatch.remove();
        (void)0;
        return false;
    }
    g_countryDrawOriginal = g_countryDrawPatch.original_target();
    if (!g_builderPatch.install(reinterpret_cast<void*>(base + kBuilderCallRva),
            builderBytes, reinterpret_cast<void*>(&HookGlyphBuilder))) {
        g_countryDrawPatch.remove(); g_primitiveSetupPatch.remove();
        (void)0;
        return false;
    }
    g_builderOriginal = g_builderPatch.original_target();
    if (!g_curvePatch.install(reinterpret_cast<void*>(base + kCurveCallRva),
            curveBytes, reinterpret_cast<void*>(&HookCountryCurve))) {
        g_builderPatch.remove(); g_countryDrawPatch.remove(); g_primitiveSetupPatch.remove();
        (void)0;
        return false;
    }
    g_curveOriginal = g_curvePatch.original_target();
    const unsigned char measurePrologue[6] = {0x55,0x8B,0xEC,0x83,0xEC,0x20};
    memset(g_measureTrampoline, 0, sizeof(g_measureTrampoline));
    if (!api->StealToTrampoline(0x006279A0, sizeof(measurePrologue), g_measureTrampoline,
            sizeof(g_measureTrampoline), measurePrologue, reinterpret_cast<void*>(&HookMeasure),
            &g_measureOriginal, "TTF country-label measurement")) {
        g_curvePatch.remove(); g_builderPatch.remove();
        g_countryDrawPatch.remove(); g_primitiveSetupPatch.remove();
        (void)0;
        return false;
    }
    g_mapHooksInstalled = true;
    const bool captureHook = PatchCharUpperIat();
    (void)0;
    return true;
}


#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <cwctype>
#include <numeric>
#include <cstdio>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include "map_province_ttf_hooks.h"


#include "map_call_patch.h"


#pragma comment(lib, "gdi32.lib")

namespace map_province_detail {
// These RVAs/signatures are for Victoria II 3.04, SHA-256
// 62D48C204364DD706584777C2E2B3C7AB3C5F1DD0170872554943575D53D6648.
constexpr uint32_t kProvinceBuilderCallRva = 0x003F20D5;
constexpr uint32_t kProvinceLockCallRva = 0x003F1FF1;
constexpr uint32_t kProvinceUnlockCallRva = 0x003F2114;
// Final renderer invocation inside the province-label draw routine.
// Province-label loop calls the shared renderer here. Hook this caller, not
// an instruction inside the shared renderer: the caller gives us the exact
// province renderable in EAX, which is also the key captured at buffer lock.
constexpr uint32_t kProvinceDrawCallRva = 0x003FC2D5;
constexpr uint32_t kProvinceDrawTargetRva = 0x005F9D50;
constexpr uint32_t kActualTriangleDrawRva = 0x005F9DE7;
constexpr uint32_t kGraphicsGlobalRva = 0x00E58A74;
constexpr size_t kProvinceBufferBytes = 6000;
constexpr size_t kMaxUtf8Bytes = 50; // vanilla already clamps to 49 bytes; don't change that in this first run
constexpr size_t kMaxTextures = 8192;
constexpr int kMaxTtfTextWidth = 1020; // +4px padding remains within a 1024px POT texture
constexpr uint32_t kSlotExhaustionRva = 0x003F1CF1;
constexpr uint32_t kSlotCandidateRva = 0x003F1CD3;

unsigned PowerOfTwo(unsigned value) {
    unsigned result = 1;
    while (result < value) result <<= 1;
    return result;
}

struct Vertex { float x, y, z, u, v; };
static_assert(sizeof(Vertex) == 20, "Province vertex stride changed");
using BuilderFn = int (__stdcall*)(void*, void*, const std::string*, const float*,
    const float*, const float*, float, int);

const V2ModAPI* g_api;
void* g_builderOriginal;
uintptr_t g_lockContinuation;
uintptr_t g_unlockContinuation;
uintptr_t g_provinceDrawOriginal;
uintptr_t g_triangleDrawContinuation;
MapCallPatch g_builderPatch;
thread_local void* g_currentRenderable;
thread_local uintptr_t g_lockedProvinceData;
thread_local LONG g_provinceLockHr = E_PENDING;
thread_local uintptr_t g_builderDestination;
thread_local bool g_traceBufferThisLabel;
thread_local bool g_traceWrapLabel;
thread_local void* g_unlockResource;
thread_local std::string g_currentBuilderTraceName;
thread_local void* g_currentSlotCandidate;
void* g_viennaDiagnosticRenderable;
IDirect3DDevice9* g_device;
HANDLE g_fontResource;
HFONT g_font;
HDC g_dc;
HGDIOBJ g_oldFont;
std::vector<uint8_t> g_provinceFontBytes;
std::unordered_map<void*, IDirect3DTexture9*> g_textures;
SRWLOCK g_textureLock = SRWLOCK_INIT;
std::unordered_map<void*, std::string> g_traceNames;
SRWLOCK g_traceNameLock = SRWLOCK_INIT;
std::unordered_map<unsigned, std::string> g_provinceIdNames;
SRWLOCK g_provinceNameLock = SRWLOCK_INIT;
struct ProvinceDiagGeometry {
    std::string name;
    Vertex vertices[6]{};
    unsigned width = 0, height = 0, textureWidth = 0, textureHeight = 0;
    unsigned alphaPixels = 0, alphaMin = 0, alphaMax = 0;
    int alphaLeft = -1, alphaTop = -1, alphaRight = -1, alphaBottom = -1;
    uint64_t pixelHash = 0;
    int vanillaVertexCount = 0;
};
std::unordered_map<void*, ProvinceDiagGeometry> g_provinceGeometry;
SRWLOCK g_provinceGeometryLock = SRWLOCK_INIT;
std::unordered_map<void*, uint64_t> g_diagDrawSignatures;
SRWLOCK g_diagDrawLock = SRWLOCK_INIT;
std::unordered_map<void*, uint64_t> g_diagRouteSignatures;
SRWLOCK g_diagRouteLock = SRWLOCK_INIT;
volatile LONG g_slotCandidateCount;
volatile LONG g_slotExhaustionCount;
volatile LONG g_builderFallbackCount;
volatile LONG g_projectionTraceCount;
volatile LONG g_independentWrapLogCount;
uintptr_t g_slotExhaustionContinuation;
uintptr_t g_slotCandidateContinuation;
unsigned g_lastDrawProvinceId = 0xffffffffu;
void* g_lastDrawHandle;
void* g_lastDrawTextureKey;
IDirect3DTexture9* g_expectedDrawTexture;
std::string g_lastDrawName;
bool g_lastDrawTtf = false;
volatile LONG g_builderLogOnce;
volatile LONG g_drawLogOnce;
volatile LONG g_viennaDrawLogOnce;
volatile LONG g_viennaTraceDrawOnce;
volatile LONG g_resicTraceDrawOnce;
volatile LONG g_sanktTraceDrawOnce,g_szekesTraceDrawOnce,g_zalaTraceDrawOnce,g_banskTraceDrawOnce;
volatile LONG g_wrapWestBermudaOnce,g_wrapEastBermudaOnce,g_wrapSargassoOnce,g_wrapMediterraneanOnce,g_wrapApproachesOnce;
constexpr unsigned kCharsetProbeProvinceIds[] = {612,613,614,615,616,617,618,619,620,621,622,623,624};
volatile LONG g_charsetSlotTraceOnce[_countof(kCharsetProbeProvinceIds)]{};
volatile LONG g_charsetBuilderTraceOnce[_countof(kCharsetProbeProvinceIds)]{};
volatile LONG g_charsetVanillaTraceOnce[_countof(kCharsetProbeProvinceIds)]{};
volatile LONG g_provinceUtf8RepairTraceCount;
volatile LONG g_viennaSelectedOnce;
volatile LONG g_ttfAlphaOverrideLogOnce;
volatile LONG g_resicSelectedOnce;
volatile LONG g_fallbackLogOnce;
volatile LONG g_utf8LogOnce;
volatile LONG g_lastDrawCallResult;
volatile LONG g_drawResultCaptured;
volatile LONG g_frameViennaOnce,g_frameResicOnce,g_frameSanktOnce,g_frameSzekesOnce,g_frameZalaOnce,g_frameBanskOnce;
volatile LONG g_deviceIdentityViennaOnce,g_deviceIdentityResicOnce,g_deviceIdentitySanktOnce,g_deviceIdentitySzekesOnce,g_deviceIdentityZalaOnce,g_deviceIdentityBanskOnce;
struct ProvinceFrameSnapshot { UINT width=0,height=0; D3DFORMAT format=D3DFMT_UNKNOWN; std::vector<uint32_t> pixels; };
ProvinceFrameSnapshot g_frameBefore;
HRESULT g_frameBeforeHr=E_PENDING;
IDirect3DDevice9* g_frameDiagDevice;
std::string g_frameDiagName;
void* g_frameDiagKey;
bool g_frameDiagArmed;
IDirect3DQuery9* g_frameOcclusionQuery;
bool g_frameOcclusionStarted;
bool g_frameOcclusionEnded;
bool g_frameOcclusionTtfPath;
std::string g_frameOcclusionName;
ULONGLONG g_frameOcclusionStartTick;
unsigned g_frameOcclusionPolls;
thread_local std::string g_activeDrawTraceName;

extern "C" void __cdecl CaptureActualTriangleDraw(void* device, unsigned vertexCount, unsigned primitiveCount) {
    if (g_activeDrawTraceName.empty() || !g_api || !g_api->Log) return;
    (void)0;
    IDirect3DDevice9* d=static_cast<IDirect3DDevice9*>(device);
    const D3DRENDERSTATETYPE ids[]={D3DRS_FILLMODE,D3DRS_SHADEMODE,D3DRS_CLIPPING,D3DRS_CLIPPLANEENABLE,
        D3DRS_STENCILENABLE,D3DRS_STENCILFUNC,D3DRS_STENCILREF,D3DRS_STENCILMASK,D3DRS_STENCILWRITEMASK,
        D3DRS_STENCILFAIL,D3DRS_STENCILZFAIL,D3DRS_STENCILPASS,D3DRS_MULTISAMPLEANTIALIAS,D3DRS_MULTISAMPLEMASK,
        D3DRS_SCISSORTESTENABLE,D3DRS_FOGENABLE,D3DRS_LIGHTING,D3DRS_COLORWRITEENABLE,D3DRS_CULLMODE,
        D3DRS_ZENABLE,D3DRS_ZFUNC,D3DRS_ALPHATESTENABLE,D3DRS_ALPHAFUNC,D3DRS_ALPHAREF,D3DRS_TEXTUREFACTOR};
    DWORD v[sizeof(ids)/sizeof(ids[0])]{};HRESULT h[sizeof(ids)/sizeof(ids[0])]{};
    for(size_t i=0;i<sizeof(ids)/sizeof(ids[0]);++i)h[i]=d->GetRenderState(ids[i],&v[i]);
    (void)0;
}

extern "C" void __cdecl RecordActualTriangleDrawResult(HRESULT hr) {
    if (!g_activeDrawTraceName.empty() && g_api && g_api->Log)
        (void)0;
}

extern "C" void __cdecl RecordProvinceLockResult(HRESULT hr, void* data) {
    g_provinceLockHr = hr;
    g_lockedProvinceData = reinterpret_cast<uintptr_t>(data);
}

extern "C" void __cdecl CaptureProvinceUnlockResource(void* resource) { g_unlockResource=resource; }

extern "C" void __cdecl RecordProvinceUnlockResult(HRESULT hr) {
    if (g_traceBufferThisLabel && g_api && g_api->Log)
        (void)0;
    g_lockedProvinceData = 0;
    g_builderDestination = 0;
    g_provinceLockHr = E_PENDING;
    g_traceBufferThisLabel = false;
    g_unlockResource=nullptr;
    g_currentBuilderTraceName.clear();
}

uintptr_t Address(uint32_t rva) {
    return g_api && g_api->gameBase ? uintptr_t(g_api->gameBase) + rva : 0;
}

bool GetDevice(IDirect3DDevice9** out) {
    if (!out) return false;
    *out = nullptr;
    const uintptr_t slot = Address(kGraphicsGlobalRva);
    if (!slot) return false;
    __try {
        void* graphics = *reinterpret_cast<void**>(slot);
        if (!graphics) return false;
        *out = *reinterpret_cast<IDirect3DDevice9**>(static_cast<unsigned char*>(graphics) + 0x64);
    } __except (EXCEPTION_EXECUTE_HANDLER) { *out = nullptr; }
    return *out != nullptr;
}

bool SnapshotString(const void* object, char* out, size_t cap, size_t* length) {
    if (!object || !out || cap < 2 || !length) return false;
    __try {
        const auto* p = static_cast<const unsigned char*>(object);
        const uint32_t size = *reinterpret_cast<const uint32_t*>(p + 16);
        const uint32_t capacity = *reinterpret_cast<const uint32_t*>(p + 20);
        if (!size || size >= cap || capacity < size || capacity > 0x100000) return false;
        const char* data = capacity < 16 ? reinterpret_cast<const char*>(p) :
            *reinterpret_cast<const char* const*>(p);
        if (!data) return false;
        for (uint32_t i = 0; i < size; ++i) {
            if (!data[i]) return false;
            out[i] = data[i];
        }
        out[size] = 0;
        *length = size;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

int CharsetProbeIndex(unsigned id);

bool IsStrictUtf8(const char* bytes, size_t length) {
    if (!bytes || !length || length > kMaxUtf8Bytes) return false;
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes,
        static_cast<int>(length), nullptr, 0) > 0;
}

// Return the mutable storage of the confirmed MSVC x86 std::string at label+0x24.
// This helper deliberately contains no C++ objects with destructors because it
// uses SEH to guard reads from engine-owned memory.
bool GetMutableStringData(void* object, size_t expectedLength, char** dataOut) {
    if (!object || !dataOut || !expectedLength) return false;
    *dataOut = nullptr;
    __try {
        auto* p = static_cast<unsigned char*>(object);
        const uint32_t size = *reinterpret_cast<const uint32_t*>(p + 16);
        const uint32_t capacity = *reinterpret_cast<const uint32_t*>(p + 20);
        if (size != expectedLength || capacity < size || capacity > 0x100000) return false;
        char* data = capacity < 16 ? reinterpret_cast<char*>(p) :
            *reinterpret_cast<char**>(p);
        if (!data) return false;
        *dataOut = data;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { *dataOut = nullptr; return false; }
}

void RepairProvinceUtf8Escapes(void* label, unsigned id, char* raw, size_t length) {
    if (!label || !raw || !length || length > kMaxUtf8Bytes || IsStrictUtf8(raw, length)) return;

    char repaired[kMaxUtf8Bytes + 1]{};
    std::memcpy(repaired, raw, length);
    unsigned changed1D = 0, changed7F = 0;
    for (size_t i = 0; i < length;) {
        const unsigned char lead = static_cast<unsigned char>(repaired[i]);
        unsigned continuationCount = lead >= 0xC2 && lead <= 0xDF ? 1u :
            (lead >= 0xE0 && lead <= 0xEF ? 2u :
            (lead >= 0xF0 && lead <= 0xF4 ? 3u : 0u));
        if (!continuationCount) { ++i; continue; }
        ++i;
        for (unsigned j = 0; j < continuationCount && i < length; ++j) {
            unsigned char byte = static_cast<unsigned char>(repaired[i]);
            if (byte == 0x1D) { repaired[i] = static_cast<char>(0xBD); ++changed1D; byte = 0xBD; }
            else if (byte == 0x7F) { repaired[i] = static_cast<char>(0x80); ++changed7F; byte = 0x80; }
            if (byte < 0x80 || byte > 0xBF) break;
            ++i;
        }
    }

    const bool validAfter = IsStrictUtf8(repaired, length);
    bool committed = false;
    if ((changed1D || changed7F) && validAfter) {
        char* target = nullptr;
        if (GetMutableStringData(static_cast<unsigned char*>(label) + 0x24, length, &target)) {
            __try {
                std::memcpy(target, repaired, length);
                target[length] = '\0';
                committed = true;
            } __except (EXCEPTION_EXECUTE_HANDLER) { committed = false; }
        }
    }
    if ((changed1D || changed7F) && g_api && g_api->Log) {
        const LONG trace = InterlockedIncrement(&g_provinceUtf8RepairTraceCount);
        if (trace <= 256) (void)0;
    }
    if (committed) std::memcpy(raw, repaired, length + 1);
}

bool DecodeProvinceName(const char* bytes, size_t length, std::vector<wchar_t>& wide,
    UINT& codePage) {
    if (!bytes || !length || length > kMaxUtf8Bytes) return false;
    const UINT candidates[] = {CP_UTF8, 1250, 1252};
    for (UINT candidate : candidates) {
        const DWORD flags = candidate == CP_UTF8 ? MB_ERR_INVALID_CHARS : MB_ERR_INVALID_CHARS;
        const int count = MultiByteToWideChar(candidate, flags, bytes, int(length), nullptr, 0);
        if (count <= 0 || count > 96) continue;
        wide.assign(size_t(count) + 1, 0);
        if (MultiByteToWideChar(candidate, flags, bytes, int(length), wide.data(), count) == count) {
            wide[size_t(count)] = 0;
            codePage = candidate;
            return true;
        }
        wide.clear();
    }
    return false;
}

bool IsTraceProvince(const wchar_t* name) { return name && (wcscmp(name,L"Vienna")==0 || wcscmp(name,L"Resicab\u00E1nya")==0 || _wcsicmp(name,L"Resicabanya")==0 || wcscmp(name,L"Sankt P\u00F6lten")==0 || wcscmp(name,L"Sz\u00E9kesfeh\u00E9rv\u00E1r")==0 || wcscmp(name,L"Zalaegerszeg")==0 || wcscmp(name,L"Bansk?Bystrica")==0); }
bool IsWrapTraceProvince(const wchar_t* name) {
    return name && (wcscmp(name,L"West Bermuda Sea")==0 || wcscmp(name,L"East Bermuda Sea")==0 ||
        wcscmp(name,L"Western Sargasso Sea")==0 || wcscmp(name,L"Western Mediterranean")==0 ||
        wcscmp(name,L"Bermuda Approaches")==0);
}
std::string ToUtf8(const wchar_t* name);
bool ClaimWrapTrace(const wchar_t* name) {
    if(!name)return false;
    volatile LONG* flag=nullptr;
    if(wcscmp(name,L"West Bermuda Sea")==0)flag=&g_wrapWestBermudaOnce;
    else if(wcscmp(name,L"East Bermuda Sea")==0)flag=&g_wrapEastBermudaOnce;
    else if(wcscmp(name,L"Western Sargasso Sea")==0)flag=&g_wrapSargassoOnce;
    else if(wcscmp(name,L"Western Mediterranean")==0)flag=&g_wrapMediterraneanOnce;
    else if(wcscmp(name,L"Bermuda Approaches")==0)flag=&g_wrapApproachesOnce;
    return flag&&InterlockedCompareExchange(flag,1,0)==0;
}
bool BuildVanillaWrappedText(const wchar_t* source,void* destination,int vertexCount,const float* origin,
    const float* horizontal,const float* vertical,float scale,std::wstring& output,int& lineStepPixels,
    float& bottomOffset,unsigned& lineCount) {
    output.clear();lineStepPixels=0;bottomOffset=0;lineCount=0;
    if(!source||!destination||!origin||!horizontal||!vertical||vertexCount<=0||vertexCount%6||
        size_t(vertexCount)>kProvinceBufferBytes/sizeof(Vertex)||!std::isfinite(scale)||scale<=0)return false;
    const float hLen2=horizontal[0]*horizontal[0]+horizontal[1]*horizontal[1]+horizontal[2]*horizontal[2];
    const float vLen2=vertical[0]*vertical[0]+vertical[1]*vertical[1]+vertical[2]*vertical[2];
    if(hLen2<1e-6f||vLen2<1e-6f)return false;
    const size_t glyphs=size_t(vertexCount)/6;
    size_t expected=0;for(const wchar_t* p=source;*p;++p)if(!iswspace(*p))++expected;
    if(expected!=glyphs)return false; // never guess when engine glyphs cannot be mapped 1:1 to source characters
    struct Glyph { float centerH,minH,maxH,minV,maxV; size_t sourceIndex; };
    std::vector<Glyph> items;items.reserve(glyphs);
    const auto* v=static_cast<const Vertex*>(destination);
    size_t sourceIndex=0;
    std::vector<float> heights;heights.reserve(glyphs);
    for(size_t q=0;q<glyphs;++q){
        while(source[sourceIndex]&&iswspace(source[sourceIndex]))++sourceIndex;
        if(!source[sourceIndex])return false;
        Glyph g{};g.sourceIndex=sourceIndex++;
        g.minH=g.minV=std::numeric_limits<float>::infinity();g.maxH=g.maxV=-g.minH;
        for(size_t j=0;j<6;++j){const Vertex& p=v[q*6+j];const float dx=p.x-origin[0],dy=p.y-origin[1],dz=p.z-origin[2];
            const float hp=(dx*horizontal[0]+dy*horizontal[1]+dz*horizontal[2])/hLen2;
            const float vp=(dx*vertical[0]+dy*vertical[1]+dz*vertical[2])/vLen2;
            g.minH=std::min(g.minH,hp);g.maxH=std::max(g.maxH,hp);g.minV=std::min(g.minV,vp);g.maxV=std::max(g.maxV,vp);}
        g.centerH=(g.minH+g.maxH)*0.5f;heights.push_back(g.maxH-g.minH);items.push_back(g);
    }
    if(items.empty())return false;
    std::vector<float> sortedHeights=heights;std::sort(sortedHeights.begin(),sortedHeights.end());
    const float medianHeight=sortedHeights[sortedHeights.size()/2];
    const float rowThreshold=std::max(0.5f,medianHeight*1.4f);
    std::vector<size_t> breaks;std::vector<float> rowGaps;
    for(size_t q=1;q<items.size();++q){const float delta=items[q].centerH-items[q-1].centerH;
        if(std::fabs(delta)>rowThreshold){
            size_t k=items[q].sourceIndex;while(k>0&&iswspace(source[k-1]))--k;
            if(k==items[q].sourceIndex)return false; // vanilla broke a word; retain bitmap until that case is deliberately handled
            breaks.push_back(items[q].sourceIndex);rowGaps.push_back(std::fabs(delta));}}
    output.reserve(wcslen(source)+breaks.size());size_t glyphIndex=0;bool pendingSpace=false;
    for(size_t i=0;source[i];){
        if(iswspace(source[i])){pendingSpace=true;++i;continue;}
        if(glyphIndex>=items.size()||items[glyphIndex].sourceIndex!=i)return false;
        const bool newRow=std::find(breaks.begin(),breaks.end(),i)!=breaks.end();
        if(newRow){while(!output.empty()&&iswspace(output.back()))output.pop_back();output.push_back(L'\n');++lineCount;pendingSpace=false;}
        else if(pendingSpace&&!output.empty())output.push_back(L' ');
        pendingSpace=false;output.push_back(source[i]);++glyphIndex;++i;
    }
    if(glyphIndex!=items.size())return false;
    ++lineCount;
    const float gameScale=scale*0.05f;
    if(gameScale<=0||!std::isfinite(gameScale))return false;
    if(!rowGaps.empty())lineStepPixels=std::max(1,int(std::lround((std::accumulate(rowGaps.begin(),rowGaps.end(),0.0f)/rowGaps.size())/gameScale)));
    if(breaks.empty())lineStepPixels=0;
    float lowestCenter=std::numeric_limits<float>::infinity();
    for(const Glyph& g:items)lowestCenter=std::min(lowestCenter,g.centerH);
    for(const Glyph& g:items)if(g.centerH<=lowestCenter+rowThreshold*0.5f)bottomOffset=std::min(bottomOffset==0?g.minH:bottomOffset,g.minH);
    if(g_api&&g_api->Log&&lineCount>1){
        (void)0;
    }
    return true;
}

// Use measured Unicode layout when vanilla bitmap quads cannot be mapped
// one-to-one to decoded codepoints. This also covers names changed at runtime
// by events and decisions.
bool BuildMeasuredTtfWrap(const wchar_t* source,int maxWidth,
    std::wstring& output,int& lineStepPixels,float& bottomOffset,unsigned& lineCount){
    output.clear();lineStepPixels=0;bottomOffset=0;lineCount=0;
    if(!source||!*source||!g_dc)return false;
    const int widthLimit=std::max(64,std::min(maxWidth>0?maxWidth:320,kMaxTtfTextWidth-8));
    std::wstring line;
    auto measure=[&](const std::wstring& text,int& width)->bool{
        if(text.empty()){width=0;return true;}
        SIZE extent{};if(text.size()>INT_MAX||!GetTextExtentPoint32W(g_dc,text.data(),int(text.size()),&extent))return false;
        width=int(extent.cx);return width>0;
    };
    auto emitLine=[&](){while(!line.empty()&&iswspace(line.back()))line.pop_back();if(!line.empty()){if(!output.empty())output.push_back(L'\n');output+=line;++lineCount;}line.clear();};
    for(size_t i=0;source[i];){
        if(source[i]==L'\n'){emitLine();++i;continue;}
        size_t unit=1;
        const uint16_t first=uint16_t(source[i]);
        if(first>=0xD800&&first<=0xDBFF&&source[i+1]){const uint16_t second=uint16_t(source[i+1]);if(second>=0xDC00&&second<=0xDFFF)unit=2;}
        std::wstring glyph(source+i,source+i+unit);
        if(line.empty()&&glyph.size()==1&&iswspace(glyph[0])){i+=unit;continue;}
        std::wstring candidate=line+glyph;int extent=0;if(!measure(candidate,extent))return false;
        if(extent>widthLimit&&!line.empty()){
            const size_t space=line.find_last_of(L' ');
            if(space!=std::wstring::npos&&space>0){
                std::wstring tail=line.substr(space+1);line.resize(space);emitLine();line.swap(tail);
            }else emitLine();
            candidate=line+glyph;if(!measure(candidate,extent))return false;
        }
        if(extent>widthLimit&&!line.empty())emitLine();
        line+=glyph;
        i+=unit;
    }
    emitLine();
    if(!lineCount) return false;
    return lineCount<=32;
}

// When UTF-8 bytes are fed through the legacy bitmap builder, its quad count
// can differ from the Unicode codepoint count. In that case preserve the
// bitmap builder's *observed row boundaries* and map each boundary onto the
// nearest Unicode codepoint boundary. Unlike BuildMeasuredTtfWrap this does
// not reinterpret the engine's maxWidth (0x140) as a GDI pixel width.
bool BuildGeometryMappedTtfWrap(const wchar_t* source,void* destination,int vertexCount,
    const float* origin,const float* horizontal,const float* vertical,float scale,
    std::wstring& output,int& lineStepPixels,float& bottomOffset,unsigned& lineCount){
    output.clear();lineStepPixels=0;bottomOffset=0;lineCount=0;
    if(!source||!destination||!origin||!horizontal||!vertical||vertexCount<6||vertexCount%6||
        size_t(vertexCount)>kProvinceBufferBytes/sizeof(Vertex)||!std::isfinite(scale)||scale<=0)return false;
    const float hLen2=horizontal[0]*horizontal[0]+horizontal[1]*horizontal[1]+horizontal[2]*horizontal[2];
    const float vLen2=vertical[0]*vertical[0]+vertical[1]*vertical[1]+vertical[2]*vertical[2];
    if(hLen2<1e-6f||vLen2<1e-6f)return false;
    struct Glyph { size_t sourceIndex; size_t sourceUnits; float centerH,minH,maxH,height; };
    std::vector<Glyph> textGlyphs; textGlyphs.reserve(wcslen(source));
    for(size_t i=0;source[i];){
        size_t units=1;
        const uint16_t first=uint16_t(source[i]);
        if(first>=0xD800&&first<=0xDBFF&&source[i+1]){const uint16_t second=uint16_t(source[i+1]);if(second>=0xDC00&&second<=0xDFFF)units=2;}
        if(!(units==1&&iswspace(source[i])))textGlyphs.push_back({i,units,0,0,0,0});
        i+=units;
    }
    const size_t quadCount=size_t(vertexCount)/6;
    if(textGlyphs.empty()||quadCount<2||quadCount*4<textGlyphs.size()||textGlyphs.size()*4<quadCount)return false;
    std::vector<Glyph> quads(quadCount);
    const auto* vertices=static_cast<const Vertex*>(destination);
    std::vector<float> heights;heights.reserve(quadCount);
    for(size_t q=0;q<quadCount;++q){
        Glyph g{};g.minH=std::numeric_limits<float>::infinity();g.maxH=-g.minH;
        float minV=std::numeric_limits<float>::infinity(),maxV=-minV;
        for(size_t j=0;j<6;++j){const Vertex& p=vertices[q*6+j];const float dx=p.x-origin[0],dy=p.y-origin[1],dz=p.z-origin[2];
            const float hp=(dx*horizontal[0]+dy*horizontal[1]+dz*horizontal[2])/hLen2;
            const float vp=(dx*vertical[0]+dy*vertical[1]+dz*vertical[2])/vLen2;
            g.minH=std::min(g.minH,hp);g.maxH=std::max(g.maxH,hp);minV=std::min(minV,vp);maxV=std::max(maxV,vp);}
        g.centerH=(g.minH+g.maxH)*0.5f;g.height=maxV-minV;quads[q]=g;heights.push_back(g.height);
    }
    std::vector<float> sortedHeights=heights;std::sort(sortedHeights.begin(),sortedHeights.end());
    const float medianHeight=sortedHeights[sortedHeights.size()/2];
    const float rowThreshold=std::max(0.5f,medianHeight*1.4f);
    std::vector<size_t> quadBreaks;std::vector<float> rowGaps;
    for(size_t q=1;q<quadCount;++q){const float delta=quads[q].centerH-quads[q-1].centerH;
        if(std::fabs(delta)>rowThreshold){quadBreaks.push_back(q);rowGaps.push_back(std::fabs(delta));}}
    if(quadBreaks.size()>31||quadBreaks.size()>=textGlyphs.size())return false;
    std::vector<size_t> textBreaks;textBreaks.reserve(quadBreaks.size());
    for(size_t q:quadBreaks){
        size_t glyphBoundary=(q*textGlyphs.size()+quadCount/2)/quadCount;
        glyphBoundary=std::max<size_t>(1,std::min(glyphBoundary,textGlyphs.size()-1));
        const size_t at=textGlyphs[glyphBoundary].sourceIndex;
        if(!textBreaks.empty()&&at<=textBreaks.back())return false;
        textBreaks.push_back(at);
    }
    size_t breakCursor=0;bool pendingSpace=false;
    for(size_t i=0;source[i];){
        if(breakCursor<textBreaks.size()&&i==textBreaks[breakCursor]){
            while(!output.empty()&&iswspace(output.back()))output.pop_back();
            if(!output.empty())output.push_back(L'\n');
            pendingSpace=false;++breakCursor;
        }
        size_t units=1;const uint16_t first=uint16_t(source[i]);
        if(first>=0xD800&&first<=0xDBFF&&source[i+1]){const uint16_t second=uint16_t(source[i+1]);if(second>=0xDC00&&second<=0xDFFF)units=2;}
        if(units==1&&iswspace(source[i])){pendingSpace=true;++i;continue;}
        if(pendingSpace&&!output.empty()&&output.back()!=L'\n')output.push_back(L' ');
        pendingSpace=false;output.append(source+i,source+i+units);i+=units;
    }
    if(breakCursor!=textBreaks.size()||output.empty())return false;
    lineCount=unsigned(textBreaks.size()+1);
    const float gameScale=scale*0.05f;if(!std::isfinite(gameScale)||gameScale<=0)return false;
    if(!rowGaps.empty())lineStepPixels=std::max(1,int(std::lround((std::accumulate(rowGaps.begin(),rowGaps.end(),0.0f)/rowGaps.size())/gameScale)));
    const size_t firstRowEnd=quadBreaks.empty()?quadCount:quadBreaks.front();
    for(size_t q=0;q<firstRowEnd;++q)bottomOffset=std::min(bottomOffset==0?quads[q].minH:bottomOffset,quads[q].minH);
    if(g_api&&g_api->Log&&lineCount>1)
        (void)0;
    return lineCount<=32;
}
volatile LONG* TraceSelectedFlag(const std::string& n){ if(n=="Vienna")return &g_viennaSelectedOnce; if(n=="Resicabánya"||n=="Resicabanya")return &g_resicSelectedOnce; if(n=="Sankt Pölten")return &g_sanktTraceDrawOnce; if(n=="Székesfehérvár")return &g_szekesTraceDrawOnce; if(n=="Zalaegerszeg")return &g_zalaTraceDrawOnce; if(n=="Bansk?Bystrica")return &g_banskTraceDrawOnce; return &g_drawLogOnce; }
volatile LONG* DeviceIdentityFlag(const std::string& n){if(n=="Vienna")return &g_deviceIdentityViennaOnce;if(n=="Resicabánya"||n=="Resicabanya")return &g_deviceIdentityResicOnce;if(n=="Sankt Pölten")return &g_deviceIdentitySanktOnce;if(n=="Székesfehérvár")return &g_deviceIdentitySzekesOnce;if(n=="Zalaegerszeg")return &g_deviceIdentityZalaOnce;if(n=="Bansk?Bystrica")return &g_deviceIdentityBanskOnce;return nullptr;}
std::string ToUtf8(const wchar_t* name) { if(!name)return {}; int n=WideCharToMultiByte(CP_UTF8,0,name,-1,nullptr,0,nullptr,nullptr); if(n<=1||n>256)return {}; std::string r(size_t(n),'\0'); if(!WideCharToMultiByte(CP_UTF8,0,name,-1,r.data(),n,nullptr,nullptr))return {}; r.resize(size_t(n-1)); return r; }

void FormatBytesHex(const char* bytes, size_t length, char* output, size_t outputSize) {
    static const char digits[] = "0123456789ABCDEF";
    if (!output || !outputSize) return;
    size_t j = 0;
    for (size_t i = 0; bytes && i < length && j + 2 < outputSize; ++i) {
        const unsigned char b = static_cast<unsigned char>(bytes[i]);
        output[j++] = digits[b >> 4]; output[j++] = digits[b & 15];
    }
    output[j] = 0;
}

int CharsetProbeIndex(unsigned id){for(size_t i=0;i<_countof(kCharsetProbeProvinceIds);++i)if(kCharsetProbeProvinceIds[i]==id)return int(i);return -1;}
bool ClaimCharsetProbeTrace(unsigned id,unsigned stage){
    const int index=CharsetProbeIndex(id);if(index<0)return false;
    volatile LONG* flags=stage==0?g_charsetSlotTraceOnce:(stage==1?g_charsetBuilderTraceOnce:g_charsetVanillaTraceOnce);
    return InterlockedCompareExchange(&flags[index],1,0)==0;
}
void FormatWideCodepoints(const wchar_t* text,char* output,size_t outputSize){
    if(!output||!outputSize)return;output[0]=0;if(!text)return;size_t used=0;
    for(size_t i=0;text[i]&&used+12<outputSize;++i){uint32_t cp=uint16_t(text[i]);
        if(cp>=0xD800&&cp<=0xDBFF&&text[i+1]>=0xDC00&&text[i+1]<=0xDFFF){cp=0x10000+((cp-0xD800)<<10)+(uint16_t(text[++i])-0xDC00);}
        const int written=_snprintf_s(output+used,outputSize-used,_TRUNCATE,"U+%04X%s",cp,text[i+1]?" ":"");
        if(written<0)break;used+=size_t(written);
    }
}
void TraceCharsetProbeBytes(const char* stage,unsigned id,const void* object,const char* bytes,size_t length){
    if(!g_api||!g_api->Log||CharsetProbeIndex(id)<0)return;
    char hex[kMaxUtf8Bytes*2+1]={},utf8Codepoints[1024]="invalid",cp1250Codepoints[1024]="unavailable";
    FormatBytesHex(bytes,length,hex,sizeof(hex));
    wchar_t wide[256]={};
    const int utf8Count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes,int(length),wide,_countof(wide));
    const bool utf8Valid=utf8Count>0;if(utf8Valid){wide[utf8Count]=0;FormatWideCodepoints(wide,utf8Codepoints,sizeof(utf8Codepoints));}
    std::memset(wide,0,sizeof(wide));
    const int cp1250Count=MultiByteToWideChar(1250,0,bytes,int(length),wide,_countof(wide));
    if(cp1250Count>0){wide[cp1250Count]=0;FormatWideCodepoints(wide,cp1250Codepoints,sizeof(cp1250Codepoints));}
    bool hasUtf8N=false,hasRuntimeD01D=false;
    for(size_t i=0;i+1<length;++i){if((unsigned char)bytes[i]==0xD0&&(unsigned char)bytes[i+1]==0xBD)hasUtf8N=true;if((unsigned char)bytes[i]==0xD0&&(unsigned char)bytes[i+1]==0x1D)hasRuntimeD01D=true;}
    (void)0;
}
void TraceCharsetProbeObject(const char* stage,unsigned id,const void* object){
    char raw[kMaxUtf8Bytes+1]={};size_t length=0;
    if(!object||!SnapshotString(object,raw,sizeof(raw),&length)){
        if(g_api&&g_api->Log)(void)0;
        return;
    }
    TraceCharsetProbeBytes(stage,id,object,raw,length);
}

std::string ProvinceNameForId(unsigned id) {
    std::string result;
    AcquireSRWLockShared(&g_provinceNameLock);
    auto it = g_provinceIdNames.find(id);
    if (it != g_provinceIdNames.end()) result = it->second;
    ReleaseSRWLockShared(&g_provinceNameLock);
    return result;
}

void StoreProvinceGeometry(void* key, const std::string& name, const std::vector<uint32_t>& pixels,
    unsigned width, unsigned height, const Vertex* vertices, int vanillaVertexCount) {
    if (!key || !vertices || pixels.size() != size_t(width) * height) return;
    ProvinceDiagGeometry d{};
    d.name = name;
    std::memcpy(d.vertices, vertices, sizeof(d.vertices));
    d.width = width; d.height = height;
    d.textureWidth = PowerOfTwo(width); d.textureHeight = PowerOfTwo(height);
    d.vanillaVertexCount = vanillaVertexCount;
    d.alphaMin = 255;
    d.alphaLeft = int(width); d.alphaTop = int(height);
    uint64_t hash = 1469598103934665603ull;
    for (unsigned y = 0; y < height; ++y) {
        for (unsigned x = 0; x < width; ++x) {
            const uint32_t px = pixels[size_t(y) * width + x];
            const unsigned alpha = px >> 24;
            if (alpha) {
                ++d.alphaPixels;
                d.alphaMin = std::min(d.alphaMin, alpha);
                d.alphaMax = std::max(d.alphaMax, alpha);
                d.alphaLeft = std::min(d.alphaLeft, int(x));
                d.alphaTop = std::min(d.alphaTop, int(y));
                d.alphaRight = std::max(d.alphaRight, int(x));
                d.alphaBottom = std::max(d.alphaBottom, int(y));
            }
            for (unsigned shift : {0u, 8u, 16u, 24u}) {
                hash ^= (px >> shift) & 0xffu;
                hash *= 1099511628211ull;
            }
        }
    }
    if (!d.alphaPixels) d.alphaMin = 0;
    d.pixelHash = hash;
    bool keyReusedForDifferentName = false;
    std::string oldName;
    AcquireSRWLockExclusive(&g_provinceGeometryLock);
    try {
        auto old = g_provinceGeometry.find(key);
        if (old != g_provinceGeometry.end() && old->second.name != name) {
            keyReusedForDifferentName = true;
            oldName = old->second.name;
        }
        g_provinceGeometry[key] = d;
    } catch (...) {}
    ReleaseSRWLockExclusive(&g_provinceGeometryLock);
    if (keyReusedForDifferentName && g_api && g_api->Log)
        (void)0;
    if (g_api && g_api->Log)
        (void)0;
}

unsigned ReadProvinceId(void* renderable);

void RecordProvinceCandidate(void* label) {
    const LONG n = InterlockedIncrement(&g_slotCandidateCount);
    if (!label) return;
    const unsigned id = ReadProvinceId(label);
    char raw[kMaxUtf8Bytes + 1] = {};
    size_t length = 0;
    const bool hasName = SnapshotString(static_cast<const unsigned char*>(label) + 0x24,
        raw, sizeof(raw), &length);
    if (!hasName) {
        if (n <= 1000 && g_api && g_api->Log)
            (void)0;
        return;
    }
    // Repair runs for every province candidate, even after the verbose
    // candidate trace limit, so scrolling to later map regions is covered.
    RepairProvinceUtf8Escapes(label, id, raw, length);
    if (n > 1000 || !g_api || !g_api->Log) return;
    if(ClaimCharsetProbeTrace(id,0))TraceCharsetProbeBytes("slot-candidate",id,label,raw,length);
    std::vector<wchar_t> wide;
    UINT cp = 0;
    std::string name;
    if (DecodeProvinceName(raw, length, wide, cp)) name = ToUtf8(wide.data());
    else name.assign(raw, length);
    AcquireSRWLockExclusive(&g_provinceNameLock);
    try { g_provinceIdNames[id] = name; } catch (...) {}
    ReleaseSRWLockExclusive(&g_provinceNameLock);
    (void)0;
}

void LogBuilderFallback(const char* reason, void* fontObject, void* destination,
    const std::string* gameString, float scale, int maxWidth) {
    const LONG n = InterlockedIncrement(&g_builderFallbackCount);
    if (n > 2000 || !g_api || !g_api->Log) return;
    char raw[kMaxUtf8Bytes + 1] = {};
    char hex[kMaxUtf8Bytes * 2 + 1] = {};
    size_t length = 0;
    if (gameString && SnapshotString(gameString, raw, sizeof(raw), &length)) FormatBytesHex(raw, length, hex, sizeof(hex));
    (void)0;
}

void TraceBuilderVertexSummary(const char* name,const char* source,void* destination,int reportedCount){
    if(!name||!destination||reportedCount<=0||!g_api||!g_api->Log)return;
    Vertex first[3]{},last[3]{};uint64_t hash=1469598103934665603ull;size_t count=std::min<size_t>(size_t(reportedCount),kProvinceBufferBytes/sizeof(Vertex));
    float minX=std::numeric_limits<float>::infinity(),minY=minX,minZ=minX,maxX=-minX,maxY=-minX,maxZ=-minX;
    __try{
        const auto* bytes=static_cast<const unsigned char*>(destination);
        for(size_t i=0;i<count*sizeof(Vertex);++i){hash^=bytes[i];hash*=1099511628211ull;}
        const auto* v=static_cast<const Vertex*>(destination);
        for(size_t i=0;i<3&&i<count;++i)first[i]=v[i];
        const size_t tailStart=count>3?count-3:0;
        for(size_t i=tailStart;i<count;++i)last[i-tailStart]=v[i];
        for(size_t i=0;i<count;++i){minX=std::min(minX,v[i].x);maxX=std::max(maxX,v[i].x);minY=std::min(minY,v[i].y);maxY=std::max(maxY,v[i].y);minZ=std::min(minZ,v[i].z);maxZ=std::max(maxZ,v[i].z);}
    }__except(EXCEPTION_EXECUTE_HANDLER){(void)0;return;}
    (void)0;
}

// Dump every vanilla glyph quad for a small, explicit set of long sea labels.
// The engine's original builder has already populated destination at this point.
// Local H/V projections expose row changes independently of camera orientation.
void TraceVanillaWrapGeometry(const char* name,const wchar_t* text,void* destination,int reportedCount,
    const float* origin,const float* horizontal,const float* vertical) {
    if(!name||!text||!destination||!origin||!horizontal||!vertical||reportedCount<=0||!g_api||!g_api->Log)return;
    const size_t count=std::min<size_t>(size_t(reportedCount),kProvinceBufferBytes/sizeof(Vertex));
    const size_t quads=count/6;
    (void)0;
    const auto* vertices=static_cast<const Vertex*>(destination);
    size_t sourceIndex=0;
    for(size_t q=0;q<quads;++q){
        while(text[sourceIndex] && iswspace(text[sourceIndex]))++sourceIndex;
        const unsigned cp=text[sourceIndex]?unsigned(text[sourceIndex]):0;
        if(text[sourceIndex])++sourceIndex;
        float hMin=std::numeric_limits<float>::infinity(),hMax=-hMin,vMin=hMin,vMax=-hMin;
        float xMin=hMin,xMax=-hMin,yMin=hMin,yMax=-hMin,zMin=hMin,zMax=-hMin;
        for(size_t j=0;j<6;++j){
            const Vertex& p=vertices[q*6+j];
            const float dx=p.x-origin[0],dy=p.y-origin[1],dz=p.z-origin[2];
            const float hp=dx*horizontal[0]+dy*horizontal[1]+dz*horizontal[2];
            const float vp=dx*vertical[0]+dy*vertical[1]+dz*vertical[2];
            hMin=std::min(hMin,hp);hMax=std::max(hMax,hp);vMin=std::min(vMin,vp);vMax=std::max(vMax,vp);
            xMin=std::min(xMin,p.x);xMax=std::max(xMax,p.x);yMin=std::min(yMin,p.y);yMax=std::max(yMax,p.y);zMin=std::min(zMin,p.z);zMax=std::max(zMax,p.z);
        }
        (void)0;
    }
    (void)0;
}

void TraceSlotExhausted(void* manager,LONG inspectedSlots,void* currentCell,LONG currentEntry){
    const LONG n=InterlockedIncrement(&g_slotExhaustionCount); if(n>1000||!g_api||!g_api->Log)return;
    unsigned id=0xffffffffu;char raw[kMaxUtf8Bytes+1]={},hex[kMaxUtf8Bytes*2+1]={};size_t length=0;
    if(g_currentSlotCandidate){__try{id=*reinterpret_cast<const unsigned*>(static_cast<const unsigned char*>(g_currentSlotCandidate)+0x40);}__except(EXCEPTION_EXECUTE_HANDLER){id=0xffffffffu;}}
    const bool hasName=g_currentSlotCandidate&&SnapshotString(static_cast<const unsigned char*>(g_currentSlotCandidate)+0x24,raw,sizeof(raw),&length);
    if(hasName)FormatBytesHex(raw,length,hex,sizeof(hex));
    (void)0;
}

void CaptureSlotCandidate(void* label){g_currentSlotCandidate=label;RecordProvinceCandidate(label);}

bool ReadFontHeight(void* fontObject, int& height) {
    height = 60;
    if (!fontObject) return false;
    __try {
        const int candidate = *reinterpret_cast<const int*>(static_cast<const unsigned char*>(fontObject) + 0x494);
        if (candidate > 0 && candidate <= 256) height = candidate;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void CaptureRenderable(void* renderable) { g_currentRenderable = renderable; g_lockedProvinceData=0; g_builderDestination=0; g_provinceLockHr=E_PENDING;g_traceBufferThisLabel=false;g_traceWrapLabel=false;g_currentBuilderTraceName.clear(); }

bool InitializeFont(void* fontObject) {
    IDirect3DDevice9* device = nullptr;
    if (!g_provinceNameMapFontSettings.active || !g_provinceNameMapFontSettings.userTtfFile[0] || !GetDevice(&device)) return false;
    if (g_device && g_device != device) return false;
    if (!g_font) {
        int height = 60;
        if (!ReadFontHeight(fontObject, height)) return false;
        wchar_t modFolder[MAX_PATH] = {};
        wchar_t path[MAX_PATH] = {};
        if (!g_api || !g_api->modFolder || !MultiByteToWideChar(CP_ACP, 0, g_api->modFolder, -1, modFolder, MAX_PATH) ||
            _snwprintf_s(path, _countof(path), _TRUNCATE, L"%s\\TTF Unicode Support\\%s", modFolder,
                g_provinceNameMapFontSettings.userTtfFile) < 0)
            return false;
        HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER length = {};
        bool ok = GetFileSizeEx(file, &length) && length.QuadPart > 0 && length.QuadPart <= 16 * 1024 * 1024;
        std::vector<unsigned char> bytes(ok ? size_t(length.QuadPart) : 0);
        DWORD read = 0;
        ok = ok && ReadFile(file, bytes.data(), DWORD(bytes.size()), &read, nullptr) && read == bytes.size();
        CloseHandle(file);
        std::wstring faceName;
        if (!ok || !TtfFamilyName(bytes, faceName)) return false;
        g_provinceFontBytes.assign(bytes.begin(), bytes.end());
        DWORD count = 0;
        if (!(g_fontResource = AddFontMemResourceEx(bytes.data(), read, nullptr, &count)) || !count) return false;
        g_dc = CreateCompatibleDC(nullptr);
        if (!g_dc) return false;
        g_font = CreateFontW(-height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_TT_ONLY_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, faceName.c_str());
        if (!g_font) return false;
        g_oldFont = SelectObject(g_dc, g_font);
        if (!g_oldFont || g_oldFont == HGDI_ERROR) { g_oldFont = nullptr; return false; }
        wchar_t face[LF_FACESIZE] = {};
        if (!GetTextFaceW(g_dc, LF_FACESIZE, face) || _wcsicmp(face, faceName.c_str()) != 0) return false;
        g_device = device;
        if (g_api->Log) (void)0;
    }
    return true;
}

bool Rasterize(const wchar_t* text,int requestedLineStep,std::vector<uint32_t>& pixels,unsigned& width,unsigned& height,const char** failure,int* measuredWidth,int* measuredHeight){
    if(measuredWidth)*measuredWidth=0;if(measuredHeight)*measuredHeight=0;
    if(failure)*failure="none";if(!g_dc||!text||!*text){if(failure)*failure="no_dc_or_empty";return false;}
    const int chars=int(wcslen(text));if(chars<=0||chars>96){if(failure)*failure="char_count_limit";return false;}
    for(int i=0;i<chars;++i){
        if(text[i]==L'\n')continue;
        if(text[i]>=0xD800&&text[i]<=0xDFFF){if(failure)*failure="supplementary_scalar_not_supported_by_gdi_probe";return false;}
        if(!TtfHasMappedGlyph(g_provinceFontBytes, uint32_t(text[i]))){if(failure)*failure="glyph_missing_from_configured_ttf_cmap";return false;}
    }
    std::vector<std::wstring> lines;lines.emplace_back();for(int i=0;i<chars;++i){if(text[i]==L'\n')lines.emplace_back();else lines.back().push_back(text[i]);}
    if(lines.empty()||lines.size()>32){if(failure)*failure="line_count_limit";return false;}
    SIZE extent={};TEXTMETRICW metrics={};if(!GetTextMetricsW(g_dc,&metrics)){if(failure)*failure="GetTextMetricsW";return false;}
    if(measuredHeight)*measuredHeight=metrics.tmHeight;
    if(metrics.tmHeight<=0||metrics.tmHeight>256){if(failure)*failure="font_height_outside_1_256";return false;}
    int widest=0;for(const auto& line:lines){SIZE e={};if(!GetTextExtentPoint32W(g_dc,line.c_str(),int(line.size()),&e)){if(failure)*failure="GetTextExtentPoint32W";return false;}widest=std::max(widest,int(e.cx));}
    if(measuredWidth)*measuredWidth=widest;
    if(widest<=0||widest>kMaxTtfTextWidth){if(failure)*failure="extent_width_outside_1_1020";return false;}
    const int lineStep=lines.size()>1?std::max(int(metrics.tmHeight),requestedLineStep):int(metrics.tmHeight);
    const int imageHeight=metrics.tmHeight+int(lines.size()-1)*lineStep+4;
    width=unsigned(widest+4);height=unsigned(imageHeight);if(width<4||height<4||width>1024||height>1024){if(failure)*failure="bitmap_dimensions_limit";return false;}
    BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=LONG(width);info.bmiHeader.biHeight=-LONG(height);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    void* bits=nullptr;HBITMAP dib=CreateDIBSection(g_dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);if(!dib||!bits){if(failure)*failure="CreateDIBSection";if(dib)DeleteObject(dib);return false;}
    HGDIOBJ old=SelectObject(g_dc,dib);bool ok=old&&old!=HGDI_ERROR;if(!ok&&failure)*failure="SelectObject_DIB";
    if(ok){std::memset(bits,0,size_t(width)*height*4);SetBkMode(g_dc,TRANSPARENT);SetTextColor(g_dc,RGB(255,255,255));
        for(size_t i=0;i<lines.size();++i){RECT rect{2,LONG(2+int(i)*lineStep),LONG(width-2),LONG(2+int(i)*lineStep+metrics.tmHeight)};
            const int drawn=DrawTextW(g_dc,lines[i].c_str(),int(lines[i].size()),&rect,DT_CENTER|DT_TOP|DT_SINGLELINE|DT_NOPREFIX|DT_NOCLIP);
            if(drawn<=0){ok=false;if(failure)*failure="DrawTextW_returned_zero";break;}}
        if(ok){const auto* src=static_cast<const uint32_t*>(bits);pixels.resize(size_t(width)*height);for(size_t i=0;i<pixels.size();++i){const uint32_t c=src[i];const unsigned r=c&255,g=(c>>8)&255,b=(c>>16)&255,a=std::max(r,std::max(g,b));pixels[i]=(a<<24);}}
        SelectObject(g_dc,old);}DeleteObject(dib);return ok;
}

bool UploadTexture(IDirect3DDevice9* device, void* renderable, const std::string& text,
    const std::vector<uint32_t>& pixels, unsigned width, unsigned height, IDirect3DTexture9** out,
    const char** failure, HRESULT* failureHr) {
    if(failure)*failure="none";if(failureHr)*failureHr=S_OK;
    if (!device || !renderable || !out || pixels.size() != size_t(width) * height) {if(failure)*failure="invalid-arguments";return false;}
    // The game's sampler uses its normal map-label settings. On D3D9 devices
    // with conditional NPOT support, a tightly sized texture can be created
    // successfully yet cannot be sampled with those settings. Pad each label
    // to power-of-two dimensions and keep its UVs inside the painted region.
    const unsigned textureWidth = PowerOfTwo(width);
    const unsigned textureHeight = PowerOfTwo(height);
    *out = nullptr;
    D3DCAPS9 caps{};
    const HRESULT capsHr=device->GetDeviceCaps(&caps);
    if(FAILED(capsHr)){if(failure)*failure="GetDeviceCaps";if(failureHr)*failureHr=capsHr;return false;}
    if(textureWidth>caps.MaxTextureWidth||textureHeight>caps.MaxTextureHeight){if(failure)*failure="texture-exceeds-device-caps";if(failureHr)*failureHr=D3DERR_NOTAVAILABLE;return false;}
    AcquireSRWLockExclusive(&g_textureLock);
    auto found = g_textures.find(renderable);
    IDirect3DTexture9* texture = found == g_textures.end() ? nullptr : found->second;
    IDirect3DTexture9* created = nullptr;
    bool ok = true;
    if (texture) {
        D3DSURFACE_DESC desc = {};
        const HRESULT descHr=texture->GetLevelDesc(0,&desc);
        if (FAILED(descHr) || desc.Width != textureWidth || desc.Height != textureHeight) texture = nullptr;
    }
    if (!texture) {
        if (found == g_textures.end() && g_textures.size() >= kMaxTextures) {ok=false;if(failure)*failure="cache-capacity";}
        else {const HRESULT createHr=device->CreateTexture(textureWidth,textureHeight,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&created,nullptr);if(FAILED(createHr)){ok=false;if(failure)*failure="CreateTexture";if(failureHr)*failureHr=createHr;}}
        if(ok)texture = created;
    }
    // The cache owns one reference. Return a separate reference to the caller,
    // which releases it after the builder has finished; otherwise the cache
    // keeps a dangling pointer and camera redraws crash in GetLevelDesc.
    if (ok && !texture) {ok=false;if(failure)*failure="null-texture";}
    if (ok) {
        D3DLOCKED_RECT lock = {};
        const HRESULT lockHr=texture->LockRect(0,&lock,nullptr,0);
        if (FAILED(lockHr)) {ok=false;if(failure)*failure="LockRect";if(failureHr)*failureHr=lockHr;}
        else {
            for (unsigned y = 0; y < textureHeight; ++y) {
                auto* row = static_cast<unsigned char*>(lock.pBits) + size_t(y) * lock.Pitch;
                std::memset(row, 0, size_t(textureWidth) * sizeof(uint32_t));
                if (y < height) {
                    auto* dst = reinterpret_cast<uint32_t*>(row);
                    const uint32_t rgb = (uint32_t(g_provinceNameMapFontSettings.colorR) << 16) | (uint32_t(g_provinceNameMapFontSettings.colorG) << 8) | uint32_t(g_provinceNameMapFontSettings.colorB);
                    for (unsigned x = 0; x < width; ++x)
                        dst[x] = (pixels[size_t(y) * width + x] & 0xFF000000u) | rgb;
                }
            }
            const HRESULT unlockHr=texture->UnlockRect(0);if(FAILED(unlockHr)){ok=false;if(failure)*failure="UnlockRect";if(failureHr)*failureHr=unlockHr;}
        }
    }
    if(ok){
        D3DLOCKED_RECT verify{};const HRESULT readHr=texture->LockRect(0,&verify,nullptr,D3DLOCK_READONLY);
        if(SUCCEEDED(readHr)){
            uint64_t verifyHash=1469598103934665603ull;unsigned alphaPixels=0,paddingNonzero=0,mismatch=0;unsigned left=textureWidth,top=textureHeight,right=0,bottom=0;bool any=false;
            for(unsigned y=0;y<textureHeight;++y){const auto* row=reinterpret_cast<const uint32_t*>(static_cast<const unsigned char*>(verify.pBits)+size_t(y)*verify.Pitch);for(unsigned x=0;x<textureWidth;++x){const uint32_t px=row[x];for(unsigned shift:{0u,8u,16u,24u}){verifyHash^=(px>>shift)&0xffu;verifyHash*=1099511628211ull;}const unsigned a=px>>24;if(a){++alphaPixels;if(x>=width||y>=height)++paddingNonzero;if(!any){left=x;right=x;top=y;bottom=y;any=true;}else{left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}}if(x<width&&y<height&&px!=((pixels[size_t(y)*width+x]&0xFF000000u)|(uint32_t(g_provinceNameMapFontSettings.colorR)<<16)|(uint32_t(g_provinceNameMapFontSettings.colorG)<<8)|uint32_t(g_provinceNameMapFontSettings.colorB)))++mismatch;}}
            const HRESULT unlockVerifyHr=texture->UnlockRect(0);
            if(g_api&&g_api->Log)(void)0;
        }else if(g_api&&g_api->Log)(void)0;
    }
    if (ok && created) {
        try {
            if (found != g_textures.end()) {
                IDirect3DTexture9* previous = found->second;
                found->second = created;
                if (previous) previous->Release();
                created = nullptr;
            } else {
                g_textures.emplace(renderable, created);
                created = nullptr;
            }
        } catch (...) { ok = false;if(failure)*failure="cache-allocation"; }
    }
    if (!ok && created) created->Release();
    if (ok) {
        texture->AddRef();
        *out = texture;
    }
    ReleaseSRWLockExclusive(&g_textureLock);
    return ok;
}

void RemoveTexture(void* renderable) {
    if (!renderable) return;
    AcquireSRWLockExclusive(&g_textureLock);
    auto it = g_textures.find(renderable);
    if (it != g_textures.end()) {
        if (it->second) it->second->Release();
        g_textures.erase(it);
    }
    ReleaseSRWLockExclusive(&g_textureLock);
    AcquireSRWLockExclusive(&g_provinceGeometryLock);
    g_provinceGeometry.erase(renderable);
    ReleaseSRWLockExclusive(&g_provinceGeometryLock);
    AcquireSRWLockExclusive(&g_traceNameLock);
    g_traceNames.erase(renderable);
    ReleaseSRWLockExclusive(&g_traceNameLock);
}

bool FindTexture(void* renderable, IDirect3DTexture9** out) {
    if (!renderable || !out) return false;
    *out = nullptr;
    AcquireSRWLockShared(&g_textureLock);
    auto it = g_textures.find(renderable);
    if (it != g_textures.end() && it->second) { *out = it->second; (*out)->AddRef(); }
    ReleaseSRWLockShared(&g_textureLock);
    return *out != nullptr;
}

int __stdcall HookProvinceBuilder(void* fontObject, void* destination, const std::string* gameString,
    const float* origin, const float* horizontal, const float* vertical, float scale, int maxWidth) {
    g_builderDestination=reinterpret_cast<uintptr_t>(destination);g_traceBufferThisLabel=false;g_traceWrapLabel=false;
    auto original = reinterpret_cast<BuilderFn>(g_builderOriginal);
    bool vanillaSeeded=false;int vanillaVertices=0;
    if (!original || !destination || !gameString || !origin || !horizontal || !vertical || !g_currentRenderable ||
        maxWidth <= 0 || maxWidth > 320 || !InitializeFont(fontObject)) {
        LogBuilderFallback("precheck-or-font-init", fontObject, destination, gameString, scale, maxWidth);
        RemoveTexture(g_currentRenderable);
        g_currentRenderable = nullptr;
        return original ? original(fontObject, destination, gameString, origin, horizontal, vertical, scale, maxWidth) : 0;
    }

    try {
    char utf8[kMaxUtf8Bytes + 1] = {};
    size_t byteLength = 0;
    const char* fallbackReason = "snapshot-or-length";
    if (!SnapshotString(gameString, utf8, sizeof(utf8), &byteLength) || byteLength > kMaxUtf8Bytes) { fallbackReason = "snapshot-or-length"; goto fallback; }
    {
        const unsigned slotCandidateId=ReadProvinceId(g_currentSlotCandidate);
        const unsigned renderableId=ReadProvinceId(g_currentRenderable);
        const unsigned provinceId=slotCandidateId!=0xffffffffu?slotCandidateId:renderableId;
        const unsigned probeId=CharsetProbeIndex(slotCandidateId)>=0?slotCandidateId:
            (CharsetProbeIndex(renderableId)>=0?renderableId:0xffffffffu);
        if(CharsetProbeIndex(probeId)>=0&&ClaimCharsetProbeTrace(probeId,1)){
            TraceCharsetProbeBytes("builder-input",probeId,gameString,utf8,byteLength);
            if(g_api&&g_api->Log)(void)0;
        }
        std::vector<wchar_t> wide;
        UINT sourceCodePage = 0;
        if (!DecodeProvinceName(utf8, byteLength, wide, sourceCodePage)) {
            if (InterlockedCompareExchange(&g_utf8LogOnce, 1, 0) == 0 && g_api && g_api->Log)
                (void)0;
            fallbackReason = "decode"; goto fallback;
        }
        if (sourceCodePage != CP_UTF8 && InterlockedCompareExchange(&g_utf8LogOnce, 1, 0) == 0 && g_api && g_api->Log)
            (void)0;
        const bool traceProvince=IsTraceProvince(wide.data()); const bool traceWrap=IsWrapTraceProvince(wide.data()); const std::string traceName=ToUtf8(wide.data());
        g_traceBufferThisLabel=traceProvince||traceWrap;g_traceWrapLabel=traceWrap;g_currentBuilderTraceName=traceName;
        if(traceProvince&&g_api&&g_api->Log)(void)0;
        if (g_currentRenderable) {
            std::string oldName;
            AcquireSRWLockExclusive(&g_traceNameLock);
            try { auto old=g_traceNames.find(g_currentRenderable); if(old!=g_traceNames.end()) oldName=old->second; g_traceNames[g_currentRenderable]=traceName; }
            catch (...) {}
            ReleaseSRWLockExclusive(&g_traceNameLock);
            if (!oldName.empty() && oldName != traceName && g_api && g_api->Log)
                (void)0;
        }
        if(traceProvince&&g_api&&g_api->Log)(void)0;
        vanillaVertices=original(fontObject,destination,gameString,origin,horizontal,vertical,scale,maxWidth);vanillaSeeded=true;
        if(CharsetProbeIndex(probeId)>=0&&ClaimCharsetProbeTrace(probeId,2)&&g_api&&g_api->Log)
            (void)0;
        if(traceProvince)TraceBuilderVertexSummary(traceName.c_str(),"vanilla-seed-before-ttf-overwrite",destination,vanillaVertices);
        if(traceWrap&&ClaimWrapTrace(wide.data()))TraceVanillaWrapGeometry(traceName.c_str(),wide.data(),destination,vanillaVertices,origin,horizontal,vertical);
        if(vanillaVertices<6||size_t(vanillaVertices)>kProvinceBufferBytes/sizeof(Vertex)||vanillaVertices%6){
            if(g_api&&g_api->Log)(void)0;
            RemoveTexture(g_currentRenderable);g_currentRenderable=nullptr;return vanillaVertices;
        }
        std::wstring renderText;int lineStepPixels=0;float bottomOffset=0;unsigned vanillaLines=0;
        const bool vanillaLayoutMapped=BuildVanillaWrappedText(wide.data(),destination,vanillaVertices,origin,horizontal,vertical,scale,renderText,lineStepPixels,bottomOffset,vanillaLines);
        if(!vanillaLayoutMapped){
            if(!BuildGeometryMappedTtfWrap(wide.data(),destination,vanillaVertices,origin,horizontal,vertical,scale,renderText,lineStepPixels,bottomOffset,vanillaLines)){
                if(g_api&&g_api->Log)(void)0;
                renderText=wide.data();lineStepPixels=0;bottomOffset=0;vanillaLines=1;
            }
            const LONG wrapTrace=InterlockedIncrement(&g_independentWrapLogCount);
            if(wrapTrace<=256&&g_api&&g_api->Log)
                (void)0;
        }
        if(vanillaLines>1&&g_api&&g_api->Log)(void)0;
        std::vector<uint32_t> pixels; unsigned width=0,height=0;
        int measuredWidth=0,measuredHeight=0;const char* rasterFailure=nullptr;
        if(!Rasterize(renderText.c_str(),lineStepPixels,pixels,width,height,&rasterFailure,&measuredWidth,&measuredHeight)){
            const char* initialRasterFailure=rasterFailure?rasterFailure:"rasterize";
            bool fittedToAtlas=false;
            if(std::strcmp(initialRasterFailure,"extent_width_outside_1_1020")==0){
                const int initialMeasuredWidth=measuredWidth;
                std::wstring fittedText;int fittedLineStep=0;float ignoredBottom=0;unsigned fittedLines=0;
                if(BuildMeasuredTtfWrap(renderText.c_str(),kMaxTtfTextWidth-8,fittedText,fittedLineStep,ignoredBottom,fittedLines)&&
                    fittedText!=renderText&&Rasterize(fittedText.c_str(),std::max(lineStepPixels,fittedLineStep),pixels,width,height,&rasterFailure,&measuredWidth,&measuredHeight)){
                    if(g_api&&g_api->Log)(void)0;
                    renderText.swap(fittedText);vanillaLines=fittedLines;lineStepPixels=std::max(lineStepPixels,fittedLineStep);fittedToAtlas=true;
                }
            }
            if(!fittedToAtlas){
                if(traceProvince&&g_api&&g_api->Log)(void)0;
                fallbackReason=rasterFailure?rasterFailure:initialRasterFailure;goto fallback;
            }
        }
        if(traceProvince&&g_api&&g_api->Log)(void)0;
        if(traceProvince&&g_api&&g_api->Log){unsigned nonzero=0,amin=255,amax=0;unsigned long long asum=0;for(uint32_t px:pixels){const unsigned a=px>>24;if(a){++nonzero;amin=std::min(amin,a);amax=std::max(amax,a);asum+=a;}}(void)0;}

        std::vector<Vertex> vertices(6);
        const float u0 = 0.0f, u1 = float(width) / float(PowerOfTwo(width));
        const float v0 = 0.0f, v1 = float(height) / float(PowerOfTwo(height));
        // Vanilla centers X on the label anchor and places the glyphs above
        // its baseline (positive local Y). Mirror that geometry here.
        const float x0 = -float(width) * 0.5f, x1 = float(width) * 0.5f;
        const float y0 = vanillaLines>1?bottomOffset:0.0f, y1 = y0+float(height);
        const float coordinates[6][4] = {{x0,y0,u0,v1},{x1,y0,u1,v1},{x0,y1,u0,v0},
            {x1,y1,u1,v0},{x1,y0,u1,v1},{x0,y1,u0,v0}};
        const float gameScale = scale * 0.05f;
        if (!std::isfinite(gameScale) || gameScale <= 0) { fallbackReason="invalid-scale"; goto fallback; }
        for (int i = 0; i < 6; ++i) {
            const float x = coordinates[i][0] * gameScale, y = coordinates[i][1] * gameScale;
            // Vanilla FUN_00A26D00 uses its sixth argument for glyph X and
            // its fifth argument for glyph Y. Keep that winding: reversing
            // the axes makes the quad back-facing and it gets culled.
            vertices[i] = {origin[0] + vertical[0]*x + horizontal[0]*y,
                origin[1] + vertical[1]*x + horizontal[1]*y,
                origin[2] + vertical[2]*x + horizontal[2]*y,
                coordinates[i][2], coordinates[i][3]};
            if (!std::isfinite(vertices[i].x) || !std::isfinite(vertices[i].y) || !std::isfinite(vertices[i].z)) { fallbackReason="nonfinite-vertex"; goto fallback; }
        }
        if(traceProvince&&g_api&&g_api->Log)(void)0;
        if(traceProvince&&g_api&&g_api->Log)(void)0;
        IDirect3DDevice9* device = nullptr;
        IDirect3DTexture9* texture = nullptr;
        const bool deviceReady=GetDevice(&device)&&device==g_device;
        const char* uploadFailure=deviceReady?"unknown":"device-unavailable";HRESULT uploadHr=E_FAIL;
        const bool textureUploaded=deviceReady&&UploadTexture(device,g_currentRenderable,std::string(utf8,byteLength),pixels,width,height,&texture,&uploadFailure,&uploadHr);
        if(traceProvince&&!textureUploaded&&g_api&&g_api->Log)(void)0;
        if(traceProvince){AcquireSRWLockExclusive(&g_traceNameLock);try{g_traceNames[g_currentRenderable]=traceName;}catch(...){}ReleaseSRWLockExclusive(&g_traceNameLock);
            if(g_api&&g_api->Log)(void)0;}
        if(!textureUploaded){fallbackReason=deviceReady?"texture-upload":"device-unavailable";goto fallback;}
        texture->Release(); // cache owns one reference
        if(traceProvince&&g_api&&g_api->Log&&vanillaVertices>=3){const auto* seed=static_cast<const Vertex*>(destination);(void)0;}
        if(traceProvince&&g_api&&g_api->Log&&vanillaVertices>=6){const auto* seed=static_cast<const Vertex*>(destination);const int b=vanillaVertices-3;(void)0;}
        if(vanillaVertices<6||size_t(vanillaVertices)>kProvinceBufferBytes/sizeof(Vertex)){
            if(g_api&&g_api->Log)(void)0;
            RemoveTexture(g_currentRenderable);g_currentRenderable=nullptr;return vanillaVertices;
        }
        static_assert(sizeof(Vertex)*6<=kProvinceBufferBytes,"province quad exceeds buffer");
        auto* target=static_cast<unsigned char*>(destination);
        // vertices is a std::vector, so sizeof(vertices) is the container
        // object's size rather than the payload size. Copy and verify all six
        // Vertex records explicitly (120 bytes).
        const size_t geometryBytes=sizeof(Vertex)*vertices.size();
        std::memset(target,0,kProvinceBufferBytes);
        std::memcpy(target,vertices.data(),geometryBytes);
        uint64_t bufferHash=1469598103934665603ull;size_t tailNonzero=0;
        for(size_t bi=0;bi<kProvinceBufferBytes;++bi){bufferHash^=target[bi];bufferHash*=1099511628211ull;if(bi>=geometryBytes&&target[bi])++tailNonzero;}
        const bool vertexBytesMatch=std::memcmp(target,vertices.data(),geometryBytes)==0;
        if(g_api&&g_api->Log)(void)0;
        if(g_api&&g_api->Log)(void)0;
        StoreProvinceGeometry(g_currentRenderable,traceName,pixels,width,height,vertices.data(),vanillaVertices);
        if(traceProvince&&g_api&&g_api->Log)(void)0;
        if(InterlockedCompareExchange(&g_builderLogOnce,1,0)==0&&g_api&&g_api->Log)
            (void)0;
        g_currentRenderable=nullptr;
        return vanillaVertices;
    }
fallback:
    LogBuilderFallback(fallbackReason, fontObject, destination, gameString, scale, maxWidth);
    if (InterlockedCompareExchange(&g_fallbackLogOnce, 1, 0) == 0 && g_api && g_api->Log)
        (void)0;
    RemoveTexture(g_currentRenderable);
    g_currentRenderable = nullptr;
    if(vanillaSeeded)return vanillaVertices;
    return original(fontObject, destination, gameString, origin, horizontal, vertical, scale, maxWidth);
    } catch (...) {
        RemoveTexture(g_currentRenderable);
        g_currentRenderable = nullptr;
        if (InterlockedCompareExchange(&g_fallbackLogOnce, 1, 0) == 0 && g_api && g_api->Log)
            (void)0;
        if(vanillaSeeded)return vanillaVertices;
        return original(fontObject, destination, gameString, origin, horizontal, vertical, scale, maxWidth);
    }
}

extern "C" void __cdecl BeforeProvinceDraw(void* renderable, IDirect3DDevice9* drawDevice);
extern "C" void __cdecl AfterProvinceDraw();
IDirect3DBaseTexture9* g_previousTexture;
IDirect3DDevice9* g_restoreDevice;

bool SnapshotProvinceDrawPacket(void* object,uint32_t fields[9]) { if(!object||!fields)return false; __try { const auto* p=static_cast<const uint32_t*>(object); for(unsigned i=0;i<9;++i)fields[i]=p[i]; return true; } __except(EXCEPTION_EXECUTE_HANDLER) { return false; } }

void* ResolveProvinceTextureKey(void* renderable) { if(!renderable)return nullptr; __try { return *reinterpret_cast<void**>(renderable); } __except(EXCEPTION_EXECUTE_HANDLER) { return nullptr; } }

unsigned ReadProvinceId(void* renderable) {
    if (!renderable) return 0xffffffffu;
    __try { return *reinterpret_cast<const uint32_t*>(static_cast<const unsigned char*>(renderable) + 0x40); }
    __except (EXCEPTION_EXECUTE_HANDLER) { return 0xffffffffu; }
}

bool DiagSignatureChanged(void* handle, uint64_t signature) {
    bool changed = false;
    AcquireSRWLockExclusive(&g_diagDrawLock);
    try { auto it=g_diagDrawSignatures.find(handle); changed=it==g_diagDrawSignatures.end()||it->second!=signature; if(changed)g_diagDrawSignatures[handle]=signature; }
    catch (...) { changed=true; }
    ReleaseSRWLockExclusive(&g_diagDrawLock);
    return changed;
}

bool DiagRouteSignatureChanged(void* handle,uint64_t signature){bool changed=false;AcquireSRWLockExclusive(&g_diagRouteLock);try{auto it=g_diagRouteSignatures.find(handle);changed=it==g_diagRouteSignatures.end()||it->second!=signature;if(changed)g_diagRouteSignatures[handle]=signature;}catch(...){changed=true;}ReleaseSRWLockExclusive(&g_diagRouteLock);return changed;}

void TraceProvinceDrawRoute(unsigned id, const std::string& name, void* handle, void* textureKey,
    bool textureFound, bool deviceReady, IDirect3DBaseTexture9* previous, IDirect3DTexture9* replacement) {
    if (!g_api || !g_api->Log) return;
    uint64_t sig=uint64_t(id) ^ (uint64_t(reinterpret_cast<uintptr_t>(textureKey))<<1) ^
        (uint64_t(reinterpret_cast<uintptr_t>(replacement))*0x9e3779b97f4a7c15ull) ^
        (uint64_t(reinterpret_cast<uintptr_t>(previous))*0xbf58476d1ce4e5b9ull) ^
        (uint64_t(textureFound?1:0)<<61) ^ (uint64_t(deviceReady?1:0)<<62);
    if(!DiagRouteSignatureChanged(handle,sig))return;
    uint32_t packet[9]{}; const bool ok=SnapshotProvinceDrawPacket(handle,packet);
    (void)0;
}

HRESULT CaptureRenderTarget(IDirect3DDevice9* device, ProvinceFrameSnapshot& out,
    HRESULT* getTargetHr, HRESULT* descHrOut, HRESULT* resolveHrOut, HRESULT* createHrOut,
    HRESULT* readHrOut, HRESULT* lockHrOut) {
    if(getTargetHr)*getTargetHr=E_FAIL;if(descHrOut)*descHrOut=E_FAIL;
    if(resolveHrOut)*resolveHrOut=S_OK;if(createHrOut)*createHrOut=E_FAIL;
    if(readHrOut)*readHrOut=E_FAIL;if(lockHrOut)*lockHrOut=E_FAIL;
    if(!device)return E_POINTER;
    IDirect3DSurface9* source=nullptr;IDirect3DSurface9* resolved=nullptr;IDirect3DSurface9* system=nullptr;
    HRESULT hr=device->GetRenderTarget(0,&source);if(getTargetHr)*getTargetHr=hr;if(FAILED(hr)||!source)return hr;
    D3DSURFACE_DESC desc{};hr=source->GetDesc(&desc);if(descHrOut)*descHrOut=hr;if(FAILED(hr)){source->Release();return hr;}
    if(desc.Format!=D3DFMT_A8R8G8B8&&desc.Format!=D3DFMT_X8R8G8B8&&desc.Format!=D3DFMT_A2R10G10B10){source->Release();return D3DERR_NOTAVAILABLE;}
    IDirect3DSurface9* readSource=source;
    if(desc.MultiSampleType!=D3DMULTISAMPLE_NONE){
        hr=device->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&resolved,nullptr);
        if(FAILED(hr)||!resolved){if(resolveHrOut)*resolveHrOut=hr;source->Release();return hr;}
        hr=device->StretchRect(source,nullptr,resolved,nullptr,D3DTEXF_NONE);if(resolveHrOut)*resolveHrOut=hr;
        if(FAILED(hr)){resolved->Release();source->Release();return hr;}readSource=resolved;
    }
    hr=device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&system,nullptr);
    if(createHrOut)*createHrOut=hr;
    if(FAILED(hr)||!system){if(resolved)resolved->Release();source->Release();return hr;}
    hr=device->GetRenderTargetData(readSource,system);if(readHrOut)*readHrOut=hr;
    if(SUCCEEDED(hr)){
        D3DLOCKED_RECT lock{};hr=system->LockRect(&lock,nullptr,D3DLOCK_READONLY);if(lockHrOut)*lockHrOut=hr;
        if(SUCCEEDED(hr)&&lock.pBits&&lock.Pitch>0){
            try{out.width=desc.Width;out.height=desc.Height;out.format=desc.Format;out.pixels.resize(size_t(desc.Width)*desc.Height);
                for(UINT y=0;y<desc.Height;++y)std::memcpy(out.pixels.data()+size_t(y)*desc.Width,static_cast<const unsigned char*>(lock.pBits)+size_t(y)*lock.Pitch,size_t(desc.Width)*4);
            }catch(...){hr=E_OUTOFMEMORY;out.pixels.clear();}
            const HRESULT unlockHr=system->UnlockRect();if(FAILED(unlockHr))hr=unlockHr;
        }
    }
    system->Release();if(resolved)resolved->Release();source->Release();return hr;
}

void LogFrameDifference(const std::string& name,const ProvinceFrameSnapshot& before,
    const ProvinceFrameSnapshot& after,HRESULT captureHr) {
    if(!g_api||!g_api->Log)return;
    if(FAILED(captureHr)||before.pixels.empty()||after.pixels.empty()||before.width!=after.width||before.height!=after.height||before.format!=after.format){
        (void)0;return;
    }
    size_t changed=0;UINT left=before.width,top=before.height,right=0,bottom=0;uint64_t hash=1469598103934665603ull;
    const uint32_t compareMask=before.format==D3DFMT_X8R8G8B8?0x00ffffffu:0xffffffffu;
    for(UINT y=0;y<before.height;++y)for(UINT x=0;x<before.width;++x){const size_t i=size_t(y)*before.width+x;const uint32_t a=before.pixels[i],b=after.pixels[i];if((a&compareMask)!=(b&compareMask)){++changed;left=std::min(left,x);right=std::max(right,x);top=std::min(top,y);bottom=std::max(bottom,y);}hash^=b&compareMask;hash*=1099511628211ull;}
    if(changed)(void)0;
    else (void)0;
}

bool IsFrameDiagnosticLabel(const std::string& name){return name=="Vienna"||name=="Resicabánya"||name=="Resicabanya"||name=="Sankt Pölten"||name=="Székesfehérvár"||name=="Zalaegerszeg"||name=="Bansk?Bystrica";}

bool TryArmFrameDiagnostic(const std::string& name){volatile LONG* flag=nullptr;if(name=="Vienna")flag=&g_frameViennaOnce;else if(name=="Resicabánya"||name=="Resicabanya")flag=&g_frameResicOnce;else if(name=="Sankt Pölten")flag=&g_frameSanktOnce;else if(name=="Székesfehérvár")flag=&g_frameSzekesOnce;else if(name=="Zalaegerszeg")flag=&g_frameZalaOnce;else if(name=="Bansk?Bystrica")flag=&g_frameBanskOnce;return flag&&InterlockedCompareExchange(flag,1,0)==0;}

void TraceActualVertexBuffer(const std::string& name,IDirect3DDevice9* device,void* expectedKey){
    if(!device||!g_api||!g_api->Log)return;
    IDirect3DVertexBuffer9* stream=nullptr;UINT offset=0,stride=0;
    const HRESULT streamHr=device->GetStreamSource(0,&stream,&offset,&stride);
    if(FAILED(streamHr)||!stream){(void)0;return;}
    D3DVERTEXBUFFER_DESC desc{};const HRESULT descHr=stream->GetDesc(&desc);
    ProvinceDiagGeometry expected{};bool known=false;
    AcquireSRWLockShared(&g_provinceGeometryLock);auto it=g_provinceGeometry.find(expectedKey);if(it!=g_provinceGeometry.end()){expected=it->second;known=true;}ReleaseSRWLockShared(&g_provinceGeometryLock);
    // Do not Lock the game's D3DUSAGE_WRITEONLY dynamic buffer. Its bytes are
    // captured from the original CPU Lock pointer before the matching Unlock.
    (void)0;
    stream->Release();
}

void TraceTexturePipeline(const std::string& name,IDirect3DDevice9* device){
    if(!device||!g_api||!g_api->Log)return;
    for(DWORD s=0;s<8;++s){
        DWORD colorOp=0,colorArg1=0,colorArg2=0,alphaOp=0,alphaArg1=0,alphaArg2=0,coord=0,transform=0;
        DWORD minF=0,magF=0,mipF=0,u=0,v=0,srgb=0;
        const HRESULT co=device->GetTextureStageState(s,D3DTSS_COLOROP,&colorOp),
            ca1=device->GetTextureStageState(s,D3DTSS_COLORARG1,&colorArg1),ca2=device->GetTextureStageState(s,D3DTSS_COLORARG2,&colorArg2),
            ao=device->GetTextureStageState(s,D3DTSS_ALPHAOP,&alphaOp),aa1=device->GetTextureStageState(s,D3DTSS_ALPHAARG1,&alphaArg1),aa2=device->GetTextureStageState(s,D3DTSS_ALPHAARG2,&alphaArg2),
            tc=device->GetTextureStageState(s,D3DTSS_TEXCOORDINDEX,&coord),tf=device->GetTextureStageState(s,D3DTSS_TEXTURETRANSFORMFLAGS,&transform),
            mf=device->GetSamplerState(s,D3DSAMP_MINFILTER,&minF),gf=device->GetSamplerState(s,D3DSAMP_MAGFILTER,&magF),pf=device->GetSamplerState(s,D3DSAMP_MIPFILTER,&mipF),
            au=device->GetSamplerState(s,D3DSAMP_ADDRESSU,&u),av=device->GetSamplerState(s,D3DSAMP_ADDRESSV,&v),st=device->GetSamplerState(s,D3DSAMP_SRGBTEXTURE,&srgb);
        IDirect3DBaseTexture9* tex=nullptr;const HRESULT th=device->GetTexture(s,&tex);
        (void)0;
        if(tex)tex->Release();
    }
}

void BeginProvinceOcclusionQuery(IDirect3DDevice9* device,const std::string& name,bool ttfPath){
    g_frameOcclusionStarted=false;
    if(!device||!g_frameDiagArmed)return;
    if(g_frameOcclusionQuery){if(g_api&&g_api->Log)(void)0;return;}
    g_frameOcclusionEnded=false;
    HRESULT createHr=device->CreateQuery(D3DQUERYTYPE_OCCLUSION,&g_frameOcclusionQuery);
    HRESULT beginHr=FAILED(createHr)||!g_frameOcclusionQuery?E_FAIL:g_frameOcclusionQuery->Issue(D3DISSUE_BEGIN);
    g_frameOcclusionStarted=SUCCEEDED(beginHr);
    if(g_frameOcclusionStarted){g_frameOcclusionName=name;g_frameOcclusionTtfPath=ttfPath;g_frameOcclusionStartTick=GetTickCount64();g_frameOcclusionPolls=0;}
    else if(g_frameOcclusionQuery){g_frameOcclusionQuery->Release();g_frameOcclusionQuery=nullptr;}
    if(g_api&&g_api->Log)(void)0;
}

void PollProvinceOcclusionQuery(bool flush){
    if(!g_frameOcclusionQuery||!g_frameOcclusionEnded)return;
    DWORD samples=0;const DWORD flags=flush?D3DGETDATA_FLUSH:0;
    const HRESULT hr=g_frameOcclusionQuery->GetData(&samples,sizeof(samples),flags);++g_frameOcclusionPolls;
    if(hr==S_OK){if(g_api&&g_api->Log)(void)0;g_frameOcclusionQuery->Release();g_frameOcclusionQuery=nullptr;g_frameOcclusionEnded=false;g_frameOcclusionName.clear();return;}
    if(FAILED(hr)){if(g_api&&g_api->Log)(void)0;g_frameOcclusionQuery->Release();g_frameOcclusionQuery=nullptr;g_frameOcclusionEnded=false;g_frameOcclusionName.clear();return;}
    if(GetTickCount64()-g_frameOcclusionStartTick>=5000){if(g_api&&g_api->Log)(void)0;g_frameOcclusionQuery->Release();g_frameOcclusionQuery=nullptr;g_frameOcclusionEnded=false;g_frameOcclusionName.clear();}
}

void EndProvinceOcclusionQuery(const std::string& name){
    if(!g_frameOcclusionQuery||!g_frameOcclusionStarted)return;
    const HRESULT endHr=g_frameOcclusionQuery->Issue(D3DISSUE_END);g_frameOcclusionEnded=SUCCEEDED(endHr);g_frameOcclusionStarted=false;
    if(g_api&&g_api->Log)(void)0;
    if(g_frameOcclusionEnded)PollProvinceOcclusionQuery(true);
    else{g_frameOcclusionQuery->Release();g_frameOcclusionQuery=nullptr;g_frameOcclusionName.clear();}
}

void TransformPoint(const float in[4], const D3DMATRIX& m, float out[4]) {
    for (int c=0;c<4;++c) out[c]=in[0]*m.m[0][c]+in[1]*m.m[1][c]+in[2]*m.m[2][c]+in[3]*m.m[3][c];
}

void TraceD3DProjection(const std::string& name, void* handle, void* textureKey,
    IDirect3DDevice9* device, IDirect3DTexture9* expected) {
    if (!device || name.empty() || !g_api || !g_api->Log) return;
    ProvinceDiagGeometry d{};
    bool haveGeometry=false;
    AcquireSRWLockShared(&g_provinceGeometryLock);
    auto it=g_provinceGeometry.find(textureKey);
    if(it!=g_provinceGeometry.end()){d=it->second;haveGeometry=true;}
    ReleaseSRWLockShared(&g_provinceGeometryLock);
    D3DMATRIX world{},view{},projection{};
    const HRESULT worldHr=device->GetTransform(D3DTS_WORLD,&world);
    const HRESULT viewHr=device->GetTransform(D3DTS_VIEW,&view);
    const HRESULT projHr=device->GetTransform(D3DTS_PROJECTION,&projection);
    D3DVIEWPORT9 vp{};
    const HRESULT viewportHr=device->GetViewport(&vp);
    IDirect3DVertexBuffer9* stream=nullptr;
    UINT streamOffset=0,stride=0;
    const HRESULT streamHr=device->GetStreamSource(0,&stream,&streamOffset,&stride);
    UINT streamFreq[4]{};HRESULT streamFreqHr[4]{};
    for(UINT s=0;s<4;++s)streamFreqHr[s]=device->GetStreamSourceFreq(s,&streamFreq[s]);
    IDirect3DIndexBuffer9* indexBuffer=nullptr;
    const HRESULT indexHr=device->GetIndices(&indexBuffer);
    DWORD fvf=0;
    const HRESULT fvfHr=device->GetFVF(&fvf);
    IDirect3DVertexDeclaration9* declaration=nullptr;
    const HRESULT declHr=device->GetVertexDeclaration(&declaration);
    IDirect3DVertexShader9* vertexShader=nullptr; IDirect3DPixelShader9* pixelShader=nullptr;
    const HRESULT vertexShaderHr=device->GetVertexShader(&vertexShader); const HRESULT pixelShaderHr=device->GetPixelShader(&pixelShader);
    DWORD cull=0,zEnable=0,zWrite=0,blend=0,srcBlend=0,dstBlend=0,alphaTest=0,alphaFunc=0,alphaRef=0,factor=0;
    DWORD colorOp=0,colorArg1=0,colorArg2=0,alphaOp=0,alphaArg1=0,alphaArg2=0;
    device->GetRenderState(D3DRS_CULLMODE,&cull); device->GetRenderState(D3DRS_ZENABLE,&zEnable);
    device->GetRenderState(D3DRS_ZWRITEENABLE,&zWrite); device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend);
    device->GetRenderState(D3DRS_SRCBLEND,&srcBlend); device->GetRenderState(D3DRS_DESTBLEND,&dstBlend);
    device->GetRenderState(D3DRS_ALPHATESTENABLE,&alphaTest); device->GetRenderState(D3DRS_ALPHAFUNC,&alphaFunc);
    device->GetRenderState(D3DRS_ALPHAREF,&alphaRef); device->GetRenderState(D3DRS_TEXTUREFACTOR,&factor);
    device->GetTextureStageState(0,D3DTSS_COLOROP,&colorOp); device->GetTextureStageState(0,D3DTSS_COLORARG1,&colorArg1);
    device->GetTextureStageState(0,D3DTSS_COLORARG2,&colorArg2); device->GetTextureStageState(0,D3DTSS_ALPHAOP,&alphaOp);
    device->GetTextureStageState(0,D3DTSS_ALPHAARG1,&alphaArg1); device->GetTextureStageState(0,D3DTSS_ALPHAARG2,&alphaArg2);
    DWORD scissorEnable=0,colorWrite=0,fogEnable=0;device->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissorEnable);device->GetRenderState(D3DRS_COLORWRITEENABLE,&colorWrite);device->GetRenderState(D3DRS_FOGENABLE,&fogEnable);
    RECT scissor{};const HRESULT scissorHr=device->GetScissorRect(&scissor);
    IDirect3DBaseTexture9* actual=nullptr;
    const HRESULT textureHr=device->GetTexture(0,&actual);
    const bool textureMatch=actual==expected;
    D3DSURFACE_DESC desc{};
    HRESULT descHr=expected?expected->GetLevelDesc(0,&desc):E_POINTER;
    uint64_t sig=uint64_t(reinterpret_cast<uintptr_t>(stream)) ^ (uint64_t(stride)<<32) ^
        (uint64_t(reinterpret_cast<uintptr_t>(actual))*0x9e3779b97f4a7c15ull) ^
        (uint64_t(uint32_t(worldHr))<<7) ^ (uint64_t(uint32_t(viewHr))<<13) ^
        (uint64_t(uint32_t(projHr))<<19) ^ (uint64_t(uint32_t(viewportHr))<<23) ^
        (uint64_t(vp.X)<<3) ^ (uint64_t(vp.Y)<<17) ^ (uint64_t(vp.Width)<<29) ^
        (uint64_t(vp.Height)<<41) ^ (uint64_t(fvf)<<11) ^ (uint64_t(reinterpret_cast<uintptr_t>(vertexShader))<<5) ^ (uint64_t(reinterpret_cast<uintptr_t>(pixelShader))<<17) ^ (uint64_t(scissorEnable)<<57);
    if (haveGeometry) for (const Vertex& v:d.vertices) {
        uint32_t q[3]; std::memcpy(&q[0],&v.x,4); std::memcpy(&q[1],&v.y,4); std::memcpy(&q[2],&v.z,4);
        sig^=(uint64_t(q[0])<<1)^(uint64_t(q[1])<<21)^(uint64_t(q[2])<<43);
    }
    const bool changed=DiagSignatureChanged(handle,sig);
    if (changed) {
        const LONG projectionEvent=InterlockedIncrement(&g_projectionTraceCount);
        if(projectionEvent>1000){if(projectionEvent==1001)(void)0;if(actual)actual->Release();if(stream)stream->Release();if(indexBuffer)indexBuffer->Release();if(declaration)declaration->Release();if(vertexShader)vertexShader->Release();if(pixelShader)pixelShader->Release();return;}
        (void)0;
        (void)0;
        if (declaration) {
            D3DVERTEXELEMENT9 elems[16]{}; UINT count=16;
            const HRESULT elemHr=declaration->GetDeclaration(elems,&count);
            char elemText[512]{}; size_t pos=0;
            if(SUCCEEDED(elemHr)) for(UINT i=0;i<count&&i<15&&pos+48<sizeof(elemText);++i){
                const int n=_snprintf_s(elemText+pos,sizeof(elemText)-pos,_TRUNCATE,"%s%u:%u/%u/%u/%u",i?";":"",elems[i].Stream,elems[i].Offset,elems[i].Type,elems[i].Usage,elems[i].UsageIndex);
                if(n<0)break; pos+=size_t(n);
            }
            (void)0;
        }
        if(SUCCEEDED(worldHr)&&SUCCEEDED(viewHr)&&SUCCEEDED(projHr)&&SUCCEEDED(viewportHr)&&haveGeometry){
            float minX=std::numeric_limits<float>::infinity(),minY=minX,maxX=-minX,maxY=-minX,minZ=minX,maxZ=-minX;
            unsigned front=0,inside=0; float sx[6]{},sy[6]{}; bool projected[6]{}; char points[1800]{}; size_t pos=0;
            for(int i=0;i<6;++i){
                const Vertex& v=d.vertices[i]; float a[4]={v.x,v.y,v.z,1},b[4]{},c[4]{},clip[4]{};
                TransformPoint(a,world,b);TransformPoint(b,view,c);TransformPoint(c,projection,clip);
                bool valid=std::isfinite(clip[3])&&std::abs(clip[3])>1e-6f;
                float nx=0,ny=0,nz=0,screenX=0,screenY=0;
                if(valid){nx=clip[0]/clip[3];ny=clip[1]/clip[3];nz=clip[2]/clip[3];screenX=float(vp.X)+(nx+1.0f)*float(vp.Width)*0.5f;screenY=float(vp.Y)+(1.0f-ny)*float(vp.Height)*0.5f;valid=std::isfinite(nx)&&std::isfinite(ny)&&std::isfinite(nz)&&std::isfinite(screenX)&&std::isfinite(screenY);}
                projected[i]=valid;
                if(valid){sx[i]=screenX;sy[i]=screenY;if(clip[3]>0)++front;if(clip[3]>0&&nx>=-1&&nx<=1&&ny>=-1&&ny<=1&&nz>=0&&nz<=1)++inside;minX=std::min(minX,screenX);maxX=std::max(maxX,screenX);minY=std::min(minY,screenY);maxY=std::max(maxY,screenY);minZ=std::min(minZ,nz);maxZ=std::max(maxZ,nz);}
                int n=_snprintf_s(points+pos,sizeof(points)-pos,_TRUNCATE,"%sp%d=(%sclip %.3f,%.3f,%.3f,%.3f ndc %.3f,%.3f,%.3f screen %.1f,%.1f)",i?" ":"",i,valid?"":"invalid ",clip[0],clip[1],clip[2],clip[3],nx,ny,nz,screenX,screenY); if(n<0)break;pos+=size_t(n);
            }
            const float area0=projected[0]&&projected[1]&&projected[2]?(sx[1]-sx[0])*(sy[2]-sy[0])-(sy[1]-sy[0])*(sx[2]-sx[0]):0.0f;
            const float area1=projected[3]&&projected[4]&&projected[5]?(sx[4]-sx[3])*(sy[5]-sy[3])-(sy[4]-sy[3])*(sx[5]-sx[3]):0.0f;
            (void)0;
            (void)0;
        }
    }
    if(actual)actual->Release(); if(stream)stream->Release(); if(indexBuffer)indexBuffer->Release(); if(declaration)declaration->Release(); if(vertexShader)vertexShader->Release(); if(pixelShader)pixelShader->Release();
}

extern "C" void __cdecl BeforeProvinceDraw(void* renderable, IDirect3DDevice9* drawDevice) {
    g_previousTexture = nullptr;
    g_restoreDevice = nullptr;
    IDirect3DDevice9* device = nullptr;
    if (!renderable || !drawDevice || !GetDevice(&device) || device != g_device || drawDevice != device)
        return;

    void* textureKey = renderable;
    IDirect3DTexture9* replacement = nullptr;
    bool found = FindTexture(textureKey, &replacement);
    if (!found) {
        textureKey = ResolveProvinceTextureKey(renderable);
        if (textureKey) found = FindTexture(textureKey, &replacement);
    }
    if (!found || !replacement) return;

    IDirect3DBaseTexture9* previous = nullptr;
    const HRESULT getHr = device->GetTexture(0, &previous);
    const HRESULT setHr = SUCCEEDED(getHr) ? device->SetTexture(0, replacement) : E_FAIL;
    replacement->Release();
    if (FAILED(getHr) || FAILED(setHr)) {
        if (previous) previous->Release();
        return;
    }

    g_previousTexture = previous;
    g_restoreDevice = device;
    // Keep TTF labels fully opaque for the draw; the caller restores its factor.
    DWORD textureFactor = 0;
    if (SUCCEEDED(device->GetRenderState(D3DRS_TEXTUREFACTOR, &textureFactor)) &&
        (textureFactor >> 24) != 0xff) {
        textureFactor = (textureFactor & 0x00ffffffu) | 0xff000000u;
        device->SetRenderState(D3DRS_TEXTUREFACTOR, textureFactor);
    }
}

extern "C" void __cdecl AfterProvinceDraw() {
    if (g_restoreDevice) g_restoreDevice->SetTexture(0, g_previousTexture);
    if (g_previousTexture) g_previousTexture->Release();
    g_previousTexture = nullptr;
    g_restoreDevice = nullptr;
}

} // namespace

extern "C" __declspec(naked) void ProvinceLockCallStub() {
    using namespace map_province_detail;
    __asm {
        pushad
        push ecx
        call CaptureRenderable
        add esp, 4
        popad
        call eax
        pushad
        push dword ptr [ebp-34h]
        push eax
        call RecordProvinceLockResult
        add esp, 8
        popad
        mov ebx, dword ptr [ebp-34h]
        jmp dword ptr [g_lockContinuation]
    }
}

extern "C" __declspec(naked) void ProvinceUnlockCallStub() {
    using namespace map_province_detail;
    __asm {
        // Replay original push-this / virtual Unlock call.
        pushad
        push eax
        call CaptureProvinceUnlockResource
        add esp, 4
        popad
        push eax
        mov eax, dword ptr [edx+30h]
        call eax
        pushad
        push eax
        call RecordProvinceUnlockResult
        add esp, 4
        popad
        jmp dword ptr [g_unlockContinuation]
    }
}

extern "C" __declspec(naked) void ProvinceDrawCallStub() {
    using namespace map_province_detail;
    __asm {
        pushad
        push esi
        push eax
        call BeforeProvinceDraw
        add esp, 8
        popad
        // This replaces the province loop's direct CALL. Invoke its original
        // target with the original registers/stack, then return to the loop.
        call dword ptr [g_provinceDrawOriginal]
        // FUN_009f9d50 returns the final D3D DrawPrimitive/Draw* HRESULT in EAX.
        // Preserve it before the diagnostic callback so the real result is observable.
        mov dword ptr [g_lastDrawCallResult], eax
        mov dword ptr [g_drawResultCaptured], 1
        pushad
        call AfterProvinceDraw
        popad
        ret
    }
}

extern "C" __declspec(naked) void ProvinceTriangleDrawStub() {
    using namespace map_province_detail;
    __asm {
        // At this verified FUN_009F9D50 site: EAX is PrimitiveCount,
        // EDI is the packet's vertex count, ESI is the D3D9 device.
        pushfd
        pushad
        push eax
        push edi
        push esi
        call CaptureActualTriangleDraw
        add esp, 12
        popad
        popfd
        // Replay original DrawPrimitive(4, 0, eax).
        push eax
        push 0
        push 4
        push esi
        call ecx
        pushad
        push eax
        call RecordActualTriangleDrawResult
        add esp, 4
        popad
        jmp dword ptr [g_triangleDrawContinuation]
    }
}

extern "C" __declspec(naked) void SlotExhaustionStub() {
    using namespace map_province_detail;
    __asm {
        pushad
        push dword ptr [ebp-18h]
        push dword ptr [ebp-14h]
        push edi
        push dword ptr [ebp+8]
        call TraceSlotExhausted
        add esp, 16
        popad
        mov ecx, dword ptr [ebp-0Ch]
        mov dword ptr fs:[0], ecx
        jmp dword ptr [g_slotExhaustionContinuation]
    }
}

extern "C" __declspec(naked) void SlotCandidateStub() {
    using namespace map_province_detail;
    __asm {
        pushad
        push eax
        call CaptureSlotCandidate
        add esp, 4
        popad
        mov eax, dword ptr [ebp+8]
        xor edi, edi
        add eax, 02BCh
        jmp dword ptr [g_slotCandidateContinuation]
    }
}

namespace map_province_detail {
bool PatchFive(void* address, const unsigned char expected[5], const unsigned char replacement[5]) {
    DWORD old = 0;
    if (!VirtualProtect(address, 5, PAGE_EXECUTE_READWRITE, &old)) return false;
    bool ok = std::memcmp(address, expected, 5) == 0;
    if (ok) { std::memcpy(address, replacement, 5); FlushInstructionCache(GetCurrentProcess(), address, 5); }
    DWORD ignored = 0;
    VirtualProtect(address, 5, old, &ignored);
    return ok;
}

bool PatchSix(void* address,const unsigned char expected[6],const unsigned char replacement[6]){
    DWORD old=0;if(!VirtualProtect(address,6,PAGE_EXECUTE_READWRITE,&old))return false;
    const bool ok=std::memcmp(address,expected,6)==0;
    if(ok){std::memcpy(address,replacement,6);FlushInstructionCache(GetCurrentProcess(),address,6);}
    DWORD ignored=0;VirtualProtect(address,6,old,&ignored);return ok;
}

bool PatchEight(void* address,const unsigned char expected[8],const unsigned char replacement[8]){
    DWORD old=0;if(!VirtualProtect(address,8,PAGE_EXECUTE_READWRITE,&old))return false;
    const bool ok=std::memcmp(address,expected,8)==0;
    if(ok){std::memcpy(address,replacement,8);FlushInstructionCache(GetCurrentProcess(),address,8);}
    DWORD ignored=0;VirtualProtect(address,8,old,&ignored);return ok;
}

bool InstallTenByteDiagnosticHook(uintptr_t base,uint32_t rva,const unsigned char expected[10],void* stub,uintptr_t* continuation){
    auto* site=reinterpret_cast<unsigned char*>(base+rva);unsigned char jump[10]={0xE9,0,0,0,0,0x90,0x90,0x90,0x90,0x90};
    const intptr_t displacement=reinterpret_cast<intptr_t>(stub)-(reinterpret_cast<intptr_t>(site)+5);
    if(displacement<INT32_MIN||displacement>INT32_MAX)return false;
    const int32_t rel=int32_t(displacement);std::memcpy(jump+1,&rel,sizeof(rel));DWORD old=0;
    if(!VirtualProtect(site,10,PAGE_EXECUTE_READWRITE,&old))return false;
    const bool ok=std::memcmp(site,expected,10)==0;
    if(ok){*continuation=uintptr_t(site+10);std::memcpy(site,jump,10);FlushInstructionCache(GetCurrentProcess(),site,10);}
    DWORD ignored=0;VirtualProtect(site,10,old,&ignored);return ok;
}

bool InstallSlotCandidateHook(uintptr_t base){const unsigned char expected[10]={0x8B,0x45,0x08,0x33,0xFF,0x05,0xBC,0x02,0x00,0x00};return InstallTenByteDiagnosticHook(base,kSlotCandidateRva,expected,reinterpret_cast<void*>(&SlotCandidateStub),&g_slotCandidateContinuation);}
bool InstallSlotExhaustionHook(uintptr_t base){const unsigned char expected[10]={0x8B,0x4D,0xF4,0x64,0x89,0x0D,0,0,0,0};return InstallTenByteDiagnosticHook(base,kSlotExhaustionRva,expected,reinterpret_cast<void*>(&SlotExhaustionStub),&g_slotExhaustionContinuation);}

bool InstallLockHook(uintptr_t base) {
    auto* site = reinterpret_cast<unsigned char*>(base + kProvinceLockCallRva);
    const unsigned char expected[5] = {0xFF,0xD0,0x8B,0x5D,0xCC};
    const intptr_t displacement = reinterpret_cast<intptr_t>(&ProvinceLockCallStub) - (reinterpret_cast<intptr_t>(site) + 5);
    if (displacement < INT32_MIN || displacement > INT32_MAX) return false;
    unsigned char jump[5] = {0xE9};
    const int32_t rel = int32_t(displacement);
    std::memcpy(jump + 1, &rel, sizeof(rel));
    g_lockContinuation = uintptr_t(site + 5);
    if (!PatchFive(site, expected, jump)) return false;
    return true;
}

bool InstallUnlockHook(uintptr_t base){
    auto* site=reinterpret_cast<unsigned char*>(base+kProvinceUnlockCallRva);
    const unsigned char expected[6]={0x50,0x8B,0x42,0x30,0xFF,0xD0};
    const intptr_t displacement=reinterpret_cast<intptr_t>(&ProvinceUnlockCallStub)-(reinterpret_cast<intptr_t>(site)+5);
    if(displacement<INT32_MIN||displacement>INT32_MAX)return false;
    unsigned char jump[6]={0xE9,0,0,0,0,0x90};const int32_t rel=int32_t(displacement);std::memcpy(jump+1,&rel,4);
    g_unlockContinuation=uintptr_t(site+6);
    return PatchSix(site,expected,jump);
}

bool InstallTriangleDrawHook(uintptr_t base){
    auto* site=reinterpret_cast<unsigned char*>(base+kActualTriangleDrawRva);
    const unsigned char expected[8]={0x50,0x6A,0x00,0x6A,0x04,0x56,0xFF,0xD1};
    const intptr_t displacement=reinterpret_cast<intptr_t>(&ProvinceTriangleDrawStub)-(reinterpret_cast<intptr_t>(site)+5);
    if(displacement<INT32_MIN||displacement>INT32_MAX)return false;
    unsigned char jump[8]={0xE9,0,0,0,0,0x90,0x90,0x90};const int32_t rel=int32_t(displacement);std::memcpy(jump+1,&rel,4);
    g_triangleDrawContinuation=uintptr_t(site+8);
    return PatchEight(site,expected,jump);
}
} // namespace map_province_detail

bool map_install_province_ttf_hooks(const V2ModAPI* api) {
    using namespace map_province_detail;
    using map_province_detail::g_api;
    if (!api || !api->Log || !api->gameBase) return false;
    g_api = api;
    const unsigned char builderBytes[5] = {0xE8,0x26,0x4C,0x23,0x00};
    const unsigned char drawBytes[5] = {0xE8,0x76,0xDA,0x1F,0x00};
    if (!g_builderPatch.install(reinterpret_cast<void*>(uintptr_t(api->gameBase) + kProvinceBuilderCallRva),
        builderBytes, reinterpret_cast<void*>(&HookProvinceBuilder))) {
        (void)0;
        return false;
    }
    g_builderOriginal = g_builderPatch.original_target();
    auto* drawSite = reinterpret_cast<unsigned char*>(uintptr_t(api->gameBase) + kProvinceDrawCallRva);
    const intptr_t drawDisplacement = reinterpret_cast<intptr_t>(&ProvinceDrawCallStub) - (reinterpret_cast<intptr_t>(drawSite) + 5);
    if (drawDisplacement < INT32_MIN || drawDisplacement > INT32_MAX) {
        g_builderPatch.remove();
        (void)0;
        return false;
    }
    unsigned char drawJump[5] = {0xE8};
    const int32_t drawRel = int32_t(drawDisplacement);
    std::memcpy(drawJump + 1, &drawRel, sizeof(drawRel));
    g_provinceDrawOriginal = uintptr_t(api->gameBase) + kProvinceDrawTargetRva;
    if (!PatchFive(drawSite, drawBytes, drawJump)) {
        g_builderPatch.remove();
        (void)0;
        return false;
    }
    if (!InstallLockHook(api->gameBase)) {
        const unsigned char originalDraw[5] = {0xE8,0x76,0xDA,0x1F,0x00};
        PatchFive(drawSite, drawJump, originalDraw);
        g_builderPatch.remove();
        (void)0;
        return false;
    }
    if(!InstallUnlockHook(api->gameBase)){
        auto* lockSite=reinterpret_cast<unsigned char*>(uintptr_t(api->gameBase)+kProvinceLockCallRva);
        const unsigned char lockJump[5]={0xE9,0,0,0,0};const unsigned char lockOriginal[5]={0xFF,0xD0,0x8B,0x5D,0xCC};
        DWORD old=0;if(VirtualProtect(lockSite,5,PAGE_EXECUTE_READWRITE,&old)){if(lockSite[0]==0xE9){std::memcpy(lockSite,lockOriginal,5);FlushInstructionCache(GetCurrentProcess(),lockSite,5);}DWORD ignored=0;VirtualProtect(lockSite,5,old,&ignored);}
        const unsigned char originalDraw[5]={0xE8,0x76,0xDA,0x1F,0x00};PatchFive(drawSite,drawJump,originalDraw);g_builderPatch.remove();
        (void)0;return false;
    }
    const bool actualTriangleDrawTrace=false;
    const bool slotCandidateTrace=InstallSlotCandidateHook(api->gameBase);
    const bool slotExhaustionTrace=false;
    (void)0;
    (void)0;
    (void)0;
    return true;
}



