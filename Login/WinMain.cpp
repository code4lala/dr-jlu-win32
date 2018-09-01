#include <windows.h>
#include <cstdio>

#include "dogcom\login.h"
#include "resource.h"
#include "LogUtil.h"
const char * szDebugPrefixA = "Login.cpp";
const WCHAR * szDebugPrefixW = TEXT("Login.cpp");

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProcHidden(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK WndProcChild(HWND, UINT, WPARAM, LPARAM);
WNDPROC procEditAccount;
WNDPROC procEditPassword;
WNDPROC procEditMac;
WNDPROC procCheckRemember;
WNDPROC procCheckAuto;
WNDPROC procButtonLogin;
int g_dpix = 96, g_dpiy = 96;
HMENU g_idFocus = (HMENU)0;
TCHAR szBuffer[4096];
char strBuf[1024];
int status = OFFLINE;
bool fFirstShowLoginWindow = true;
char strConfFileName[] = ".\\Login.ini";

// 登录信息
struct tagLogInfo logInfo;
// 收到的报文中的ip地址
unsigned char receivedIp[4];

// 托盘消息
static const TCHAR*		szTaskbarTip = TEXT("Drcom客户端");
static const int		iTaskbarUid = 1;
NOTIFYICONDATA	nid;

// 两个窗口类 第一个为登录窗口的窗口类 第二个为显示托盘图标的窗口类
WNDCLASSEX  wndClassEx, wndclassHidden;
const TCHAR szAppName[] = TEXT("DrClient");
const TCHAR szTitle[] = TEXT("Dr客户端");
const TCHAR szHiddenName[] = TEXT("HiddenParentWindow");
const TCHAR szHiddenTitle[] = TEXT("Dr客户端隐藏父窗口");

static int  iHwndLoginCount = 0;
static bool fMin = false;

// 四个标签 三个输入框 两个多选按钮 一个普通按钮
HWND			hwndTagAccount, hwndTagPassword, hwndTagMac, hwndTagIp;
HWND			hwndEditAccount, hwndEditPassword, hwndEditMac;
HWND			hwndCheckRemember, hwndCheckAuto;
HWND			hwndButtonLogin;
const HMENU		idTagAccount = (HMENU)1;
const HMENU		idTagPassword = (HMENU)2;
const HMENU     idTagMac = (HMENU)3;
const HMENU     idTagIp = (HMENU)4;
const HMENU		idEditAccount = (HMENU)5;
const HMENU		idEditPassword = (HMENU)6;
const HMENU     idEditMac = (HMENU)7;
const HMENU		idCheckRemember = (HMENU)8;
const HMENU		idCheckAuto = (HMENU)9;
const HMENU		idButtonLogin = (HMENU)10;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	PSTR szCmdLine, int iCmdShow)
{
	// 警告明文存储
	GetPrivateProfileStringA("conf", "warning", "true", strBuf, sizeof(strBuf), strConfFileName);
	if (strBuf[0] == 't') {
		MessageBox(nullptr, TEXT("该程序保存密码选项采取明文存储方式！"), TEXT("警告！"), MB_ICONWARNING);
		WritePrivateProfileStringA("conf", "warning", "false", strConfFileName);
		MessageBox(nullptr, TEXT("mac地址请填12位小写字母和数字的组合"), TEXT("提示"), MB_ICONINFORMATION);
	}

	// 开启DPI缩放
	FARPROC spdpia = GetProcAddress(GetModuleHandle(TEXT("user32")), "SetProcessDPIAware");
	if (spdpia) spdpia();
	// 获取系统当前DPI
	HDC hdc = GetDC(nullptr);
	if (hdc) {
		g_dpix = GetDeviceCaps(hdc, LOGPIXELSX);
		g_dpiy = GetDeviceCaps(hdc, LOGPIXELSY);
		ReleaseDC(nullptr, hdc);
	}
	logd(TEXT("g_dpix = %d, g_dpiy = %d"), g_dpix, g_dpiy);

	MSG msg;
	HWND hwndTaskBar;

	// 生成一个隐藏的父窗口
	wndclassHidden = {
		sizeof(WNDCLASSEX),
		CS_ENABLE,
		WndProcHidden,
		0,
		0,
		hInstance,
		LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_JLU)),
		LoadCursor(nullptr, IDC_ARROW),
		(HBRUSH)GetStockObject(WHITE_BRUSH),
		nullptr,
		szHiddenName,
		LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_JLU_SM))
	};
	if (!RegisterClassEx(&wndclassHidden)) {
		MessageBox(nullptr, TEXT("This program requires Windows NT!"),
			szTitle, MB_ICONERROR);
		return 1;
	}
	hwndTaskBar = CreateWindow(
		szHiddenName,
		szHiddenTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		nullptr,
		nullptr,
		hInstance,
		nullptr
	);

	// 设置最小化到托盘
	nid.cbSize = sizeof(nid);
	nid.hWnd = hwndTaskBar;
	nid.uID = iTaskbarUid;
	nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nid.uCallbackMessage = WM_USER;
	nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_JLU));
	memcpy(nid.szTip, szTaskbarTip, lstrlen(szTaskbarTip) * sizeof(TCHAR));
	Shell_NotifyIcon(NIM_ADD, &nid);

	ShowWindow(hwndTaskBar, SW_HIDE);
	UpdateWindow(hwndTaskBar);
	while (GetMessage(&msg, nullptr, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// 删除托盘图标
	Shell_NotifyIcon(NIM_DELETE, &nid);

	return msg.wParam;
}

char getCharFromInt(int x) {
	if (x <= 9 && x >= 0) return (char)(x + '0');
	return (char)(x - 10 + 'a');
}
int getIntFromChar(char x) {
	if (x <= '9' && x >= '0') return x - '0';
	return x - 'a' + 10;
}
void getEditText() {
	char mac[24];
	GetWindowTextA(hwndEditAccount, logInfo.account, sizeof(logInfo.account));
	GetWindowTextA(hwndEditPassword, logInfo.pass, sizeof(logInfo.pass));
	GetWindowTextA(hwndEditMac, mac, 12 + 1); // 最后一个字符是 '\0' 所以要多加一位空
	for (int i = 0; i < 6; i++) {
		logInfo.mac[i] = (unsigned char)(getIntFromChar(mac[2 * i]) * 16 + getIntFromChar(mac[2 * i + 1]));
	}
}
void save_pass() {
	WritePrivateProfileStringA("conf", "account", logInfo.account, strConfFileName);
	logdA("saved account: %s", logInfo.account);
	WritePrivateProfileStringA("conf", "password", logInfo.pass, strConfFileName);
	logdA("saved psss: %s", logInfo.pass);
	for (int i = 0; i < 6; i++) {
		strBuf[i * 2] = getCharFromInt(logInfo.mac[i] >> 4);
		strBuf[i * 2 + 1] = getCharFromInt(logInfo.mac[i] & 0xf);
	}
	strBuf[12] = logInfo.account[strlen(logInfo.account)];
	WritePrivateProfileStringA("conf", "mac", strBuf, strConfFileName);
	logdA("saved mac: %s", strBuf);
}
void clear_pass() {
	WritePrivateProfileStringA("conf", "account", "", strConfFileName);
	WritePrivateProfileStringA("conf", "password", "", strConfFileName);
	WritePrivateProfileStringA("conf", "mac", "", strConfFileName);
	WritePrivateProfileStringA("check", "remember", "false", strConfFileName);
	WritePrivateProfileStringA("check", "auto", "false", strConfFileName);
}
void load_pass() {
	GetPrivateProfileStringA("conf", "account", "", logInfo.account, sizeof(logInfo.account), strConfFileName);
	logdA("load account from file: %s", logInfo.account);
	GetPrivateProfileStringA("conf", "password", "", logInfo.pass, sizeof(logInfo.pass), strConfFileName);
	logdA("load pass from file: %s", logInfo.pass);
	GetPrivateProfileStringA("conf", "mac", "", strBuf, sizeof(strBuf), strConfFileName);
	logdA("load mac from file: %s", strBuf);
	for (int i = 0; i < 6; i++) {
		logInfo.mac[i] = (unsigned char)((getIntFromChar(strBuf[2 * i]) << 4)
						| getIntFromChar(strBuf[2 * i + 1]));
		logdA("%02x", logInfo.mac[i]);
	}
}

LRESULT CALLBACK WndProcHidden(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
	static HINSTANCE	hInstance;
	static HBRUSH		hbrush = CreateSolidBrush(RGB(240, 240, 240));
	POINT				point;
	HMENU				hmenu;
	static bool			fDidNotHandle = false;
	static LRESULT		lresult;
	static bool			remember, autoLogin;
	HWND				hwndLogin;

	switch (message) {
	case WM_CREATE:
		// 初始化 hInstance
		hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

		// 注册正常的登录窗口的窗口类
		wndClassEx.cbSize = sizeof(WNDCLASSEX);
		wndClassEx.style = CS_HREDRAW | CS_VREDRAW;
		wndClassEx.lpfnWndProc = WndProc;
		wndClassEx.cbClsExtra = 0;
		wndClassEx.cbWndExtra = 0;
		wndClassEx.hInstance = hInstance;
		wndClassEx.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_JLU));
		wndClassEx.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wndClassEx.hbrBackground = hbrush;
		wndClassEx.lpszMenuName = nullptr;
		wndClassEx.lpszClassName = szAppName;
		wndClassEx.hIconSm = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON_JLU_SM));
		if (!RegisterClassEx(&wndClassEx))
		{
			MessageBox(nullptr, TEXT("This program requires Windows NT!"),
				szTitle, MB_ICONERROR);
			return 1;
		}
		SendMessage(hwnd, WM_USER_SHOWLOGINWINDOW, 0, 0);

		break;

	case WM_USER_SHOWLOGINWINDOW:
		logd(TEXT("iHwndLoginCount = %d"), iHwndLoginCount);
		if (iHwndLoginCount > 0) {
			ShowWindow(FindWindow(szAppName, szTitle), SW_SHOW);
			SetFocus(FindWindow(szAppName, szTitle));
			break;
		}
		hwndLogin = CreateWindow(
			szAppName,					// window class name 窗口类名
			szTitle,					// window caption 窗口标题
			WS_SYSMENU | WS_MINIMIZEBOX,
			CW_USEDEFAULT,              // initial x position
			CW_USEDEFAULT,              // initial y position
			MulDiv(400, g_dpix, 96),
			MulDiv(300, g_dpiy, 96),
			nullptr,					// parent window handle
			nullptr,                    // window menu handle
			hInstance,                  // program instance handle
			nullptr);                   // creation parameters
		iHwndLoginCount++;
		ShowWindow(hwndLogin, SW_SHOW);
		SetFocus(FindWindow(szAppName, szTitle));
		GetPrivateProfileStringA("check", "remember", "false", strBuf, sizeof(strBuf), strConfFileName);
		logdA("load remember from file: %s", strBuf);
		SendMessage(GetDlgItem(FindWindow(szAppName, szTitle), (int)idCheckRemember), BM_SETCHECK,
			strBuf[0] == 't' ? true : false, 0);
		if (strBuf[0] == 't') {
			SendMessage(FindWindow(szAppName, szTitle), WM_USER_FILL_BLANKS, 0, 0);
		}
		GetPrivateProfileStringA("check", "auto", "false", strBuf, sizeof(strBuf), strConfFileName);
		logdA("load auto login from file: %s", strBuf);
		SendMessage(GetDlgItem(FindWindow(szAppName, szTitle), (int)idCheckAuto), BM_SETCHECK,
			strBuf[0] == 't' ? true : false, 0);
		if (status == LOGGEDIN) {
			SendMessage(FindWindow(szAppName, szTitle), WM_USER_DISABLE_BUTTON, 0, 0);
		}
		if (fFirstShowLoginWindow && strBuf[0] == 't') {
			fFirstShowLoginWindow = false;
			SendMessage(hwnd, WM_USER_LOGIN, 0, 0);
		}
		break;

	case WM_USER_HIDELOGINWINDOW:
		logd(TEXT("收到了隐藏主窗口的消息"));
		logd(TEXT("发送了关闭登录窗口的消息"));
		SendMessage(FindWindow(szAppName, szTitle), WM_CLOSE, 0, 0);
		break;

	case WM_USER_QUIT:
		if (fMin) {
			iHwndLoginCount--;
			hwndLogin = nullptr;
			fMin = false;
			break;
		}
		logd(TEXT("向隐藏窗口发送了WM_CLOSE消息"));
		SendMessage(hwnd, WM_CLOSE, 0, 0);
		break;

	case WM_DESTROY:
		logd(TEXT("隐藏窗口收到了WM_DESTORY消息"));
		DeleteObject(hbrush);
		PostQuitMessage(0);
		break;

	case WM_COMMAND:
		switch (wParam) {
		case IDM_TASKBARMENU_SHOW:
			SendMessage(hwnd, WM_USER_SHOWLOGINWINDOW, 0, 0);
			break;
		case IDM_TASKBARMENU_LOGOUT:
			// 终止后台线程
			logout();
			SendMessage(hwnd, WM_USER_SHOWLOGINWINDOW, 0, 0);
			SendMessage(FindWindow(szAppName, szTitle), WM_USER_DISABLE_BUTTON, 0, 0);
			break;
		case IDM_TASKBARMENU_EXIT:
			SendMessage(hwnd, WM_CLOSE, 0, 0);
			break;
		}
		break;

	case WM_USER_LOGIN:
		// 用户敲了回车或者点击了登录
		logd(TEXT("开始登录"));
		login(logInfo.account, logInfo.pass, logInfo.mac);
		lresult = SendMessage(GetDlgItem(FindWindow(szAppName, szTitle), (int)idCheckRemember), BM_GETCHECK, 0, 0);
		WritePrivateProfileStringA("check", "remember",
			lresult == BST_CHECKED ? "true" : "false", strConfFileName);
		logd(TEXT("写入配置文件记住密码: %s"), lresult == BST_CHECKED ? TEXT("true") : TEXT("false"));
		lresult = SendMessage(GetDlgItem(FindWindow(szAppName, szTitle), (int)idCheckAuto), BM_GETCHECK, 0, 0);
		WritePrivateProfileStringA("check", "auto",
			lresult == BST_CHECKED ? "true" : "false", strConfFileName);
		logd(TEXT("写入配置文件自动登录: %s"), lresult == BST_CHECKED ? TEXT("true") : TEXT("false"));
		SendMessage(FindWindow(szAppName, szTitle), WM_USER_DISABLE_BUTTON, 0, 0);
		break;

	case WM_USER_LOGIN_FAILED:
		// 登录失败，提示用户，如果是账号或密码错误则清除已保存的密码
		MessageBox(nullptr, err_msg, TEXT("已离线"), 0);
		SendMessage(FindWindow(szAppName, szTitle), WM_USER_ENABLE_BUTTON, 0, 0);
		SendMessage(FindWindow(szAppName, szTitle), WM_USER_CLEAR_IP, 0, 0);
		if (!lstrcmp(err_msg, TEXT("[Tips] 账号或密码错误"))) {
			logi(TEXT("账号或密码错误，已清除保存的密码"));
			clear_pass();
		}
		break;

	case WM_USER_LOGIN_SUCCEED:
		// 登录成功，显示通知网页
		ShellExecuteA(nullptr, "open", "http://login.jlu.edu.cn/notice.php", nullptr, nullptr, SW_SHOWNORMAL);
		// 保存密码
		save_pass();
		break;

	case WM_USER_GET_IP:
		// 显示当前IP
		wsprintf(szBuffer, TEXT("IP: %d.%d.%d.%d"), receivedIp[0], receivedIp[1], receivedIp[2], receivedIp[3]);
		SetWindowText(hwndTagIp, szBuffer);
		break;

	case WM_CLOSE:
		if (iHwndLoginCount > 0) {
			// 假装最小化关闭窗口
			fMin = true;
			SendMessage(FindWindow(szAppName, szTitle), WM_CLOSE, 0, 0);
		}
		fDidNotHandle = true;
		break;

	case WM_USER:
		// 如果不是托盘消息退出
		if (wParam != iTaskbarUid) break;
		switch (lParam) {
		case WM_LBUTTONDBLCLK:
			logd(TEXT("托盘左键双击"));
			SendMessage(hwnd, WM_USER_SHOWLOGINWINDOW, 0, 0);
			break;
		case WM_RBUTTONUP:
			logd(TEXT("托盘右键单击"));
			GetCursorPos(&point);
			hmenu = LoadMenu(hInstance, MAKEINTRESOURCE(IDM_TASKBARMENU));
			hmenu = GetSubMenu(hmenu, 0);
			if (status == LOGGEDIN) {
				logd(TEXT("已登录，启用注销按钮"));
				EnableMenuItem(hmenu, IDM_TASKBARMENU_LOGOUT, MF_ENABLED);
			}
			else {
				logd(TEXT("未登录，禁用注销按钮"));
				EnableMenuItem(hmenu, IDM_TASKBARMENU_LOGOUT, MF_DISABLED | MF_GRAYED);
			}
			// SetForegroundWindow可以鼠标点击不是菜单的地方取消显示菜单
			SetForegroundWindow(hwnd);
			TrackPopupMenu(hmenu, TPM_RIGHTBUTTON, point.x, point.y,
				0, hwnd, nullptr);
			break;
		default:
			break;
		}
		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	if (fDidNotHandle) 
		return DefWindowProc(hwnd, message, wParam, lParam);
	else
		return 0;
}

