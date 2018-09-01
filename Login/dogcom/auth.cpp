#include <cstring>
#include <cstdio>
#include <ctime>
#include <cstdint>
#include <chrono>

#include <winsock2.h>
typedef int socklen_t;

#include "auth.h"
#include "configparse.h"
#include "keepalive.h"
#include "md5.h"

#include "login.h"
#include "../LogUtil.h"

#define BIND_PORT 61440
#define DEST_PORT 61440

// 输出日志
void print_packet(const char msg[10], const unsigned char *packet, int length) {
	static char buf[1024];
	wsprintfA(buf, "%s", msg);
	char *str = buf + strlen(buf);
	for (int i = 0; i < length; i++) {
		wsprintfA(str + 2 * i, "%02x", packet[i]);
	}
	*(str + 2 * length) = '\0';
	logiA("%s\n", buf);
}

int dhcp_challenge(int sockfd, struct sockaddr_in addr, unsigned char seed[]) {
	unsigned char challenge_packet[20], recv_packet[1024];
	memset(challenge_packet, 0, 20);
	challenge_packet[0] = 0x01;
	challenge_packet[1] = 0x02;
	challenge_packet[2] = rand() & 0xff;
	challenge_packet[3] = rand() & 0xff;
	challenge_packet[4] = drcom_config.AUTH_VERSION[0];

	sendto(sockfd, (const char*)challenge_packet, 20, 0, (struct sockaddr *)&addr, sizeof(addr));

	print_packet("[Challenge sent] ", challenge_packet, 20);

	socklen_t addrlen = sizeof(addr);
	if (recvfrom(sockfd, (char*)recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) < 0) {
		get_lasterror("Failed to recv data");
		return 1;
	}

	print_packet("[Challenge recv] ", recv_packet, 76);

	if (recv_packet[0] != 0x02) {
		logiA("Bad challenge response received.\n");
		return 1;
	}
	else {
		// 收到的报文中有ip地址
		for (int i = 0; i < 4; i++) {
			receivedIp[i] = recv_packet[20 + i];
		}
		SendMessage(FindWindow(szHiddenName, szHiddenTitle), WM_USER_GET_IP, 0, 0);
	}

	memcpy(seed, &recv_packet[4], 4);

	return 0;
}


