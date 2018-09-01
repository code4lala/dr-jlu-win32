#ifndef LOGIN_H
#define LOGIN_H
#include <csignal>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <chrono>

void login(const char *account, const char *password, const unsigned char mac[6]);
void logout();

struct tagLogInfo {
	char account[48];
	char pass[48];
	unsigned char mac[6];
};

class InterruptibleSleeper {
public:
	// returns false if killed:
	template<typename R, typename P>
	bool wait_for(std::chrono::duration<R, P> const& time) {
		std::unique_lock<std::mutex> lock(m);
		return !cv.wait_for(lock, time, [&] {return terminate; });
	}
	void interrupt() {
		std::unique_lock<std::mutex> lock(m);
		terminate = true;
		cv.notify_all();
	}
	void reset() {
		terminate = false;
	}
private:
	std::condition_variable cv;
	std::mutex m;
	bool terminate = false;
};

extern InterruptibleSleeper sleeper;

extern struct tagLogInfo logInfo;
extern int status;
extern TCHAR *err_msg;
extern unsigned char receivedIp[4];

enum {
	OFFLINE = 1,
	LOGGING = 2,
	LOGGEDIN = 3
};

extern const TCHAR szAppName[];
extern const TCHAR szTitle[];
extern const TCHAR szHiddenName[];
extern const TCHAR szHiddenTitle[];

#define WM_USER_SHOWLOGINWINDOW		(WM_USER + 1)
#define WM_USER_HIDELOGINWINDOW		(WM_USER + 2)
#define WM_USER_QUIT				(WM_USER + 3)
#define WM_USER_LOGIN				(WM_USER + 4)
#define WM_USER_LOGIN_FAILED		(WM_USER + 5)
#define WM_USER_LOGIN_SUCCEED		(WM_USER + 6)
#define WM_USER_DISABLE_BUTTON		(WM_USER + 7)
#define WM_USER_ENABLE_BUTTON		(WM_USER + 8)
#define WM_USER_GET_IP				(WM_USER + 9)
#define WM_USER_FILL_BLANKS			(WM_USER + 10)
#define WM_USER_CLEAR_IP			(WM_USER + 11)

#endif // LOGIN_H
