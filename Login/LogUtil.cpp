#include <windows.h>
#include <tchar.h>
#include <cstdio>
#include <cstdarg>
#include "LogUtil.h"

#ifdef UNICODE
const WCHAR * szDebugPrefix = szDebugPrefixW;
#elif
const char * szDebugPrefix = szDebugPrefixA;
#endif

static TCHAR szBuffer[1024];
static TCHAR szB2[1024];
static char strBuf[1024];
static char strB2[1024];
static SYSTEMTIME st;

void CDECL loge(const TCHAR * szFormat, ...) {
	va_list pArgList;
	va_start(pArgList, szFormat);
#ifdef UNICODE
	_vsnwprintf_s(szBuffer, sizeof(szBuffer) / sizeof(TCHAR), szFormat, pArgList);
#elif
	_vsnprintf_s(strBuf, sizeof(szBuffer) / sizeof(TCHAR), szFormat, pArgList);
#endif
	va_end(pArgList);

	GetLocalTime(&st);

	wsprintf(szB2, TEXT("[ERROR] %02d:%02d:%02d.%03d [%d] %s %s\n"),
		// current time
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		// process id
		GetCurrentProcessId(),
		// debug log
		szDebugPrefix, szBuffer);
	OutputDebugString(szB2);
}

void CDECL logi(const TCHAR * szFormat, ...) {
	va_list pArgList;
	va_start(pArgList, szFormat);
#ifdef UNICODE
	_vsnwprintf_s(szBuffer, sizeof(szBuffer) / sizeof(TCHAR), szFormat, pArgList);
#elif
	_vsnprintf_s(strBuf, sizeof(szBuffer) / sizeof(TCHAR), szFormat, pArgList);
#endif
	va_end(pArgList);

	GetLocalTime(&st);

	wsprintf(szB2, TEXT("[INFO] %02d:%02d:%02d.%03d [%d] %s %s\n"),
		// current time
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		// process id
		GetCurrentProcessId(),
		// debug log
		szDebugPrefix, szBuffer);
	OutputDebugString(szB2);
}

#ifndef _DEBUG
void CDECL logd(const TCHAR * szFormat, ...) { }
void CDECL logdA(const char * szFormat, ...) { }
#else
void CDECL logd(const TCHAR * szFormat, ...) {
	va_list pArgList;
	va_start(pArgList, szFormat);
#ifdef UNICODE
	_vsnwprintf_s(szBuffer, sizeof(szBuffer) / sizeof(TCHAR), szFormat, pArgList);
#elif
	_vsnprintf_s(strBuf, sizeof(szBuffer) / sizeof(TCHAR), szFormat, pArgList);
#endif
	va_end(pArgList);

	GetLocalTime(&st);

	wsprintf(szB2, TEXT("[DEBUG] %02d:%02d:%02d.%03d [%d] %s %s\n"),
		// current time
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		// process id
		GetCurrentProcessId(),
		// debug log
		szDebugPrefix, szBuffer);
	OutputDebugString(szB2);
}

void CDECL logdA(const char * szFormat, ...) {
	va_list pArgList;
	va_start(pArgList, szFormat);
	_vsnprintf_s(strBuf, sizeof(strBuf) / sizeof(char), szFormat, pArgList);
	va_end(pArgList);

	GetLocalTime(&st);

	sprintf(strB2, "[INFO] %02d:%02d:%02d.%03d [%d] %s %s\n",
		// current time
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		// process id
		GetCurrentProcessId(),
		// debug log
		szDebugPrefixA, strBuf);
	OutputDebugStringA(strB2);
}
#endif // _DEBUG



void CDECL logeA(const char * szFormat, ...) {
	va_list pArgList;
	va_start(pArgList, szFormat);
	_vsnprintf_s(strBuf, sizeof(strBuf) / sizeof(char), szFormat, pArgList);
	va_end(pArgList);

	GetLocalTime(&st);

	sprintf(strB2, "[ERROR] %02d:%02d:%02d.%03d [%d] %s %s\n",
		// current time
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		// process id
		GetCurrentProcessId(),
		// debug log
		szDebugPrefixA, strBuf);
	OutputDebugStringA(strB2);
}

void CDECL logiA(const char * szFormat, ...) {
	va_list pArgList;
	va_start(pArgList, szFormat);
	_vsnprintf_s(strBuf, sizeof(strBuf) / sizeof(char), szFormat, pArgList);
	va_end(pArgList);

	GetLocalTime(&st);

	sprintf(strB2, "[INFO] %02d:%02d:%02d.%03d [%d] %s %s\n",
		// current time
		st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
		// process id
		GetCurrentProcessId(),
		// debug log
		szDebugPrefixA, strBuf);
	OutputDebugStringA(strB2);
}
