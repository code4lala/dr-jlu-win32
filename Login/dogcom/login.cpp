#include <csignal>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <cstring>
#include "windows.h"
#include "auth.h"
#include "configparse.h"
#include "../LogUtil.h"
#include "login.h"

struct config drcom_config;
TCHAR *err_msg;
HANDLE hLoginThread;
InterruptibleSleeper sleeper;

DWORD WINAPI threadLogin(LPVOID lpParameter) {
	// 返回值非0即为登录失败
	status = LOGGING;
	logd(TEXT("status状态变为LOGGING"));
	int result = dogcom();
	logd(TEXT("\nresult = %d\n"), result);
	switch (result) {
	case 0:
		break;
	case CHECK_MAC:
		err_msg = (TCHAR*)TEXT("[Tips] 有人正在用该MAC地址使用此账号");
		break;
	case SERVER_BUSY:
		err_msg = (TCHAR*)TEXT("[Tips] 服务器繁忙，请重试");
		break;
	case WRONG_PASS:
		err_msg = (TCHAR*)TEXT("[Tips] 账号或密码错误");
		break;
	case NOT_ENOUGH:
		err_msg = (TCHAR*)TEXT("[Tips] 该账号使用时长或流量超限");
		break;
	case FREEZE_UP:
		err_msg = (TCHAR*)TEXT("[Tips] 该账号被冻结");
		break;
	case NOT_ON_THIS_IP:
		err_msg = (TCHAR*)TEXT("[Tips] IP地址不匹配，该账号只能用于指定IP");
		break;
	case NOT_ON_THIS_MAC:
		err_msg = (TCHAR*)TEXT("[Tips] MAC地址不匹配，该账号只能用于指定的MAC地址");
		break;
	case TOO_MUCH_IP:
		err_msg = (TCHAR*)TEXT("[Tips] 该账号对应多个IP地址");
		break;
	case UPDATE_CLIENT:
		err_msg = (TCHAR*)TEXT("[Tips] 客户端版本号错误，请升级客户端");
		break;
	case NOT_ON_THIS_IP_MAC:
		err_msg = (TCHAR*)TEXT("[Tips] 该账号只可用于指定的IP和MAC地址");
		break;
	case MUST_USE_DHCP:
		err_msg = (TCHAR*)TEXT("[Tips] 请将计算机的TCP/IPv4属性设置为DHCP");
		break;
	case INIT_ERROR:
		err_msg = (TCHAR*)TEXT("[Tips] 程序初始化失败");
		break;
	case CREATE_SOCKET:
		err_msg = (TCHAR*)TEXT("[Tips] 创建socket失败");
		break;
	case BIND_SOCKET:
		err_msg = (TCHAR*)TEXT("[Tips] 绑定socket失败 请检查是否有其他客户端占用了端口");
		break;
	case SET_SOCK_OPT:
		err_msg = (TCHAR*)TEXT("[Tips] 设置socket选项失败");
		break;
	case CHALLENGE_ERROR:
		err_msg = (TCHAR*)TEXT("[Tips] 与服务器握手失败");
		break;
	case USER_TERMINATED:
		err_msg = (TCHAR*)TEXT("[Tips] 用户注销");
		break;
	case UNKNOWN_ERROR:
	default:
		err_msg = (TCHAR*)TEXT("[Tips] 未知错误");
		break;
	}
	if (!result)
		logd(TEXT("%s"), err_msg);
	// 只要是返回过来了一定是失败了
	status = OFFLINE;
	logd(TEXT("status状态变为OFFLINE"));
	SendMessage(FindWindow(szHiddenName, szHiddenTitle), WM_USER_LOGIN_FAILED, 0, 0);
	return 0;
}

void login(const char *account, const char *password, const unsigned char mac[6]) {
	fillConfig(account, password, mac);
	logd(TEXT("填充完成"));
	sleeper.reset();

	// 创建一个线程并挂起该线程
	hLoginThread = CreateThread(nullptr, 0, threadLogin, nullptr, CREATE_SUSPENDED, nullptr);
	// 设置该线程的优先级
	SetThreadPriority(hLoginThread, THREAD_PRIORITY_HIGHEST);
	// 执行该线程
	ResumeThread(hLoginThread);
}

void logout() {
	sleeper.interrupt();
}