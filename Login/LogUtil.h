#ifndef LOGUTIL_H
#define LOGUTIL_H

void logd(const TCHAR * szFormat, ...);
void logi(const TCHAR * szFormat, ...);
void loge(const TCHAR * szFormat, ...);
void logdA(const char * szFormat, ...);
void logiA(const char * szFormat, ...);
void logeA(const char * szFormat, ...);
//void logdW(const TCHAR * szFormat, ...);
//void logiW(const TCHAR * szFormat, ...);
//void logeW(const TCHAR * szFormat, ...);
extern const char * szDebugPrefixA;
extern const WCHAR * szDebugPrefixW;

#endif // !LOGUTIL_H
