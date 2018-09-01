#ifndef AUTH_H_
#define AUTH_H_

enum {
	// Dr协议部分
	CHECK_MAC = 0x01,
	SERVER_BUSY = 0x02,
	WRONG_PASS = 0x03,
	NOT_ENOUGH = 0x04,
	FREEZE_UP = 0x05,
	NOT_ON_THIS_IP = 0x07,
	NOT_ON_THIS_MAC = 0x0B,
	TOO_MUCH_IP = 0x14,
	UPDATE_CLIENT = 0x15,
	NOT_ON_THIS_IP_MAC = 0x16,
	MUST_USE_DHCP = 0x17,

	// 自定义部分
	UNKNOWN_ERROR = 0x18,

	// dogcom()部分
	INIT_ERROR		 = 0x19, // WSAStartup(sockVersion, &wsaData) != 0失败
	CREATE_SOCKET	 = 0x20, // create socket失败
	BIND_SOCKET		 = 0x21, // bind socket失败
	SET_SOCK_OPT	 = 0x22, // set sock opt失败
	CHALLENGE_ERROR	 = 0x23, // challenge失败
	USER_TERMINATED  = 0x24  // 用户取消操作
};

//  输出日志
void print_packet(const char msg[10], const unsigned char *packet, int length);

int dhcp_login(int sockfd, struct sockaddr_in addr, unsigned char seed[], unsigned char auth_information[]);
int dogcom();
void get_lasterror(const char *msg);

#endif // AUTH_H_