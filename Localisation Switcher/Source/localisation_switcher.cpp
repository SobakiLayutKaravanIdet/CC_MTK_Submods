#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <vector>
#include <algorithm>
#include <new>
#include <utility>
#include "V2ModAPI.h"

namespace
{
    const uint32_t kSettingsLanguageCallbackRva = 0x0066F470;
    const uint32_t kSettingsLanguageSerializerVtableRva = 0x00A42C98;
    const uint32_t kSettingsLanguageSerializerVtableSlot = 2;
    const uint32_t kSettingsLanguageSerializerRva = 0x0066F640;
    const uint32_t kTokenStringLookupRva = 0x00590880;
    const uint32_t kRenameRegionEffectVtableRva = 0x00A2B638;
    const uint32_t kRenameRegionTooltipRva = 0x004A31A0;
    const int kRenameRegionTooltipSlot = 9;
    const uint32_t kWorldSingletonRva = 0x00E588E8;
    const uint32_t kLanguageKeyToken = 0x000000B8;
    const unsigned kCallbackStealLength = 11;
    const unsigned char kExpectedCallbackBytes[kCallbackStealLength] =
        { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C, 0x3D, 0xB8, 0x00, 0x00, 0x00 };
    const unsigned char kExpectedTokenLookupBytes[11] =
        { 0x55, 0x8B, 0xEC, 0x51, 0x80, 0x3D, 0x64, 0x8A, 0x25, 0x01, 0x00 };
    const uint32_t kPreferredImageBase = 0x00400000;
    const uint32_t kTokenLookupGlobalPreferredAddress = 0x01258A64;

    const uint32_t kNativeLanguageTokenIds[13] =
        { 0xB9, 0xBA, 0xBB, 0x12B, 0x12A, 0x129, 0xBC, 0x16B, 0x16C, 0x16D, 0x16E, 0x16F, 0x170 };
    const char* const kNativeLanguageIds[13] =
        { "l_english", "l_french", "l_german", "l_polish", "l_spanish", "l_italian", "l_swedish",
          "l_czech", "l_hungarian", "l_dutch", "l_portugese", "l_russian", "l_finnish" };