int dhcp_login(int sockfd, struct sockaddr_in addr, unsigned char seed[], unsigned char auth_information[]) {
	unsigned int login_packet_size;
	unsigned int length_padding = 0;
	int JLU_padding = 0;

	if (strlen(drcom_config.password) > 8) {
		length_padding = strlen(drcom_config.password) - 8 + (length_padding % 2);
		logiA("Start JLU mode.\n");
		if (strlen(drcom_config.password) != 16) {
			JLU_padding = strlen(drcom_config.password) / 4;
		}
		length_padding = 28 + (strlen(drcom_config.password) - 8) + JLU_padding;
	}
	login_packet_size = 338 + length_padding;
	unsigned char *login_packet = new unsigned char[login_packet_size];
	// checksum1[8]改为checksum1[16]
	unsigned char recv_packet[1024], MD5A[16], MACxorMD5A[6], MD5B[16], checksum1[16], checksum2[4];
	memset(login_packet, 0, login_packet_size);
	memset(recv_packet, 0, 100);

	// build login-packet
	login_packet[0] = 0x03;
	login_packet[1] = 0x01;
	login_packet[2] = 0x00;
	login_packet[3] = strlen(drcom_config.username) + 20;
	int MD5A_len = 6 + strlen(drcom_config.password);
	unsigned char *MD5A_str = new unsigned char[MD5A_len];
	MD5A_str[0] = 0x03;
	MD5A_str[1] = 0x01;
	memcpy(MD5A_str + 2, seed, 4);
	memcpy(MD5A_str + 6, drcom_config.password, strlen(drcom_config.password));
	MD5(MD5A_str, MD5A_len, MD5A);
	memcpy(login_packet + 4, MD5A, 16);
	memcpy(login_packet + 20, drcom_config.username, strlen(drcom_config.username));
	memcpy(login_packet + 56, &drcom_config.CONTROLCHECKSTATUS, 1);
	memcpy(login_packet + 57, &drcom_config.ADAPTERNUM, 1);
	uint64_t sum = 0;
	uint64_t mac = 0;
	// unpack
	for (int i = 0; i < 6; i++) {
		sum = (int)MD5A[i] + sum * 256;
	}
	// unpack
	for (int i = 0; i < 6; i++) {
		mac = (int)drcom_config.mac[i] + mac * 256;
	}
	sum ^= mac;
	// pack
	for (int i = 6; i > 0; i--) {
		MACxorMD5A[i - 1] = (unsigned char)(sum % 256);
		sum /= 256;
	}
	memcpy(login_packet + 58, MACxorMD5A, sizeof(MACxorMD5A));
	int MD5B_len = 9 + strlen(drcom_config.password);
	unsigned char *MD5B_str = new unsigned char[MD5B_len];
	memset(MD5B_str, 0, MD5B_len);
	MD5B_str[0] = 0x01;
	memcpy(MD5B_str + 1, drcom_config.password, strlen(drcom_config.password));
	memcpy(MD5B_str + strlen(drcom_config.password) + 1, seed, 4);
	MD5(MD5B_str, MD5B_len, MD5B);
	memcpy(login_packet + 64, MD5B, 16);
	login_packet[80] = 0x01;
	unsigned char host_ip[4];
	sscanf(drcom_config.host_ip, "%hhd.%hhd.%hhd.%hhd",
		&host_ip[0],
		&host_ip[1],
		&host_ip[2],
		&host_ip[3]);
	memcpy(login_packet + 81, host_ip, 4);
	unsigned char checksum1_str[101], checksum1_tmp[4] = { 0x14, 0x00, 0x07, 0x0b };
	memcpy(checksum1_str, login_packet, 97);
	memcpy(checksum1_str + 97, checksum1_tmp, 4);
	MD5(checksum1_str, 101, checksum1);
	memcpy(login_packet + 97, checksum1, 8);
	memcpy(login_packet + 105, &drcom_config.IPDOG, 1);
	memcpy(login_packet + 110, &drcom_config.host_name, strlen(drcom_config.host_name));
	unsigned char PRIMARY_DNS[4];
	sscanf(drcom_config.PRIMARY_DNS, "%hhd.%hhd.%hhd.%hhd",
		&PRIMARY_DNS[0],
		&PRIMARY_DNS[1],
		&PRIMARY_DNS[2],
		&PRIMARY_DNS[3]);
	memcpy(login_packet + 142, PRIMARY_DNS, 4);
	unsigned char dhcp_server[4];
	sscanf(drcom_config.dhcp_server, "%hhd.%hhd.%hhd.%hhd",
		&dhcp_server[0],
		&dhcp_server[1],
		&dhcp_server[2],
		&dhcp_server[3]);
	memcpy(login_packet + 146, dhcp_server, 4);
	unsigned char OSVersionInfoSize[4] = { 0x94 };
	unsigned char OSMajor[4] = { 0x05 };
	unsigned char OSMinor[4] = { 0x01 };
	unsigned char OSBuild[4] = { 0x28, 0x0a };
	unsigned char PlatformID[4] = { 0x02 };
	OSVersionInfoSize[0] = 0x94;
	OSMajor[0] = 0x06;
	OSMinor[0] = 0x02;
	OSBuild[0] = 0xf0;
	OSBuild[1] = 0x23;
	PlatformID[0] = 0x02;
	unsigned char ServicePack[40] = { 0x33, 0x64, 0x63, 0x37, 0x39, 0x66, 0x35, 0x32, 0x31, 0x32, 0x65, 0x38, 0x31, 0x37,
		0x30, 0x61, 0x63, 0x66, 0x61, 0x39, 0x65, 0x63, 0x39, 0x35, 0x66, 0x31, 0x64, 0x37,
		0x34, 0x39, 0x31, 0x36, 0x35, 0x34, 0x32, 0x62, 0x65, 0x37, 0x62, 0x31 };
	unsigned char hostname[9] = { 0x44, 0x72, 0x43, 0x4f, 0x4d, 0x00, 0xcf, 0x07, 0x68 };
	memcpy(login_packet + 182, hostname, 9);
	memcpy(login_packet + 246, ServicePack, 40);
	memcpy(login_packet + 162, OSVersionInfoSize, 4);
	memcpy(login_packet + 166, OSMajor, 4);
	memcpy(login_packet + 170, OSMinor, 4);
	memcpy(login_packet + 174, OSBuild, 4);
	memcpy(login_packet + 178, PlatformID, 4);
	memcpy(login_packet + 310, drcom_config.AUTH_VERSION, 2);
	int counter = 312;
	unsigned int ror_padding = 0;
	if (strlen(drcom_config.password) <= 8) {
		ror_padding = 8 - strlen(drcom_config.password);
	}
	else {
		if ((strlen(drcom_config.password) - 8) % 2) { ror_padding = 1; }
		ror_padding = JLU_padding;
	}
	MD5(MD5A_str, MD5A_len, MD5A);
	login_packet[counter + 1] = strlen(drcom_config.password);
	counter += 2;
	for (int i = 0, x = 0; i < strlen(drcom_config.password); i++) {
		x = (int)MD5A[i] ^ (int)drcom_config.password[i];
		login_packet[counter + i] = (unsigned char)(((x << 3) & 0xff) + (x >> 5));
	}
	counter += strlen(drcom_config.password);
	login_packet[counter] = 0x02;
	login_packet[counter + 1] = 0x0c;
	unsigned char *checksum2_str = new unsigned char[counter + 18]; // [counter + 14 + 4]
	memset(checksum2_str, 0, counter + 18);
	unsigned char checksum2_tmp[6] = { 0x01, 0x26, 0x07, 0x11 };
	memcpy(checksum2_str, login_packet, counter + 2);
	memcpy(checksum2_str + counter + 2, checksum2_tmp, 6);
	memcpy(checksum2_str + counter + 8, drcom_config.mac, 6);
	sum = 1234;
	uint64_t ret = 0;
	for (int i = 0; i < counter + 14; i += 4) {
		ret = 0;
		// reverse unsigned char array[4]
		for (int j = 4; j > 0; j--) {
			ret = ret * 256 + (int)checksum2_str[i + j - 1];
		}
		sum ^= ret;
	}
	sum = (1968 * sum) & 0xffffffff;
	for (int j = 0; j < 4; j++) {
		checksum2[j] = (unsigned char)(sum >> (j * 8) & 0xff);
	}
	memcpy(login_packet + counter + 2, checksum2, 4);
	memcpy(login_packet + counter + 8, drcom_config.mac, 6);
	login_packet[counter + ror_padding + 14] = 0xe9;
	login_packet[counter + ror_padding + 15] = 0x13;
	login_packet[counter + ror_padding + 14] = 0x60;
	login_packet[counter + ror_padding + 15] = 0xa2;

	// 第二个参数由 sizeof(login_packet) 改为 login_packet_size
	sendto(sockfd, (const char*)login_packet, login_packet_size, 0, (struct sockaddr *)&addr, sizeof(addr));

	print_packet("[Login sent] ", login_packet, sizeof(login_packet));

	socklen_t addrlen = sizeof(addr);
	if (recvfrom(sockfd, (char*)recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) < 0) {
		get_lasterror("Failed to recv data");
		return UNKNOWN_ERROR;
	}

	if (recv_packet[0] != 0x04) {
		print_packet("[login recv] ", recv_packet, 100);
		logiA("<<< Login failed >>>\n");
		char err_msg[256];
		if (recv_packet[0] == 0x05) {
			// 返回登录失败的原因
			return recv_packet[4];
		}
		return UNKNOWN_ERROR;
	}
	else {
		print_packet("[login recv] ", recv_packet, 100);
		logiA("<<< Logged in >>>\n");
	}

	memcpy(auth_information, &recv_packet[23], 16);

	if (recvfrom(sockfd, (char*)recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) >= 0) {
		logiA(("Get notice packet."));
	}

	delete[] checksum2_str;
	delete[] login_packet;
	delete[] MD5A_str;
	delete[] MD5B_str;

	return 0;
}



