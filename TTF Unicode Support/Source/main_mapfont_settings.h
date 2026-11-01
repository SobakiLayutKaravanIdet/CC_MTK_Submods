#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>

struct MainMapFontSettings {
    bool active = false;
    wchar_t userTtfFile[MAX_PATH] = L"";
    wchar_t reserveFontFile[MAX_PATH] = L"";
    BYTE colorR = 0;
    BYTE colorG = 0;
    BYTE colorB = 0;
    int height = 56;
    int smallHeight = 22;
};
struct ProvinceNameMapFontSettings {
    bool active = false;
    wchar_t userTtfFile[MAX_PATH] = L"";
    BYTE colorR = 0;
    BYTE colorG = 0;
    BYTE colorB = 0;
};

static inline void TrimSetting(char* value) {
    size_t n=strlen(value);
    while(n&&(value[n-1]==' '||value[n-1]=='\t'||value[n-1]=='\r'||value[n-1]=='\n'))value[--n]=0;
    char* p=value;while(*p==' '||*p=='\t')++p;if(p!=value)memmove(value,p,strlen(p)+1);
    n=strlen(value);if(n>=2&&value[0]=='"'&&value[n-1]=='"'){memmove(value,value+1,n-2);value[n-2]=0;}
}
static inline bool SettingUtf8ToWide(const char* src,wchar_t* dst,int capacity){
    return src&&*src&&dst&&capacity>=2&&MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,src,-1,dst,capacity)>0;
}
static inline bool SafeRelativeFontPath(const wchar_t* path){
    if(!path||!*path||path[0]==L'\\'||path[0]==L'/'||wcschr(path,L':'))return false;
    for(const wchar_t* p=path;*p;++p)if(*p==L'.'&&p[1]==L'.'&&(p==path||p[-1]==L'\\'||p[-1]==L'/')&&(p[2]==0||p[2]==L'\\'||p[2]==L'/'))return false;
    return true;
}
static inline bool ParseMapFontHeight(const char* text,int& out){
    if(!text||!*text)return false;char* end=nullptr;long v=strtol(text,&end,10);
    while(end&&(*end==' '||*end=='\t'))++end;if(!end||*end||v<4||v>256)return false;out=int(v);return true;
}
static inline bool ParseMapFontColor(const char* text,BYTE& r,BYTE& g,BYTE& b){
    if(!text||!*text)return false;char* p=const_cast<char*>(text);long values[3]={};
    for(int i=0;i<3;++i){while(*p==' '||*p=='\t')++p;if(!*p)return false;char* end=nullptr;long v=strtol(p,&end,10);if(end==p||v<0||v>255)return false;values[i]=v;p=end;}
    while(*p==' '||*p=='\t')++p;if(*p)return false;r=BYTE(values[0]);g=BYTE(values[1]);b=BYTE(values[2]);return true;
}
static inline bool IsSettingBom(const char* line){return (unsigned char)line[0]==0xEF&&(unsigned char)line[1]==0xBB&&(unsigned char)line[2]==0xBF;}
static inline FILE* OpenTtfSettings(const char* modFolder){
    if(!modFolder||!*modFolder)return nullptr;char path[MAX_PATH]={};
    if(_snprintf_s(path,sizeof(path),_TRUNCATE,"%s\\TTF Unicode Support\\ttf_settings.txt",modFolder)<0)return nullptr;
    FILE* f=nullptr;return fopen_s(&f,path,"rb")==0?f:nullptr;
}
static inline bool ParseMapFontActive(const char* value,bool& active){
    if(_stricmp(value,"yes")==0||_stricmp(value,"true")==0||strcmp(value,"1")==0){active=true;return true;}
    if(_stricmp(value,"no")==0||_stricmp(value,"false")==0||strcmp(value,"0")==0){active=false;return true;}
    return false;
}
static inline bool ReadSettingLine(FILE* f,char* line,size_t capacity){
    while(fgets(line,int(capacity),f)){
        if(IsSettingBom(line))memmove(line,line+3,strlen(line+3)+1);
        char* comment=strpbrk(line,"#;");if(comment)*comment=0;TrimSetting(line);if(*line)return true;
    }return false;
}
static inline void LoadMainMapFontSettings(const char* modFolder,MainMapFontSettings& out){
    FILE* f=OpenTtfSettings(modFolder);if(!f)return;char line[512];bool in=false,found=false;MainMapFontSettings parsed;
    while(ReadSettingLine(f,line,sizeof(line))){
        if(!in){if(_strnicmp(line,"main_mapfont",12)==0&&strchr(line,'{')){in=true;found=true;}continue;}
        if(strchr(line,'}'))break;char* eq=strchr(line,'=');if(!eq)continue;*eq++=0;TrimSetting(line);TrimSetting(eq);
        if(_stricmp(line,"active")==0){ParseMapFontActive(eq,parsed.active);continue;}
        if(_stricmp(line,"user_ttf_file")==0){wchar_t v[MAX_PATH]={};if(SettingUtf8ToWide(eq,v,MAX_PATH)&&SafeRelativeFontPath(v))wcsncpy_s(parsed.userTtfFile,v,_TRUNCATE);continue;}
        if(_stricmp(line,"color")==0){ParseMapFontColor(eq,parsed.colorR,parsed.colorG,parsed.colorB);continue;}
        if(_stricmp(line,"reserve_font_file")==0){if(_stricmp(eq,"none")==0){parsed.reserveFontFile[0]=0;continue;}wchar_t v[MAX_PATH]={};if(SettingUtf8ToWide(eq,v,MAX_PATH)&&SafeRelativeFontPath(v))wcsncpy_s(parsed.reserveFontFile,v,_TRUNCATE);continue;}
        if(_stricmp(line,"height")==0){ParseMapFontHeight(eq,parsed.height);continue;}
        if(_stricmp(line,"small_height")==0)ParseMapFontHeight(eq,parsed.smallHeight);
    }
    fclose(f);if(found)out=parsed;
}
static inline void LoadProvinceNameMapFontSettings(const char* modFolder,ProvinceNameMapFontSettings& out){
    FILE* f=OpenTtfSettings(modFolder);if(!f)return;char line[512];bool in=false,found=false;ProvinceNameMapFontSettings parsed;
    while(ReadSettingLine(f,line,sizeof(line))){
        if(!in){if(_strnicmp(line,"provincename_mapfont",20)==0&&strchr(line,'{')){in=true;found=true;}continue;}
        if(strchr(line,'}'))break;char* eq=strchr(line,'=');if(!eq)continue;*eq++=0;TrimSetting(line);TrimSetting(eq);
        if(_stricmp(line,"active")==0){ParseMapFontActive(eq,parsed.active);continue;}
        if(_stricmp(line,"user_ttf_file")==0){wchar_t v[MAX_PATH]={};if(SettingUtf8ToWide(eq,v,MAX_PATH)&&SafeRelativeFontPath(v))wcsncpy_s(parsed.userTtfFile,v,_TRUNCATE);}
        if(_stricmp(line,"color")==0){ParseMapFontColor(eq,parsed.colorR,parsed.colorG,parsed.colorB);}
    }
    fclose(f);if(found)out=parsed;
}