    typedef void (__thiscall *TSettingsLanguageCallback)(void* self, void* valueToken, uint32_t keyToken);
    typedef void (__thiscall *TSettingsLanguageSerializer)(void* self, void* output);
    typedef bool (__thiscall *TEngineStringReserve)(void* self, uint32_t requestedCapacity, int exact);
    const V2ModAPI* g_api = NULL;
    TSettingsLanguageCallback g_originalCallback = NULL;
    TSettingsLanguageSerializer g_originalSerializer = NULL;
    typedef void* (__thiscall *TRenameRegionTooltip)(void*, void*, void*, uint32_t, uint32_t,
        uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    TRenameRegionTooltip g_originalRenameRegionTooltip = NULL;
    volatile LONG g_renameRegionTooltipSkipLogged = 0;
    void* g_originalTokenLookup = NULL;
    unsigned char g_trampoline[64];
    unsigned char g_tokenLookupTrampoline[64];
    char g_slotNames[13][64];
    char g_activeLanguageId[64];
    __declspec(thread) int g_serializingSlot = 0;

    struct EngineString
    {
        union { char inlineText[16]; char* heapText; } storage;
        uint32_t length;
        uint32_t capacity;
    };

    bool HasSafeRegionRenameTooltipContext(void* eventScope)
    {
        if (!eventScope || !g_api) return false;
        __try
        {
            const unsigned char* scope = (const unsigned char*)eventScope;
            // CRenameRegionEffect uses the region index at CEventScope+0x18
            // when present. Otherwise it derives the region from the province
            // at +0x10, but the native tooltip dereferences a null name object
            // for unassigned provinces (the supplied crash is this case).
            if (*(const uint32_t*)(scope + 0x18) != 0) return true;

            const uint32_t provinceIndex = *(const uint32_t*)(scope + 0x10);
            unsigned char* image = (unsigned char*)g_api->gameBase;
            void* world = *(void**)(image + kWorldSingletonRva);
            if (!world) return false;
            void** provinces = *(void***)((unsigned char*)world + 0xACC);
            if (!provinces) return false;
            void* province = provinces[provinceIndex];
            if (!province) return false;
            if (!*(void**)((unsigned char*)province + 0x164)) return false;

            void* regionNameLink = *(void**)((unsigned char*)province + 0x15C);
            if (!regionNameLink) return false;
            void* regionNameObject = *(void**)regionNameLink;
            if (!regionNameObject) return false;
            void** vtable = *(void***)regionNameObject;
            return vtable && vtable[6] != NULL;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    void ClearTooltipOutput(void* output)
    {
        if (!output) return;
        __try
        {
            EngineString* text = (EngineString*)output;
            text->storage.inlineText[0] = '\0';
            text->length = 0;
            text->capacity = 15;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    bool ParseCsvField(const char* line, unsigned wantedField, std::string& field)
    {
        field.clear();
        unsigned current = 0;
        bool quoted = false;
        for (const char* p = line; *p; ++p)
        {
            const char c = *p;
            if (c == '"')
            {
                if (quoted && p[1] == '"')
                {
                    if (current == wantedField) field.push_back('"');
                    ++p;
                }
                else quoted = !quoted;
            }
            else if (c == ';' && !quoted)
            {
                if (current == wantedField) return true;
                ++current;
            }
            else if (current == wantedField && c != '\r' && c != '\n')
                field.push_back(c);
        }
        return current == wantedField;
    }

    bool LoadRenameRegionTemplate(std::string& text)
    {
        unsigned slot = 1;
        for (unsigned i = 0; i < 13; ++i)
        {
            if (g_activeLanguageId[0] && g_slotNames[i][0] &&
                _stricmp(g_activeLanguageId, g_slotNames[i]) == 0)
            {
                slot = i + 1;
                break;
            }
        }

        char paths[2][MAX_PATH] = {};
        if (g_api && g_api->modFolder && g_api->modFolder[0])
        {
            _snprintf_s(paths[0], sizeof(paths[0]), _TRUNCATE, "%s\\localisation\\text.csv", g_api->modFolder);
            char root[MAX_PATH] = {};
            strncpy_s(root, sizeof(root), g_api->modFolder, _TRUNCATE);
            char* slash = strrchr(root, '\\');
            if (slash) *slash = 0;
            slash = strrchr(root, '\\');
            if (slash) *slash = 0;
            _snprintf_s(paths[1], sizeof(paths[1]), _TRUNCATE, "%s\\localisation\\text.csv", root);
        }

        for (unsigned fileIndex = 0; fileIndex < 2; ++fileIndex)
        {
            if (!paths[fileIndex][0]) continue;
            FILE* file = NULL;
            if (fopen_s(&file, paths[fileIndex], "rb") != 0 || !file) continue;
            char line[16384];
            while (fgets(line, sizeof(line), file))
            {
                if (strncmp(line, "RENAME_REGION_EFF;", 18) != 0) continue;
                const unsigned csvField = slot; // field 0 is key; slot 1 is English, slot 2 is French, etc.
                const bool found = ParseCsvField(line, csvField, text);
                fclose(file);
                if (found && !text.empty()) return true;
                file = NULL;
                break;
            }
            if (file) fclose(file);
        }
        return false;
    }

    bool CopyRegionRenameValue(void* self, char* destination, uint32_t capacity, uint32_t* length)
    {
        if (!self || !destination || !length) return false;
        __try
        {
            EngineString* value = (EngineString*)((unsigned char*)self + 0x20);
            const uint32_t size = value->length;
            if (size > 4096 || size >= capacity || size > value->capacity) return false;
            const char* source = value->capacity > 15 ? value->storage.heapText : value->storage.inlineText;
            if (size && !source) return false;
            if (size) memcpy(destination, source, size);
            destination[size] = '\0';
            *length = size;
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    bool WriteRegionRenameTooltipFallback(void* output, void* self)
    {
        if (!output || !self || !g_api) return false;
        char selectedName[4097] = {};
        uint32_t nameLength = 0;
        if (!CopyRegionRenameValue(self, selectedName, sizeof(selectedName), &nameLength)) return false;

        EngineString* destination = (EngineString*)output;
        std::string templateText;
        if (!LoadRenameRegionTemplate(templateText))
            templateText = "$OLD$ renamed to \xA7Y$NEW$\xA7W.";
        const std::string oldToken = "$OLD$";
        size_t oldAt = 0;
        while ((oldAt = templateText.find(oldToken, oldAt)) != std::string::npos)
            templateText.erase(oldAt, oldToken.size());
        while (!templateText.empty() && isspace((unsigned char)templateText.front()))
            templateText.erase(templateText.begin());
        const std::string newToken = "$NEW$";
        const std::string selected(selectedName, nameLength);
        size_t newAt = 0;
        bool replacedNew = false;
        while ((newAt = templateText.find(newToken, newAt)) != std::string::npos)
        {
            templateText.replace(newAt, newToken.size(), selected);
            newAt += selected.size();
            replacedNew = true;
        }
        if (!replacedNew) return false;
        // text.csv stores the newline as the two literal bytes '\\n'; the
        // tooltip's parent scope already supplies the old region name.
        size_t newline = 0;
        while ((newline = templateText.find("\\n", newline)) != std::string::npos)
            templateText.erase(newline, 2);
        const uint32_t totalLength = (uint32_t)templateText.size();
        if (totalLength > 4096) return false;
        if (totalLength > 15)
        {
            TEngineStringReserve reserve = (TEngineStringReserve)(g_api->gameBase + 0x000094E0);
            if (!reserve(destination, totalLength, 1)) return false;
        }

        char* destinationData = destination->capacity > 15 ? destination->storage.heapText : destination->storage.inlineText;
        if (!destinationData) return false;
        if (totalLength) memcpy(destinationData, templateText.data(), totalLength);
        destinationData[totalLength] = '\0';
        destination->length = totalLength;
        return true;
    }

    void* __fastcall RenameRegionTooltipHook(void* self, void* edx, void* output,
        void* eventScope, uint32_t arg2, uint32_t arg3, uint32_t arg4, uint32_t arg5,
        uint32_t arg6, uint32_t arg7, uint32_t arg8, uint32_t arg9)
    {
        if (!HasSafeRegionRenameTooltipContext(eventScope))
        {
            if (InterlockedCompareExchange(&g_renameRegionTooltipSkipLogged, 1, 0) == 0)
                g_api->Log("LocalisationSwitcher: skipped CRenameRegionEffect tooltip with missing/invalid region context; rename effect remains active");
            // The native tooltip dereferences a missing region name in this
            // context. Keep the row visible with the effect's parsed target
            // name, while avoiding all native scope/name lookup work.
            ClearTooltipOutput(output);
            WriteRegionRenameTooltipFallback(output, self);
            return output;
        }

        return g_originalRenameRegionTooltip ?
            g_originalRenameRegionTooltip(self, output, eventScope, arg2, arg3, arg4, arg5,
                arg6, arg7, arg8, arg9) : output;
    }

    bool InstallRenameRegionTooltipGuard()
    {
        const uint32_t expected = g_api->gameBase + kRenameRegionTooltipRva;
        const uint32_t current = *(uint32_t*)(g_api->gameBase + kRenameRegionEffectVtableRva +
            kRenameRegionTooltipSlot * sizeof(uint32_t));
        if (current != expected)
        {
            g_api->Log("LocalisationSwitcher: CRenameRegionEffect tooltip vtable mismatch (current=%08X expected=%08X); guard not installed",
                current, expected);
            return false;
        }
        if (!g_api->PatchSlot(kRenameRegionEffectVtableRva, kRenameRegionTooltipSlot,
                (void*)&RenameRegionTooltipHook, (void**)&g_originalRenameRegionTooltip))
        {
            g_api->Log("LocalisationSwitcher: failed to install CRenameRegionEffect tooltip guard");
            return false;
        }
        g_api->Log("LocalisationSwitcher: guarded CRenameRegionEffect tooltip against missing region context");
        return true;
    }

    void Trim(char* text)
    {
        if (!text) return;
        char* first = text;
        while (*first && isspace((unsigned char)*first)) ++first;
        if (first != text) memmove(text, first, strlen(first) + 1);
        size_t length = strlen(text);
        while (length && isspace((unsigned char)text[length - 1])) text[--length] = 0;
    }

    bool IsValidLanguageId(const char* text)
    {
        if (!text || strncmp(text, "l_", 2) != 0 || !text[2]) return false;
        for (const unsigned char* p = (const unsigned char*)text; *p; ++p)
            if (!(isalnum(*p) || *p == '_')) return false;
        return true;
    }

    void LoadSlotConfiguration(const char* modFolder)
    {
        char path[MAX_PATH] = {};
        _snprintf_s(path, sizeof(path), _TRUNCATE, "%s\\Localisation Switcher\\localisation_switcher.ini", modFolder ? modFolder : "");
        FILE* file = NULL;
        if (fopen_s(&file, path, "rb") != 0 || !file)
        {
            g_api->Log("LocalisationSwitcher: cannot read config '%s'; native mappings remain active", path);
            return;
        }

        char line[512];
        bool inSlots = false;
        unsigned loaded = 0;
        while (fgets(line, sizeof(line), file))
        {
            if (line[0] == '\xEF' && (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF)
                memmove(line, line + 3, strlen(line + 3) + 1);
            Trim(line);
            if (!line[0] || line[0] == ';' || line[0] == '#') continue;
            if (line[0] == '[')
            {
                char* end = strchr(line, ']');
                if (end) *end = 0;
                inSlots = _stricmp(line + 1, "slots") == 0;
                continue;
            }
            if (!inSlots) continue;
            char* equal = strchr(line, '=');
            if (!equal) continue;
            *equal++ = 0;
            Trim(line);
            Trim(equal);
            unsigned slot = 0;
            char extra = 0;
            if (sscanf_s(line, "slot_%u%c", &slot, &extra, 1) != 1 || slot < 1 || slot > 13)
                continue;
            if (!IsValidLanguageId(equal) || strlen(equal) >= sizeof(g_slotNames[0]))
            {
                g_api->Log("LocalisationSwitcher: invalid language id for slot_%u: '%s'", slot, equal);
                continue;
            }
            strncpy_s(g_slotNames[slot - 1], sizeof(g_slotNames[slot - 1]), equal, _TRUNCATE);
            ++loaded;
        }
        fclose(file);
        g_api->Log("LocalisationSwitcher: loaded %u configured slot names from '%s'", loaded, path);
    }

    void __fastcall SettingsLanguageCallbackHook(void* self, void* edx, void* valueToken, uint32_t keyToken)
    {
        if (g_originalCallback)
            g_originalCallback(self, valueToken, keyToken);

        if (!self || !valueToken || keyToken != kLanguageKeyToken)
            return;

        // Token objects contain the token ID at +0x228 and the original lexeme
        // at +0x22C. The latter remains available even when the ID is not one
        // of the 13 built-in language tokens.
        const char* requestedId = (const char*)((const unsigned char*)valueToken + 0x22C);
        for (unsigned i = 0; i < 13; ++i)
        {
            if (g_slotNames[i][0] && _stricmp(requestedId, g_slotNames[i]) == 0)
            {
                *(uint32_t*)((unsigned char*)self + 8) = i + 1;
                strncpy_s(g_activeLanguageId, sizeof(g_activeLanguageId), requestedId, _TRUNCATE);
                g_api->Log("LocalisationSwitcher: language '%s' selects slot %u", requestedId, i + 1);
                return;
            }
        }
    }

    enum class ScriptTokenKind { Word, Quoted, Equal, OpenBrace, CloseBrace, Other };

    struct ScriptToken
    {
        ScriptTokenKind kind;
        size_t begin;
        size_t end;
        std::string value;
    };

    bool IsScriptSpace(unsigned char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
    }

    bool TokenizeScript(const std::string& text, std::vector<ScriptToken>& tokens)
    {
        tokens.clear();
        size_t i = 0;
        while (i < text.size())
        {
            const unsigned char c = (unsigned char)text[i];
            if (IsScriptSpace(c)) { ++i; continue; }
            if (c == '#')
            {
                while (i < text.size() && text[i] != '\n') ++i;
                continue;
            }

            ScriptToken token = {};
            token.begin = i;
            if (c == '"')
            {
                token.kind = ScriptTokenKind::Quoted;
                ++i;
                bool closed = false;
                while (i < text.size())
                {
                    if (text[i] == '\\' && i + 1 < text.size()) { i += 2; continue; }
                    if (text[i++] == '"') { closed = true; break; }
                }
                if (!closed) return false;
                token.end = i;
                token.value.assign(text.data() + token.begin, token.end - token.begin);
            }
            else if (c == '=')
            {
                token.kind = ScriptTokenKind::Equal;
                token.end = ++i;
                token.value = "=";
            }
            else if (c == '{')
            {
                token.kind = ScriptTokenKind::OpenBrace;
                token.end = ++i;
                token.value = "{";
            }
            else if (c == '}')
            {
                token.kind = ScriptTokenKind::CloseBrace;
                token.end = ++i;
                token.value = "}";
            }
            else
            {
                token.kind = ScriptTokenKind::Word;
                while (i < text.size() && !IsScriptSpace((unsigned char)text[i]) &&
                    text[i] != '#' && text[i] != '"' && text[i] != '=' && text[i] != '{' && text[i] != '}')
                    ++i;
                if (i == token.begin) { token.kind = ScriptTokenKind::Other; ++i; }
                token.end = i;
                token.value.assign(text.data() + token.begin, token.end - token.begin);
            }
            tokens.push_back(token);
        }
        return true;
    }

    size_t FindMatchingBrace(const std::vector<ScriptToken>& tokens, size_t open)
    {
        if (open >= tokens.size() || tokens[open].kind != ScriptTokenKind::OpenBrace) return tokens.size();
        unsigned depth = 0;
        for (size_t i = open; i < tokens.size(); ++i)
        {
            if (tokens[i].kind == ScriptTokenKind::OpenBrace) ++depth;
            else if (tokens[i].kind == ScriptTokenKind::CloseBrace && --depth == 0) return i;
        }
        return tokens.size();
    }

    bool IsLanguageWord(const std::string& word)
    {
        if (word.size() < 3 || _strnicmp(word.c_str(), "l_", 2) != 0) return false;
        for (size_t i = 2; i < word.size(); ++i)
        {
            const unsigned char c = (unsigned char)word[i];
            if (!(isalnum(c) || c == '_')) return false;
        }
        return true;
    }

    void AppendLine(std::string& output, const std::string& value)
    {
        output.append("\r\n\t\t\t");
        output.append(value);
    }

    bool BuildFlatUnitNameBody(const std::string& source, const std::vector<ScriptToken>& tokens,
        size_t openBrace, size_t closeBrace, const char* activeLanguage, std::string& body, bool* changed)
    {
        std::vector<std::string> defaults;
        std::vector<std::string> selected;
        bool foundLanguageBlock = false;
        bool matchedLanguage = false;

        for (size_t i = openBrace + 1; i < closeBrace;)
        {
            const ScriptToken& token = tokens[i];
            if (token.kind == ScriptTokenKind::Quoted)
            {
                defaults.push_back(token.value);
                ++i;
                continue;
            }

            if (token.kind == ScriptTokenKind::Word && IsLanguageWord(token.value) &&
                i + 2 < closeBrace && tokens[i + 1].kind == ScriptTokenKind::Equal &&
                tokens[i + 2].kind == ScriptTokenKind::OpenBrace)
            {
                const size_t languageClose = FindMatchingBrace(tokens, i + 2);
                if (languageClose >= closeBrace) return false;
                foundLanguageBlock = true;
                const bool isSelected = activeLanguage && activeLanguage[0] &&
                    _stricmp(token.value.c_str(), activeLanguage) == 0;
                if (isSelected && !matchedLanguage)
                {
                    for (size_t j = i + 3; j < languageClose; ++j)
                        if (tokens[j].kind == ScriptTokenKind::Quoted)
                            selected.push_back(tokens[j].value);
                    matchedLanguage = true;
                }
                i = languageClose + 1;
                continue;
            }

            ++i;
        }

        if (!foundLanguageBlock)
        {
            *changed = false;
            return true;
        }

        body.clear();
        const std::vector<std::string>& names = matchedLanguage ? selected : defaults;
        for (const std::string& name : names) AppendLine(body, name);
        body.append("\r\n\t\t");
        *changed = true;
        return true;
    }

    bool PreprocessUnitNameBlocks(const std::string& source, const char* activeLanguage, std::string& output)
    {
        std::vector<ScriptToken> tokens;
        if (!TokenizeScript(source, tokens)) return false;

        struct Replacement { size_t begin; size_t end; std::string text; };
        std::vector<Replacement> replacements;
        for (size_t i = 0; i + 2 < tokens.size(); ++i)
        {
            if (tokens[i].kind != ScriptTokenKind::Word || _stricmp(tokens[i].value.c_str(), "unit_names") != 0 ||
                tokens[i + 1].kind != ScriptTokenKind::Equal || tokens[i + 2].kind != ScriptTokenKind::OpenBrace)
                continue;
            const size_t unitNamesClose = FindMatchingBrace(tokens, i + 2);
            if (unitNamesClose >= tokens.size()) return false;
            for (size_t j = i + 3; j + 2 < unitNamesClose;)
            {
                if (tokens[j].kind == ScriptTokenKind::Word && tokens[j + 1].kind == ScriptTokenKind::Equal &&
                    tokens[j + 2].kind == ScriptTokenKind::OpenBrace)
                {
                    const size_t typeClose = FindMatchingBrace(tokens, j + 2);
                    if (typeClose >= unitNamesClose) return false;
                    bool changed = false;
                    std::string body;
                    if (!BuildFlatUnitNameBody(source, tokens, j + 2, typeClose, activeLanguage, body, &changed))
                        return false;
                    if (changed)
                        replacements.push_back({ tokens[j + 2].end, tokens[typeClose].begin, std::move(body) });
                    j = typeClose + 1;
                }
                else ++j;
            }
            i = unitNamesClose;
        }

        if (replacements.empty()) { output = source; return true; }
        output = source;
        for (auto it = replacements.rbegin(); it != replacements.rend(); ++it)
            output.replace(it->begin, it->end - it->begin, it->text);
        return true;
    }

    bool IsListKey(const ScriptToken& token)
    {
        return token.kind == ScriptTokenKind::Word &&
            (_stricmp(token.value.c_str(), "first_names") == 0 ||
             _stricmp(token.value.c_str(), "last_names") == 0);
    }

    bool PreprocessCultureBlocks(const std::string& source, const char* activeLanguage, std::string& output)
    {
        std::vector<ScriptToken> tokens;
        if (!TokenizeScript(source, tokens)) return false;

        struct Replacement { size_t begin; size_t end; std::string text; };
        std::vector<Replacement> replacements;

        // Culture records are nested inside culture-group records. Walk each
        // block's direct children so group-level nesting cannot be mistaken
        // for a culture's first_names/last_names fields.
        for (size_t cultureOpen = 0; cultureOpen + 1 < tokens.size(); ++cultureOpen)
        {
            if (tokens[cultureOpen].kind != ScriptTokenKind::OpenBrace) continue;
            const size_t cultureClose = FindMatchingBrace(tokens, cultureOpen);
            if (cultureClose >= tokens.size()) return false;

            struct Field { size_t open; size_t close; } firstNames = { tokens.size(), tokens.size() },
                lastNames = { tokens.size(), tokens.size() };
            struct LanguageBlock { size_t begin; size_t open; size_t close; bool selected; };
            std::vector<LanguageBlock> languageBlocks;

            for (size_t i = cultureOpen + 1; i < cultureClose;)
            {
                if (tokens[i].kind == ScriptTokenKind::Word && i + 2 < cultureClose &&
                    tokens[i + 1].kind == ScriptTokenKind::Equal &&
                    tokens[i + 2].kind == ScriptTokenKind::OpenBrace)
                {
                    const size_t childClose = FindMatchingBrace(tokens, i + 2);
                    if (childClose >= cultureClose) return false;
                    if (_stricmp(tokens[i].value.c_str(), "first_names") == 0)
                        firstNames = { i + 2, childClose };
                    else if (_stricmp(tokens[i].value.c_str(), "last_names") == 0)
                        lastNames = { i + 2, childClose };
                    else if (IsLanguageWord(tokens[i].value))
                        languageBlocks.push_back({ i, i + 2, childClose,
                            activeLanguage && activeLanguage[0] &&
                            _stricmp(tokens[i].value.c_str(), activeLanguage) == 0 });
                    i = childClose + 1;
                }
                else ++i;
            }

            if (languageBlocks.empty() ||
                (firstNames.open == tokens.size() && lastNames.open == tokens.size()))
            {
                continue;
            }

            size_t selectedBlock = tokens.size();
            for (size_t n = 0; n < languageBlocks.size(); ++n)
                if (languageBlocks[n].selected) { selectedBlock = n; break; }

            if (selectedBlock < languageBlocks.size())
            {
                const LanguageBlock& selected = languageBlocks[selectedBlock];
                Field localizedFirst = { tokens.size(), tokens.size() };
                Field localizedLast = { tokens.size(), tokens.size() };
                for (size_t i = selected.open + 1; i < selected.close;)
                {
                    if (tokens[i].kind == ScriptTokenKind::Word && i + 2 < selected.close &&
                        tokens[i + 1].kind == ScriptTokenKind::Equal &&
                        tokens[i + 2].kind == ScriptTokenKind::OpenBrace)
                    {
                        const size_t listClose = FindMatchingBrace(tokens, i + 2);
                        if (listClose >= selected.close) return false;
                        if (_stricmp(tokens[i].value.c_str(), "first_names") == 0)
                            localizedFirst = { i + 2, listClose };
                        else if (_stricmp(tokens[i].value.c_str(), "last_names") == 0)
                            localizedLast = { i + 2, listClose };
                        i = listClose + 1;
                    }
                    else ++i;
                }

                if (firstNames.open < tokens.size() && localizedFirst.open < tokens.size())
                    replacements.push_back({ tokens[firstNames.open].end,
                        tokens[firstNames.close].begin,
                        source.substr(tokens[localizedFirst.open].end,
                            tokens[localizedFirst.close].begin - tokens[localizedFirst.open].end) });
                if (lastNames.open < tokens.size() && localizedLast.open < tokens.size())
                    replacements.push_back({ tokens[lastNames.open].end,
                        tokens[lastNames.close].begin,
                        source.substr(tokens[localizedLast.open].end,
                            tokens[localizedLast.close].begin - tokens[localizedLast.open].end) });
            }

            // Language blocks are implementation metadata, not fields the
            // vanilla cultures parser understands. Remove every such block;
            // unmatched languages keep the original lists above.
            for (const LanguageBlock& block : languageBlocks)
                replacements.push_back({ tokens[block.begin].begin,
                    tokens[block.close].end, std::string() });
        }

        if (replacements.empty()) { output = source; return true; }
        output = source;
        for (auto it = replacements.rbegin(); it != replacements.rend(); ++it)
            output.replace(it->begin, it->end - it->begin, it->text);
        return true;
    }

    bool IsEventOrDecisionScriptPath(const char* path)
    {
        if (!path) return false;
        char normalized[MAX_PATH] = {};
        strncpy_s(normalized, sizeof(normalized), path, _TRUNCATE);
        for (char* p = normalized; *p; ++p)
        {
            if (*p == '\\') *p = '/';
            *p = (char)tolower((unsigned char)*p);
        }
        const size_t length = strlen(normalized);
        if (length < 4 || _stricmp(normalized + length - 4, ".txt") != 0) return false;
        return strstr(normalized, "decisions/") != NULL || strstr(normalized, "events/") != NULL;
    }

    bool IsOobUnitsScriptPath(const char* path)
    {
        if (!path) return false;
        char normalized[MAX_PATH] = {};
        strncpy_s(normalized, sizeof(normalized), path, _TRUNCATE);
        for (char* p = normalized; *p; ++p)
        {
            if (*p == '\\') *p = '/';
            *p = (char)tolower((unsigned char)*p);
        }
        const size_t length = strlen(normalized);
        return length >= 4 && _stricmp(normalized + length - 4, ".txt") == 0 &&
            strstr(normalized, "history/units/") != NULL;
    }

    bool IsWarHistoryScriptPath(const char* path)
    {
        if (!path) return false;
        char normalized[MAX_PATH] = {};
        strncpy_s(normalized, sizeof(normalized), path, _TRUNCATE);
        for (char* p = normalized; *p; ++p)
        {
            if (*p == '\\') *p = '/';
            *p = (char)tolower((unsigned char)*p);
        }
        const size_t length = strlen(normalized);
        return length >= 4 && _stricmp(normalized + length - 4, ".txt") == 0 &&
            strstr(normalized, "history/wars/") != NULL;
    }

    bool PreprocessProvinceRenameEffects(const std::string& source, const char* activeLanguage,
        std::string& output)
    {
        std::vector<ScriptToken> tokens;
        if (!TokenizeScript(source, tokens)) return false;

        struct Replacement { size_t begin; size_t end; std::string text; };
        std::vector<Replacement> replacements;
        for (size_t effect = 0; effect + 2 < tokens.size(); ++effect)
        {
            if (tokens[effect].kind != ScriptTokenKind::Word ||
                (_stricmp(tokens[effect].value.c_str(), "change_province_name") != 0 &&
                 _stricmp(tokens[effect].value.c_str(), "change_region_name") != 0) ||
                tokens[effect + 1].kind != ScriptTokenKind::Equal ||
                tokens[effect + 2].kind != ScriptTokenKind::OpenBrace)
                continue;

            const size_t effectClose = FindMatchingBrace(tokens, effect + 2);
            if (effectClose >= tokens.size()) return false;
            std::string defaultValue;
            std::string selectedValue;
            bool hasDefault = false;
            bool hasSelected = false;

            // The localized wrapper has direct scalar children:
            // default = "..." and l_<id> = "...". Ignore other child keys
            // here; after selection the game receives only a quoted scalar.
            for (size_t field = effect + 3; field < effectClose;)
            {
                if (field + 2 < effectClose && tokens[field].kind == ScriptTokenKind::Word &&
                    tokens[field + 1].kind == ScriptTokenKind::Equal)
                {
                    if (tokens[field + 2].kind == ScriptTokenKind::Quoted)
                    {
                        if (_stricmp(tokens[field].value.c_str(), "default") == 0)
                        {
                            if (!hasDefault)
                            {
                                defaultValue = tokens[field + 2].value;
                                hasDefault = true;
                            }
                        }
                        else if (IsLanguageWord(tokens[field].value) && activeLanguage &&
                            activeLanguage[0] &&
                            _stricmp(tokens[field].value.c_str(), activeLanguage) == 0 && !hasSelected)
                        {
                            selectedValue = tokens[field + 2].value;
                            hasSelected = true;
                        }
                        field += 3;
                        continue;
                    }
                    if (tokens[field + 2].kind == ScriptTokenKind::OpenBrace)
                    {
                        const size_t nestedClose = FindMatchingBrace(tokens, field + 2);
                        if (nestedClose >= effectClose) return false;
                        field = nestedClose + 1;
                        continue;
                    }
                }
                ++field;
            }

            if (!hasDefault)
            {
                g_api->Log("LocalisationSwitcher: localized rename effect in script has no default value; leaving source unchanged");
                effect = effectClose;
                continue;
            }
            const std::string& value = hasSelected ? selectedValue : defaultValue;
            replacements.push_back({ tokens[effect + 2].begin, tokens[effectClose].end, value });
            effect = effectClose;
        }

        if (replacements.empty()) { output = source; return true; }
        output = source;
        for (auto it = replacements.rbegin(); it != replacements.rend(); ++it)
            output.replace(it->begin, it->end - it->begin, it->text);
        return true;
    }

    bool PreprocessLocalizedNamesInBlocks(const std::string& source, const char* activeLanguage,
        const char* const* blockNames, size_t blockNameCount, std::string& output)
    {
        std::vector<ScriptToken> tokens;
        if (!TokenizeScript(source, tokens)) return false;

        struct Replacement { size_t begin; size_t end; std::string text; };
        std::vector<Replacement> replacements;

        // Rewrite only a direct name field in a block explicitly listed by the
        // caller. Continue scanning inside parent blocks so nested OOB records
        // (army -> regiment and navy -> ship) are independently handled.
        for (size_t effect = 0; effect + 2 < tokens.size(); ++effect)
        {
            if (tokens[effect].kind != ScriptTokenKind::Word ||
                tokens[effect + 1].kind != ScriptTokenKind::Equal ||
                tokens[effect + 2].kind != ScriptTokenKind::OpenBrace)
                continue;

            bool targetBlock = false;
            for (size_t type = 0; type < blockNameCount; ++type)
                if (_stricmp(tokens[effect].value.c_str(), blockNames[type]) == 0)
                {
                    targetBlock = true;
                    break;
                }
            if (!targetBlock) continue;

            const size_t effectClose = FindMatchingBrace(tokens, effect + 2);
            if (effectClose >= tokens.size()) return false;

            for (size_t field = effect + 3; field < effectClose;)
            {
                if (field + 2 < effectClose && tokens[field].kind == ScriptTokenKind::Word &&
                    tokens[field + 1].kind == ScriptTokenKind::Equal &&
                    tokens[field + 2].kind == ScriptTokenKind::OpenBrace)
                {
                    const size_t valueClose = FindMatchingBrace(tokens, field + 2);
                    if (valueClose >= effectClose) return false;

                    if (_stricmp(tokens[field].value.c_str(), "name") == 0)
                    {
                        std::string defaultValue;
                        std::string selectedValue;
                        bool hasDefault = false;
                        bool hasSelected = false;

                        for (size_t entry = field + 3; entry < valueClose;)
                        {
                            if (entry + 2 < valueClose && tokens[entry].kind == ScriptTokenKind::Word &&
                                tokens[entry + 1].kind == ScriptTokenKind::Equal &&
                                tokens[entry + 2].kind == ScriptTokenKind::Quoted)
                            {
                                if (_stricmp(tokens[entry].value.c_str(), "default") == 0 && !hasDefault)
                                {
                                    defaultValue = tokens[entry + 2].value;
                                    hasDefault = true;
                                }
                                else if (IsLanguageWord(tokens[entry].value) && activeLanguage &&
                                    activeLanguage[0] &&
                                    _stricmp(tokens[entry].value.c_str(), activeLanguage) == 0 && !hasSelected)
                                {
                                    selectedValue = tokens[entry + 2].value;
                                    hasSelected = true;
                                }
                                entry += 3;
                                continue;
                            }
                            if (entry + 2 < valueClose && tokens[entry + 1].kind == ScriptTokenKind::Equal &&
                                tokens[entry + 2].kind == ScriptTokenKind::OpenBrace)
                            {
                                const size_t nestedClose = FindMatchingBrace(tokens, entry + 2);
                                if (nestedClose >= valueClose) return false;
                                entry = nestedClose + 1;
                                continue;
                            }
                            ++entry;
                        }

                        if (hasDefault)
                        {
                            const std::string& chosen = hasSelected ? selectedValue : defaultValue;
                            replacements.push_back({ tokens[field + 2].begin, tokens[valueClose].end, chosen });
                        }
                        else
                        {
                            g_api->Log("LocalisationSwitcher: localized name in '%s' block has no default; leaving name block unchanged",
                                tokens[effect].value.c_str());
                        }
                    }

                    field = valueClose + 1;
                    continue;
                }
                ++field;
            }
        }

        if (replacements.empty()) { output = source; return true; }
        output = source;
        for (auto it = replacements.rbegin(); it != replacements.rend(); ++it)
            output.replace(it->begin, it->end - it->begin, it->text);
        return true;
    }

    bool PreprocessLocalizedLeaderNameBlocks(const std::string& source, const char* activeLanguage,
        std::string& output)
    {
        static const char* const blocks[] = { "define_general", "define_admiral" };
        return PreprocessLocalizedNamesInBlocks(source, activeLanguage, blocks,
            sizeof(blocks) / sizeof(blocks[0]), output);
    }

    bool PreprocessLocalizedOobNames(const std::string& source, const char* activeLanguage,
        std::string& output)
    {
        static const char* const blocks[] = { "leader", "army", "regiment", "navy", "ship" };
        return PreprocessLocalizedNamesInBlocks(source, activeLanguage, blocks,
            sizeof(blocks) / sizeof(blocks[0]), output);
    }

    bool PreprocessLocalizedWarName(const std::string& source, const char* activeLanguage,
        std::string& output)
    {
        std::vector<ScriptToken> tokens;
        if (!TokenizeScript(source, tokens)) return false;

        struct Replacement { size_t begin; size_t end; std::string text; };
        std::vector<Replacement> replacements;

        // War history files store `name` at the document root, followed by
        // top-level date blocks. Skip those blocks so unrelated nested fields
        // named `name` cannot be interpreted as a war title.
        for (size_t field = 0; field < tokens.size();)
        {
            if (field + 2 < tokens.size() && tokens[field].kind == ScriptTokenKind::Word &&
                _stricmp(tokens[field].value.c_str(), "name") == 0 &&
                tokens[field + 1].kind == ScriptTokenKind::Equal &&
                tokens[field + 2].kind == ScriptTokenKind::OpenBrace)
            {
                const size_t nameClose = FindMatchingBrace(tokens, field + 2);
                if (nameClose >= tokens.size()) return false;

                std::string defaultValue;
                std::string selectedValue;
                bool hasDefault = false;
                bool hasSelected = false;
                for (size_t entry = field + 3; entry < nameClose;)
                {
                    if (entry + 2 < nameClose && tokens[entry].kind == ScriptTokenKind::Word &&
                        tokens[entry + 1].kind == ScriptTokenKind::Equal &&
                        tokens[entry + 2].kind == ScriptTokenKind::Quoted)
                    {
                        if (_stricmp(tokens[entry].value.c_str(), "default") == 0 && !hasDefault)
                        {
                            defaultValue = tokens[entry + 2].value;
                            hasDefault = true;
                        }
                        else if (IsLanguageWord(tokens[entry].value) && activeLanguage &&
                            activeLanguage[0] &&
                            _stricmp(tokens[entry].value.c_str(), activeLanguage) == 0 && !hasSelected)
                        {
                            selectedValue = tokens[entry + 2].value;
                            hasSelected = true;
                        }
                        entry += 3;
                        continue;
                    }
                    if (entry + 2 < nameClose && tokens[entry + 1].kind == ScriptTokenKind::Equal &&
                        tokens[entry + 2].kind == ScriptTokenKind::OpenBrace)
                    {
                        const size_t nestedClose = FindMatchingBrace(tokens, entry + 2);
                        if (nestedClose >= nameClose) return false;
                        entry = nestedClose + 1;
                        continue;
                    }
                    ++entry;
                }

                if (hasDefault)
                {
                    const std::string& chosen = hasSelected ? selectedValue : defaultValue;
                    replacements.push_back({ tokens[field + 2].begin, tokens[nameClose].end, chosen });
                }
                else
                    g_api->Log("LocalisationSwitcher: localized war name has no default; leaving name block unchanged");

                field = nameClose + 1;
                continue;
            }

            if (tokens[field].kind == ScriptTokenKind::OpenBrace)
            {
                const size_t blockClose = FindMatchingBrace(tokens, field);
                if (blockClose >= tokens.size()) return false;
                field = blockClose + 1;
                continue;
            }
            ++field;
        }

        if (replacements.empty()) { output = source; return true; }
        output = source;
        for (auto it = replacements.rbegin(); it != replacements.rend(); ++it)
            output.replace(it->begin, it->end - it->begin, it->text);
        return true;
    }

    struct VirtualCountryFile
    {
        void* handle;
        std::vector<unsigned char> contents;
        size_t position;
    };

    typedef void* (__cdecl *TPhysfsOpenRead)(const char*);
    typedef __int64 (__cdecl *TPhysfsRead)(void*, void*, unsigned, unsigned);
    typedef int (__cdecl *TPhysfsClose)(void*);
    typedef __int64 (__cdecl *TPhysfsInt64)(void*);

    const uint32_t kPhysfsOpenReadRva = 0x00721F40;
    const uint32_t kPhysfsReadRva = 0x007206E0;
    const uint32_t kPhysfsTellRva = 0x00720780;
    const uint32_t kPhysfsFileLengthRva = 0x007207C0;
    const uint32_t kPhysfsCloseRva = 0x007221E0;
    const uint32_t kPreferredPhysfsMutexAddress = 0x00F20AA0;
    const unsigned char kPhysfsOpenReadBytes[6] = { 0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x10 };
    const unsigned char kPhysfsReadBytes[7] = { 0x55, 0x8B, 0xEC, 0x57, 0x8B, 0x7D, 0x08 };
    const unsigned char kPhysfsTellBytes[7] = { 0x55, 0x8B, 0xEC, 0x56, 0x8B, 0x75, 0x08 };
    const unsigned char kPhysfsFileLengthBytes[6] = { 0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x08 };
    const unsigned char kPhysfsClosePrefix[4] = { 0x55, 0x8B, 0xEC, 0xA1 };
    TPhysfsOpenRead g_realPhysfsOpenRead = NULL;
    TPhysfsRead g_realPhysfsRead = NULL;
    TPhysfsInt64 g_realPhysfsTell = NULL;
    TPhysfsInt64 g_realPhysfsFileLength = NULL;
    TPhysfsClose g_realPhysfsClose = NULL;
    unsigned char g_physfsOpenTramp[64];
    unsigned char g_physfsReadTramp[64];
    unsigned char g_physfsTellTramp[64];
    unsigned char g_physfsLengthTramp[64];
    unsigned char g_physfsCloseTramp[64];
    CRITICAL_SECTION g_virtualFileLock;
    bool g_virtualFileLockReady = false;
    std::vector<VirtualCountryFile*> g_virtualCountryFiles;
    __declspec(thread) bool g_loadingVirtualSource = false;

    bool IsCulturesScriptPath(const char* path)
    {
        if (!path) return false;
        char normalized[MAX_PATH] = {};
        strncpy_s(normalized, sizeof(normalized), path, _TRUNCATE);
        for (char* p = normalized; *p; ++p)
        {
            if (*p == '\\') *p = '/';
            *p = (char)tolower((unsigned char)*p);
        }
        const size_t length = strlen(normalized);
        const char* culturesSuffix = "common/cultures.txt";
        const size_t culturesSuffixLength = strlen(culturesSuffix);
        return length >= culturesSuffixLength &&
            _stricmp(normalized + length - culturesSuffixLength, culturesSuffix) == 0;
    }

    bool IsLocalisationScriptPath(const char* path)
    {
        if (!path) return false;
        char normalized[MAX_PATH] = {};
        strncpy_s(normalized, sizeof(normalized), path, _TRUNCATE);
        for (char* p = normalized; *p; ++p)
        {
            if (*p == '\\') *p = '/';
            *p = (char)tolower((unsigned char)*p);
        }
        const size_t length = strlen(normalized);
        const bool countryFile = strstr(normalized, "common/countries/") != NULL &&
            length >= 4 && _stricmp(normalized + length - 4, ".txt") == 0;
        return countryFile || IsCulturesScriptPath(normalized) || IsEventOrDecisionScriptPath(normalized) ||
            IsOobUnitsScriptPath(normalized) || IsWarHistoryScriptPath(normalized);
    }

    VirtualCountryFile* FindVirtualCountryFile(void* handle)
    {
        if (!g_virtualFileLockReady || !handle) return NULL;
        for (VirtualCountryFile* file : g_virtualCountryFiles)
            if (file && file->handle == handle) return file;
        return NULL;
    }

    bool ReadPhysfsFile(const char* path, std::string& contents)
    {
        if (!g_realPhysfsOpenRead || !g_realPhysfsRead || !g_realPhysfsClose || !g_realPhysfsFileLength)
            return false;
        void* handle = g_realPhysfsOpenRead(path);
        if (!handle) return false;
        bool ok = false;
        const __int64 length = g_realPhysfsFileLength(handle);
        if (length >= 0 && length <= (16 << 20))
        {
            contents.resize((size_t)length);
            size_t readTotal = 0;
            while (readTotal < contents.size())
            {
                const unsigned request = (unsigned)std::min<size_t>(contents.size() - readTotal, 0x40000);
                const __int64 got = g_realPhysfsRead(handle, &contents[readTotal], 1, request);
                if (got <= 0) break;
                readTotal += (size_t)got;
            }
            ok = readTotal == contents.size();
        }
        g_realPhysfsClose(handle);
        if (!ok) contents.clear();
        return ok;
    }

    void* __cdecl PhysfsOpenReadHook(const char* path)
    {
        void* handle = g_realPhysfsOpenRead ? g_realPhysfsOpenRead(path) : NULL;
        if (!handle || g_loadingVirtualSource || !IsLocalisationScriptPath(path)) return handle;

        g_loadingVirtualSource = true;
        std::string original;
        const bool readOk = ReadPhysfsFile(path, original);
        g_loadingVirtualSource = false;
        if (!readOk) return handle;

        std::string transformed;
        const bool isCulturesFile = IsCulturesScriptPath(path);
        bool parsed = false;
        if (isCulturesFile)
            parsed = PreprocessCultureBlocks(original, g_activeLanguageId, transformed);
        else if (IsEventOrDecisionScriptPath(path))
        {
            parsed = PreprocessProvinceRenameEffects(original, g_activeLanguageId, transformed);
            if (parsed)
            {
                std::string localizedNames;
                parsed = PreprocessLocalizedLeaderNameBlocks(transformed, g_activeLanguageId, localizedNames);
                if (parsed) transformed.swap(localizedNames);
            }
        }
        else if (IsOobUnitsScriptPath(path))
            parsed = PreprocessLocalizedOobNames(original, g_activeLanguageId, transformed);
        else if (IsWarHistoryScriptPath(path))
            parsed = PreprocessLocalizedWarName(original, g_activeLanguageId, transformed);
        else
            parsed = PreprocessUnitNameBlocks(original, g_activeLanguageId, transformed);
        if (!parsed)
        {
            g_api->Log("LocalisationSwitcher: cannot parse localisation syntax in '%s'; using source file", path);
            return handle;
        }
        if (transformed == original) return handle;

        VirtualCountryFile* virtualFile = new (std::nothrow) VirtualCountryFile();
        if (!virtualFile) return handle;
        virtualFile->handle = handle;
        virtualFile->contents.assign(transformed.begin(), transformed.end());
        virtualFile->position = 0;
        EnterCriticalSection(&g_virtualFileLock);
        g_virtualCountryFiles.push_back(virtualFile);
        LeaveCriticalSection(&g_virtualFileLock);
        g_api->Log("LocalisationSwitcher: preprocessed localisation in '%s' for '%s'", path,
            g_activeLanguageId[0] ? g_activeLanguageId : "default");
        return handle;
    }

    __int64 __cdecl PhysfsReadHook(void* handle, void* buffer, unsigned size, unsigned count)
    {
        if (g_loadingVirtualSource || !g_virtualFileLockReady || size == 0)
            return g_realPhysfsRead ? g_realPhysfsRead(handle, buffer, size, count) : 0;

        EnterCriticalSection(&g_virtualFileLock);
        VirtualCountryFile* file = FindVirtualCountryFile(handle);
        if (!file)
        {
            LeaveCriticalSection(&g_virtualFileLock);
            return g_realPhysfsRead ? g_realPhysfsRead(handle, buffer, size, count) : 0;
        }
        const size_t available = file->contents.size() - std::min(file->position, file->contents.size());
        const size_t items = std::min<size_t>(count, available / size);
        const size_t bytes = items * size;
        if (bytes) memcpy(buffer, &file->contents[file->position], bytes);
        file->position += bytes;
        LeaveCriticalSection(&g_virtualFileLock);
        return (__int64)items;
    }

    __int64 __cdecl PhysfsTellHook(void* handle)
    {
        if (g_virtualFileLockReady)
        {
            EnterCriticalSection(&g_virtualFileLock);
            VirtualCountryFile* file = FindVirtualCountryFile(handle);
            if (file)
            {
                const __int64 position = (__int64)file->position;
                LeaveCriticalSection(&g_virtualFileLock);
                return position;
            }
            LeaveCriticalSection(&g_virtualFileLock);
        }
        return g_realPhysfsTell ? g_realPhysfsTell(handle) : -1;
    }

    __int64 __cdecl PhysfsFileLengthHook(void* handle)
    {
        if (g_virtualFileLockReady)
        {
            EnterCriticalSection(&g_virtualFileLock);
            VirtualCountryFile* file = FindVirtualCountryFile(handle);
            if (file)
            {
                const __int64 length = (__int64)file->contents.size();
                LeaveCriticalSection(&g_virtualFileLock);
                return length;
            }
            LeaveCriticalSection(&g_virtualFileLock);
        }
        return g_realPhysfsFileLength ? g_realPhysfsFileLength(handle) : -1;
    }

    int __cdecl PhysfsCloseHook(void* handle)
    {
        int result = g_realPhysfsClose ? g_realPhysfsClose(handle) : 0;
        if (g_virtualFileLockReady)
        {
            EnterCriticalSection(&g_virtualFileLock);
            for (auto it = g_virtualCountryFiles.begin(); it != g_virtualCountryFiles.end(); ++it)
            {
                if (*it && (*it)->handle == handle)
                {
                    delete *it;
                    g_virtualCountryFiles.erase(it);
                    break;
                }
            }
            LeaveCriticalSection(&g_virtualFileLock);
        }
        return result;
    }

    bool InstallPhysfsHook(uint32_t rva, unsigned normalSteal, const unsigned char* normalBytes,
        unsigned char* trampoline, void* hook, void** original, const char* tag)
    {
        unsigned char* entry = (unsigned char*)(g_api->gameBase + rva);
        if (entry[0] == 0xE9)
        {
            unsigned char expected[5];
            memcpy(expected, entry, sizeof(expected));
            const intptr_t oldDisplacement = *(int32_t*)(entry + 1);
            void* previousHook = entry + 5 + oldDisplacement;
            if (!g_api->StealToTrampoline(rva, 5, trampoline, 64, expected, hook, original, tag))
                return false;

            DWORD oldProtect = 0;
            if (!VirtualProtect(trampoline, 5, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
            *(int32_t*)(trampoline + 1) = (int32_t)((unsigned char*)previousHook - (trampoline + 5));
            VirtualProtect(trampoline, 5, oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(), trampoline, 5);
            g_api->Log("LocalisationSwitcher: chained %s through existing JMP hook", tag);
            return true;
        }
        return g_api->StealToTrampoline(rva, normalSteal, trampoline, 64, normalBytes,
            hook, original, tag) != 0;
    }

    bool InstallCountryFileHooks()
    {
        InitializeCriticalSection(&g_virtualFileLock);
        g_virtualFileLockReady = true;

        unsigned char closeExpected[8] = { 0x55, 0x8B, 0xEC, 0xA1, 0, 0, 0, 0 };
        const uint32_t mutexAddress = kPreferredPhysfsMutexAddress + (g_api->gameBase - 0x00400000);
        memcpy(closeExpected + 4, &mutexAddress, sizeof(mutexAddress));

        bool readOk = InstallPhysfsHook(kPhysfsReadRva, sizeof(kPhysfsReadBytes), kPhysfsReadBytes,
            g_physfsReadTramp, (void*)&PhysfsReadHook, (void**)&g_realPhysfsRead, "PhysfsRead(LocalisationSwitcher)");
        bool lengthOk = InstallPhysfsHook(kPhysfsFileLengthRva, sizeof(kPhysfsFileLengthBytes), kPhysfsFileLengthBytes,
            g_physfsLengthTramp, (void*)&PhysfsFileLengthHook, (void**)&g_realPhysfsFileLength, "PhysfsFileLength(LocalisationSwitcher)");
        bool tellOk = InstallPhysfsHook(kPhysfsTellRva, sizeof(kPhysfsTellBytes), kPhysfsTellBytes,
            g_physfsTellTramp, (void*)&PhysfsTellHook, (void**)&g_realPhysfsTell, "PhysfsTell(LocalisationSwitcher)");
        bool closeOk = InstallPhysfsHook(kPhysfsCloseRva, sizeof(closeExpected), closeExpected,
            g_physfsCloseTramp, (void*)&PhysfsCloseHook, (void**)&g_realPhysfsClose, "PhysfsClose(LocalisationSwitcher)");
        bool openOk = false;
        if (readOk && lengthOk && tellOk && closeOk)
            openOk = InstallPhysfsHook(kPhysfsOpenReadRva, sizeof(kPhysfsOpenReadBytes), kPhysfsOpenReadBytes,
                g_physfsOpenTramp, (void*)&PhysfsOpenReadHook, (void**)&g_realPhysfsOpenRead, "PhysfsOpenRead(LocalisationSwitcher)");

        if (readOk && lengthOk && tellOk && closeOk && openOk)
        {
            g_api->Log("LocalisationSwitcher: country-file VFS preprocessing hooks installed");
            return true;
        }
        g_api->Log("LocalisationSwitcher: country-file preprocessing unavailable (read=%d length=%d tell=%d close=%d open=%d)",
            readOk, lengthOk, tellOk, closeOk, openOk);
        return false;
    }

    void __fastcall SettingsLanguageSerializerHook(void* self, void* edx, void* output)
    {
        int previousSlot = g_serializingSlot;
        uint32_t selector = self ? *(uint32_t*)((unsigned char*)self + 8) : 0;
        g_serializingSlot = (selector >= 1 && selector <= 13) ? (int)selector : 0;
        if (g_originalSerializer)
            g_originalSerializer(self, output);
        g_serializingSlot = previousSlot;
    }

    void __cdecl ReplaceTokenStringAfterLookup(void* stringObject, uint32_t tokenId)
    {
        const int slot = g_serializingSlot - 1;
        if (!stringObject || slot < 0 || slot >= 13 ||
            tokenId != kNativeLanguageTokenIds[slot] || !g_slotNames[slot][0] ||
            _stricmp(g_slotNames[slot], kNativeLanguageIds[slot]) == 0)
            return;

        EngineString* text = (EngineString*)stringObject;
        const size_t length = strlen(g_slotNames[slot]);
        if (text->capacity > 15 && text->storage.heapText)
        {
            typedef void (__cdecl *TGameDelete)(void*);
            TGameDelete gameDelete = (TGameDelete)(g_api->gameBase + 0x006AE91B);
            gameDelete(text->storage.heapText);
        }

        if (length <= 15)
        {
            memset(text->storage.inlineText, 0, sizeof(text->storage.inlineText));
            memcpy(text->storage.inlineText, g_slotNames[slot], length);
            text->length = (uint32_t)length;
            text->capacity = 15;
            return;
        }

        typedef void* (__cdecl *TGameNew)(size_t);
        TGameNew gameNew = (TGameNew)(g_api->gameBase + 0x006AE9AF);
        char* storage = (char*)gameNew(length + 1);
        if (!storage)
        {
            text->storage.inlineText[0] = 0;
            text->length = 0;
            text->capacity = 15;
            return;
        }
        memcpy(storage, g_slotNames[slot], length + 1);
        text->storage.heapText = storage;
        text->length = (uint32_t)length;
        text->capacity = (uint32_t)length;
    }

    // FUN_00990880 receives its token index in EDI and a destination string in ESI.
    // Preserve that private register contract while allowing the serializer's value
    // token to use the configured identifier instead of the vanilla token spelling.
    extern "C" __declspec(naked) void* TokenStringLookupHook()
    {
        __asm
        {
            pushfd
            pushad
            mov eax, dword ptr [g_originalTokenLookup]
            call eax
            mov dword ptr [esp + 28], eax
            push edi
            push esi
            call ReplaceTokenStringAfterLookup
            add esp, 8
            popad
            popfd
            ret
        }
    }
}

extern "C" __declspec(dllexport) int __cdecl V2Mod_Init(const V2ModAPI* api)
{
    if (!api || api->apiVersion != V2MOD_API_VERSION || api->size < sizeof(V2ModAPI) ||
        !api->Log || !api->StealToTrampoline || !api->gameBase)
        return 0;

    g_api = api;
    LoadSlotConfiguration(api->modFolder);

    uint32_t* settingsVtable = (uint32_t*)(api->gameBase + kSettingsLanguageSerializerVtableRva);
    if (settingsVtable[kSettingsLanguageSerializerVtableSlot] != api->gameBase + kSettingsLanguageSerializerRva ||
        settingsVtable[4] != api->gameBase + kSettingsLanguageCallbackRva)
    {
        api->Log("LocalisationSwitcher: settings vtable does not match the researched Victoria II 3.04 layout");
        return 0;
    }

    if (!api->StealToTrampoline(kSettingsLanguageCallbackRva, kCallbackStealLength,
            g_trampoline, sizeof(g_trampoline), kExpectedCallbackBytes,
            (void*)&SettingsLanguageCallbackHook, (void**)&g_originalCallback,
            "LocalisationSwitcher.LanguageSetting"))
    {
        api->Log("LocalisationSwitcher: language callback hook failed; configuration not applied");
        return 0;
    }

    // The CSV-slot remap is implemented by the settings callback above. Token
    // lookup rewriting only preserves custom aliases when settings.txt is
    // serialized; a build-specific signature mismatch must not disable CSVs.
    unsigned char expectedTokenLookupBytes[sizeof(kExpectedTokenLookupBytes)];
    memcpy(expectedTokenLookupBytes, kExpectedTokenLookupBytes, sizeof(expectedTokenLookupBytes));
    const uint32_t relocatedGlobalAddress = kTokenLookupGlobalPreferredAddress +
        (api->gameBase - kPreferredImageBase);
    memcpy(expectedTokenLookupBytes + 6, &relocatedGlobalAddress, sizeof(relocatedGlobalAddress));

    const bool tokenLookupInstalled = api->StealToTrampoline(kTokenStringLookupRva,
            sizeof(expectedTokenLookupBytes), g_tokenLookupTrampoline,
            sizeof(g_tokenLookupTrampoline), expectedTokenLookupBytes,
            (void*)&TokenStringLookupHook, &g_originalTokenLookup,
            "LocalisationSwitcher.TokenStringLookup") != 0;
    if (!tokenLookupInstalled)
        api->Log("LocalisationSwitcher: token string lookup hook unavailable; CSV slot mapping remains active");

    InstallRenameRegionTooltipGuard();

    if (!api->PatchSlot(kSettingsLanguageSerializerVtableRva, kSettingsLanguageSerializerVtableSlot,
            (void*)&SettingsLanguageSerializerHook, (void**)&g_originalSerializer))
    {
        api->Log("LocalisationSwitcher: settings language serializer hook unavailable; CSV slot mapping remains active");
        return 1;
    }

    api->Log("LocalisationSwitcher: installed language parse and CSV-slot mapping hooks%s",
        tokenLookupInstalled ? " with settings serialization support" : "");
    InstallCountryFileHooks();
    return 1;
}