int dogcom() {
	WORD sockVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(sockVersion, &wsaData) != 0) {
		return INIT_ERROR;
	}
	int sockfd;

	// 发送方地址
	struct sockaddr_in bind_addr;
	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sin_family = AF_INET;
	logiA("You are binding at %s!\n\n", drcom_config.host_ip);
	bind_addr.sin_addr.S_un.S_addr = inet_addr(drcom_config.host_ip);
	bind_addr.sin_port = htons(BIND_PORT);

	// 目标地址
	struct sockaddr_in dest_addr;
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_addr.S_un.S_addr = inet_addr(drcom_config.server);
	dest_addr.sin_port = htons(DEST_PORT);

	srand((unsigned int)time(nullptr));

	// create socket
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		get_lasterror("Failed to create socket");
		closesocket(sockfd);
		WSACleanup();
		return CREATE_SOCKET;
	}

	// bind socket
	if (bind(sockfd, (struct sockaddr *) &bind_addr, sizeof(bind_addr)) < 0) {
		get_lasterror("Failed to bind socket");
		closesocket(sockfd);
		WSACleanup();
		return BIND_SOCKET;
	}

	// set timeout
	int timeout = 3000;
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
		get_lasterror("Failed to set sock opt");
		closesocket(sockfd);
		WSACleanup();
		return SET_SOCK_OPT;
	}

	// start dogcoming
	unsigned char seed[4];
	unsigned char auth_information[16];
	// 登录 先challenge 然后login
	if (dhcp_challenge(sockfd, dest_addr, seed)) {
		// challenge失败的情况
		closesocket(sockfd);
		WSACleanup();
		return CHALLENGE_ERROR;
	}
	else {
		if (!sleeper.wait_for(std::chrono::milliseconds(200))) { // 0.2 sec
			closesocket(sockfd);
			WSACleanup();
			return USER_TERMINATED;
		}
		int loginResult = dhcp_login(sockfd, dest_addr, seed, auth_information);
		// challenge成功 尝试登录
		if (!loginResult) {
			// 设置当前状态为已登录
			status = LOGGEDIN;
			logd(TEXT("status状态变为LOGGEDIN"));
			// 通知窗口登录成功了
			SendMessage(FindWindow(szHiddenName, szHiddenTitle), WM_USER_LOGIN_SUCCEED, 0, 0);
			int keepalive_counter = 0;
			int keepalive_try_counter = 0;
			int first = 1;
			while (1) {
				if (!keepalive_1(sockfd, dest_addr, seed, auth_information)) {
					if (!sleeper.wait_for(std::chrono::milliseconds(200))) { // 0.2 sec
						closesocket(sockfd);
						WSACleanup();
						return USER_TERMINATED;
					}
					if (keepalive_2(sockfd, dest_addr, &keepalive_counter, &first, 0)) {
						// keepalive_2失败 重试keepalive_1
						continue;
					}
					logiA("Keepalive in loop.\n");
					if (!sleeper.wait_for(std::chrono::milliseconds(20000))) { // 20 sec
						closesocket(sockfd);
						WSACleanup();
						return USER_TERMINATED;
					}
				}
				else {
					// keepalive_1失败
					if (keepalive_try_counter > 5) {
						break;
					}
					keepalive_try_counter++;
					continue;
				}
			}
		}
		else {
			// 登录失败
			closesocket(sockfd);
			WSACleanup();
			return loginResult;
		};
	}

	closesocket(sockfd);
	WSACleanup();
	return UNKNOWN_ERROR;
}



void get_lasterror(const char *msg) {
	char err_msg[256];
	err_msg[0] = '\0';
	FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		WSAGetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		err_msg,
		sizeof(err_msg),
		nullptr);
	logeA("%s: %s", msg, err_msg);
}