LRESULT CALLBACK WndProcChild(HWND hwnd, UINT message,
	WPARAM wParam, LPARAM lParam) {
	HMENU id = (HMENU)GetWindowLong(hwnd, GWL_ID);
	// logd(TEXT("当前焦点: %d"), id);
	bool fPressTab = false;

	switch (message) {
	case WM_SETFOCUS:
		g_idFocus = id;
		break;

	case WM_KEYDOWN:
		if (wParam == VK_TAB) {
			fPressTab = true;
			if (GetKeyState(VK_SHIFT) < 0) {
				// 按下了 Shift + Tab 键
				g_idFocus = (HMENU)max((int)g_idFocus - 1, 5);
				logd(TEXT("按下了 Shift + Tab 键 焦点: %d"), g_idFocus);
			}
			else {
				// 按下了 Tab 键
				g_idFocus = (HMENU)min((int)g_idFocus + 1, 10);
				logd(TEXT("按下了 Tab 键 焦点: %d"), g_idFocus);
			}
			// 选择相应的子窗口作为当前焦点
			SetFocus(GetDlgItem(GetParent(hwnd), (int)g_idFocus));
		}
		else if (wParam == VK_RETURN) {
			logd(TEXT("敲了回车"));
			getEditText();
			SendMessage(FindWindow(szHiddenName, szHiddenTitle), WM_USER_LOGIN, 0, 0);
		}
		break;
	}


	// 返回语句部分
	if (fPressTab) {
		logd(TEXT("按下了tab键"));
		return 0;
	}
	if (id == idEditAccount) {
		//logd(TEXT("idEditAccount 的消息循环"));
		return CallWindowProc(procEditAccount,
			hwnd, message, wParam, lParam);
	}
	else if (id == idEditPassword) {
		//logd(TEXT("idEditPassword 的消息循环"));
		return CallWindowProc(procEditPassword,
			hwnd, message, wParam, lParam);
	}
	else if (id == idEditMac) {
		return CallWindowProc(procEditMac,
			hwnd, message, wParam, lParam);
	}
	else if (id == idCheckRemember) {
		//logd(TEXT("idCheckRemember 的消息循环"));
		return CallWindowProc(procCheckRemember,
			hwnd, message, wParam, lParam);
	}
	else if (id == idCheckAuto) {
		//logd(TEXT("idCheckAuto 的消息循环"));
		return CallWindowProc(procCheckAuto,
			hwnd, message, wParam, lParam);
	}
	else if (id == idButtonLogin) {
		//logd(TEXT("idButtonLogin 的消息循环"));
		return CallWindowProc(procButtonLogin,
			hwnd, message, wParam, lParam);
	}
	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	HDC					hdc;
	PAINTSTRUCT			ps;
	RECT				rect;
	static TEXTMETRIC	textMetric;
	static int			cxChar, cyChar, cxClient, cyClient;
	static HFONT		hfont;
	HFONT				hfontHold;
	static HINSTANCE    hInstance;
	static HICON		hicon;
	int					tx, ty;

	static bool fDidNotHandle = false;

	switch (message)
	{
	case WM_CREATE:
		// 重新设置字体，以应用字体的DPI缩放
		SendMessage(hwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
		// 创建适应高分屏的字体
		hfont = CreateFont(
			-MulDiv(10/*字号*/, g_dpiy, 72),		// cHeight
			0,									// cWidth
			0,									// cEscapement
			0,									// cOrientation
			FW_THIN,							// cWeight
			FALSE,								// bItalic
			FALSE,								// bUnderline
			FALSE,								// bStrikeOut
			DEFAULT_CHARSET,					// iCharSet
			OUT_DEFAULT_PRECIS,					// iOutPrecision
			CLIP_DEFAULT_PRECIS,				// iClipPrecision
			DEFAULT_QUALITY,					// iQuality
			FF_DONTCARE,						// iPitchAndFamily
			TEXT("微软雅黑")						// pszFaceName
		);

		// 获取系统字体信息
		hdc = GetDC(hwnd);
		GetTextMetrics(hdc, &textMetric);
		cxChar = textMetric.tmAveCharWidth;
		cyChar = textMetric.tmHeight + textMetric.tmExternalLeading;

		// 保存实例句柄
		hInstance = ((LPCREATESTRUCT)lParam)->hInstance;

		// 创建界面上的子窗口
		hwndTagAccount = CreateWindow(TEXT("static"), TEXT("账号"),
			WS_CHILD | WS_VISIBLE | SS_CENTER,
			0, 0, 0, 0,
			hwnd, idTagAccount,
			hInstance, nullptr);
		hwndTagPassword = CreateWindow(TEXT("static"), TEXT("密码"),
			WS_CHILD | WS_VISIBLE | SS_CENTER,
			0, 0, 0, 0,
			hwnd, idTagPassword,
			hInstance, nullptr);
		hwndTagMac = CreateWindow(TEXT("static"), TEXT("MAC地址"),
			WS_CHILD | WS_VISIBLE | SS_CENTER,
			0, 0, 0, 0,
			hwnd, idTagMac,
			hInstance, nullptr);
		hwndTagIp = CreateWindow(TEXT("static"), TEXT("IP : 0.0.0.0"),
			WS_CHILD | WS_VISIBLE | SS_LEFT,
			0, 0, 0, 0,
			hwnd, idTagIp,
			hInstance, nullptr);
		hwndEditAccount = CreateWindow(TEXT("edit"), nullptr,
			WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_LOWERCASE,
			0, 0, 0, 0,
			hwnd, idEditAccount,
			hInstance, nullptr);
		hwndEditPassword = CreateWindow(TEXT("edit"), nullptr,
			WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_PASSWORD,
			0, 0, 0, 0,
			hwnd, idEditPassword,
			hInstance, nullptr);
		hwndEditMac = CreateWindow(TEXT("edit"), nullptr,
			WS_CHILD | WS_VISIBLE | WS_BORDER | ES_LEFT | ES_LOWERCASE,
			0, 0, 0, 0,
			hwnd, idEditMac,
			hInstance, nullptr);
		hwndCheckRemember = CreateWindow(TEXT("button"), TEXT("记住密码"),
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
			0, 0, 0, 0,
			hwnd, idCheckRemember,
			hInstance, nullptr);
		hwndCheckAuto = CreateWindow(TEXT("button"), TEXT("自动登录"),
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | WS_TABSTOP,
			0, 0, 0, 0,
			hwnd, idCheckAuto,
			hInstance, nullptr);
		hwndButtonLogin = CreateWindow(TEXT("button"), TEXT("登录"),
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
			0, 0, 0, 0,
			hwnd, idButtonLogin,
			hInstance, nullptr);
		
		// 设置子窗口的窗口过程函数
		procEditAccount = (WNDPROC)SetWindowLong(hwndEditAccount,
			GWL_WNDPROC, (LONG)WndProcChild);
		procEditPassword = (WNDPROC)SetWindowLong(hwndEditPassword,
			GWL_WNDPROC, (LONG)WndProcChild);
		procEditMac = (WNDPROC)SetWindowLong(hwndEditMac,
			GWL_WNDPROC, (LONG)WndProcChild);
		procCheckRemember = (WNDPROC)SetWindowLong(hwndCheckRemember,
			GWL_WNDPROC, (LONG)WndProcChild);
		procCheckAuto = (WNDPROC)SetWindowLong(hwndCheckAuto,
			GWL_WNDPROC, (LONG)WndProcChild);
		procButtonLogin = (WNDPROC)SetWindowLong(hwndButtonLogin,
			GWL_WNDPROC, (LONG)WndProcChild);

		ReleaseDC(hwnd, hdc);
		break;

	case WM_SIZE:
		GetClientRect(hwnd, &rect);
		cxClient = rect.right;
		cyClient = rect.bottom;
		tx = cxClient / 20; ty = cyClient / 20;
		logd(TEXT("cxClient = %d, cyClient = %d"), cxClient, cyClient);
		logd(TEXT("tx = %d, ty = %d"), tx, ty);
		// 调整各个子窗口位置和大小
		MoveWindow(hwndTagAccount, tx * 2, ty * 2, tx * 3, ty * 2, false);
		MoveWindow(hwndTagPassword, tx * 2, ty * 5, tx * 3, ty * 2, false);
		MoveWindow(hwndTagMac, tx * 2, ty * 8, tx * 3, ty * 2, false);
		MoveWindow(hwndEditAccount, tx * 7, ty * 2, tx * 10, ty * 2, false);
		MoveWindow(hwndEditPassword, tx * 7, ty * 5, tx * 10, ty * 2, false);
		MoveWindow(hwndEditMac, tx * 7, ty * 8, tx * 10, ty * 2, false);
		MoveWindow(hwndCheckRemember, tx * 4, ty * 11, tx * 5, ty * 2, false);
		MoveWindow(hwndCheckAuto, tx * 10, ty * 11, tx * 5, ty * 2, false);
		MoveWindow(hwndTagIp, tx * 7, ty * 14, tx * 12, ty * 2, false);
		MoveWindow(hwndButtonLogin, tx * 5, ty * 17, tx * 10, ty * 2, false);
		// 调整高分屏字体
		SendMessage(hwndTagAccount, WM_SETFONT, (WPARAM)hfont, true);
		SendMessage(hwndTagPassword, WM_SETFONT, (WPARAM)hfont, true);
		SendMessage(hwndTagMac, WM_SETFONT, (WPARAM)hfont, true);
		SendMessage(hwndEditAccount, WM_SETFONT, (WPARAM)hfont, true);
		SendMessage(hwndEditPassword, WM_SETFONT, (WPARAM)hfont, true);
		SendMessage(hwndEditMac, WM_SETFONT, (WPARAM)hfont, true);
		SendMessage(hwndCheckRemember, WM_SETFONT, (WPARAM)hfont, true);
		SendMessage(hwndCheckAuto, WM_SETFONT, (WPARAM)hfont, true);
		SendMessage(hwndTagIp, WM_SETFONT, (WPARAM)hfont, true);
		SendMessage(hwndButtonLogin, WM_SETFONT, (WPARAM)hfont, true);

		break;

	case WM_USER_DISABLE_BUTTON:
		EnableWindow(hwndEditAccount, false);
		EnableWindow(hwndEditPassword, false);
		EnableWindow(hwndEditMac, false);
		EnableWindow(hwndCheckAuto, false);
		EnableWindow(hwndCheckRemember, false);
		EnableWindow(hwndButtonLogin, false);
		break;

	case WM_USER_ENABLE_BUTTON:
		EnableWindow(hwndEditAccount, true);
		EnableWindow(hwndEditPassword, true);
		EnableWindow(hwndEditMac, true);
		EnableWindow(hwndCheckAuto, true);
		EnableWindow(hwndCheckRemember, true);
		EnableWindow(hwndButtonLogin, true);
		break;

	case WM_USER_CLEAR_IP:
		SetWindowTextA(hwndTagIp, "IP: 0.0.0.0");
		break;

	case WM_SETFOCUS:
		if ((int)g_idFocus < 5 || (int)g_idFocus > 10) {
			g_idFocus = (HMENU)5;
			logd(TEXT("修改了焦点为5"));
		}
		if (g_idFocus == idEditAccount) {
			SetFocus(hwndEditAccount);
		}
		else if (g_idFocus == idEditPassword) {
			SetFocus(hwndEditPassword);
		}
		else if (g_idFocus == idEditMac) {
			SetFocus(hwndEditMac);
		}
		else if (g_idFocus == idCheckRemember) {
			SetFocus(hwndCheckRemember);
		}
		else if (g_idFocus == idCheckAuto) {
			SetFocus(hwndCheckAuto);
		}
		else if (g_idFocus == idButtonLogin) {
			SetFocus(hwndButtonLogin);
		}
		break;

	case WM_USER_FILL_BLANKS:
		// 填充账号密码
		load_pass();
		SetWindowTextA(hwndEditAccount, logInfo.account);
		SetWindowTextA(hwndEditPassword, logInfo.pass);
		for (int i = 0; i < 6; i++) {
			strBuf[i * 2] = getCharFromInt(logInfo.mac[i] >> 4);
			strBuf[i * 2 + 1] = getCharFromInt(logInfo.mac[i] & 0xf);
		}
		strBuf[12] = '\0';
		logdA("fill mac::::: %s", strBuf);
		SetWindowTextA(hwndEditMac, strBuf);
		if (status == LOGGEDIN) {
			wsprintf(szBuffer, TEXT("IP: %d.%d.%d.%d"), receivedIp[0], receivedIp[1], receivedIp[2], receivedIp[3]);
			SetWindowText(hwndTagIp, szBuffer);
		}
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == (WORD)(int)idCheckAuto) {
			logd(TEXT("点击了自动登录"));
			LRESULT r = SendMessage(
				GetDlgItem(hwnd, (int)idCheckAuto),
				BM_GETCHECK, 0, 0);
			if (r == BST_CHECKED) {
				logd(TEXT("当前自动登录为选中状态"));
				SendMessage(
					GetDlgItem(hwnd, (int)idCheckRemember),
					BM_SETCHECK, true, 0);
			}
		}
		else if (LOWORD(wParam) == (WORD)(int)idCheckRemember) {
			logd(TEXT("点击了记住密码"));
			LRESULT r = SendMessage(
				GetDlgItem(hwnd, (int)idCheckRemember),
				BM_GETCHECK, 0, 0);
			if (r == BST_UNCHECKED) {
				logd(TEXT("当前状态为不记住密码"));
				SendMessage(
					GetDlgItem(hwnd, (int)idCheckAuto),
					BM_SETCHECK, false, 0);
			}
		}
		else if (LOWORD(wParam) == (WORD)(int)idButtonLogin) {
			logd(TEXT("点击了登录按钮"));
			getEditText();
			SendMessage(FindWindow(szHiddenName, szHiddenTitle), WM_USER_LOGIN, 0, 0);
		}
		break;

	case WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);
		GetClientRect(hwnd, &rect);
		logd(TEXT("rect 上%d 下%d 左%d 右%d"), rect.top, rect.bottom, rect.left, rect.right);

		hfontHold = (HFONT)SelectObject(hdc, hfont);

		SelectObject(hdc, hfontHold);

		EndPaint(hwnd, &ps);
		break;

	case WM_SYSCOMMAND:
		switch (wParam) {
		case SC_MINIMIZE:
			logd(TEXT("收到了最小化消息 不应该退出"));
			fMin = true;
			SendMessage(FindWindow(szHiddenName, szHiddenTitle), WM_USER_HIDELOGINWINDOW, 0, 0);
			break;
		default:
			break;
		}
		fDidNotHandle = true;
		break;

	case WM_CLOSE:
		logd(TEXT("收到了关闭登录窗口的消息"));
		fDidNotHandle = true;
		break;

	case WM_DESTROY:
		DeleteObject(hfont);
		fDidNotHandle = true;
		// PostQuitMessage(0);
		SendMessage(FindWindow(szHiddenName, szHiddenTitle), WM_USER_QUIT, 0, 0);
		break;

	default:
		return DefWindowProc(hwnd, message, wParam, lParam);
	}
	if (fDidNotHandle)
		return DefWindowProc(hwnd, message, wParam, lParam);
	else
		return 0;
}
