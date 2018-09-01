#include <cstring>
#include <cstdio>
#include <cstdlib>

#include <winsock2.h>
typedef int socklen_t;

#include "keepalive.h"
#include "configparse.h"
#include "auth.h"
#include "md5.h"
#include "md4.h"
#include "sha1.h"

#include "../LogUtil.h"

void gen_crc(unsigned char seed[], int encrypt_type, unsigned char crc[]) {
	if (encrypt_type == 0) {
		char DRCOM_DIAL_EXT_PROTO_CRC_INIT[4] = { (char)0xc7, (char)0x2f, (char)0x31, (char)0x01 };
		char gencrc_tmp[4] = { 0x7e };
		memcpy(crc, DRCOM_DIAL_EXT_PROTO_CRC_INIT, 4);
		memcpy(crc + 4, gencrc_tmp, 4);
	}
	else if (encrypt_type == 1) {
		unsigned char hash[32] = { 0 };
		MD5(seed, 4, hash);
		crc[0] = hash[2];
		crc[1] = hash[3];
		crc[2] = hash[8];
		crc[3] = hash[9];
		crc[4] = hash[5];
		crc[5] = hash[6];
		crc[6] = hash[13];
		crc[7] = hash[14];
	}
	else if (encrypt_type == 2) {
		unsigned char hash[32] = { 0 };
		MD4(seed, 4, hash);
		crc[0] = hash[1];
		crc[1] = hash[2];
		crc[2] = hash[8];
		crc[3] = hash[9];
		crc[4] = hash[4];
		crc[5] = hash[5];
		crc[6] = hash[11];
		crc[7] = hash[12];
	}
	else if (encrypt_type == 3) {
		unsigned char hash[32] = { 0 };
		SHA1(seed, 4, hash);
		crc[0] = hash[2];
		crc[1] = hash[3];
		crc[2] = hash[9];
		crc[3] = hash[10];
		crc[4] = hash[5];
		crc[5] = hash[6];
		crc[6] = hash[15];
		crc[7] = hash[16];
	}
}


int keepalive_1(int sockfd, struct sockaddr_in addr, unsigned char seed[], unsigned char auth_information[]) {
	unsigned char keepalive_1_packet1[8] = { 0x07, 0x01, 0x08, 0x00, 0x01, 0x00, 0x00, 0x00 };
	unsigned char recv_packet1[1024], keepalive_1_packet2[38], recv_packet2[1024];
	memset(keepalive_1_packet2, 0, 38);
	sendto(sockfd, (const char *)keepalive_1_packet1, 8, 0, (struct sockaddr *) &addr, sizeof(addr));
	print_packet("[Keepalive1 sent] ", keepalive_1_packet1, 42);
	socklen_t addrlen = sizeof(addr);
	while (1) {
		if (recvfrom(sockfd, (char *)recv_packet1, 1024, 0, (struct sockaddr *) &addr, &addrlen) < 0) {
			get_lasterror("Failed to recv data");
			return 1;
		}
		else {
			print_packet("[Keepalive1 challenge_recv] ", recv_packet1, 100);

			if (recv_packet1[0] == 0x07) {
				break;
			}
			else if (recv_packet1[0] == 0x4d) {
				logiA("Get notice packet.\n");
				continue;
			}
			else {
				logiA("Bad keepalive1 challenge response received.\n");
				return 1;
			}
		}
	}

	unsigned char keepalive1_seed[4] = { 0 };
	int encrypt_type;
	unsigned char crc[8] = { 0 };
	memcpy(keepalive1_seed, &recv_packet1[8], 4);
	encrypt_type = keepalive1_seed[0] & 3;
	gen_crc(keepalive1_seed, encrypt_type, crc);
	keepalive_1_packet2[0] = 0xff;
	memcpy(keepalive_1_packet2 + 8, keepalive1_seed, 4);
	memcpy(keepalive_1_packet2 + 12, crc, 8);
	memcpy(keepalive_1_packet2 + 20, auth_information, 16);
	keepalive_1_packet2[36] = rand() & 0xff;
	keepalive_1_packet2[37] = rand() & 0xff;

	sendto(sockfd, (const char *)keepalive_1_packet2, 42, 0, (struct sockaddr *) &addr, sizeof(addr));

	if (recvfrom(sockfd, (char *)recv_packet2, 1024, 0, (struct sockaddr *) &addr, &addrlen) < 0) {
		get_lasterror("Failed to recv data");
		return 1;
	}
	else {
		print_packet("[Keepalive1 recv] ", recv_packet2, 100);

		if (recv_packet2[0] != 0x07) {
			logiA("Bad keepalive1 response received.\n");
			return 1;
		}
	}


	return 0;
}


void keepalive_2_packetbuilder(unsigned char keepalive_2_packet[], int keepalive_counter, int filepacket, int type, int encrypt_type) {
	keepalive_2_packet[0] = 0x07;
	keepalive_2_packet[1] = keepalive_counter;
	keepalive_2_packet[2] = 0x28;
	keepalive_2_packet[4] = 0x0b;
	keepalive_2_packet[5] = type;
	if (filepacket) {
		keepalive_2_packet[6] = 0x0f;
		keepalive_2_packet[7] = 0x27;
	}
	else {
		memcpy(keepalive_2_packet + 6, drcom_config.KEEP_ALIVE_VERSION, 2);
	}
	keepalive_2_packet[8] = 0x2f;
	keepalive_2_packet[9] = 0x12;
	if (type == 3) {
		unsigned char host_ip[4] = { 0 };
		sscanf(drcom_config.host_ip, "%hhd.%hhd.%hhd.%hhd",
			&host_ip[0],
			&host_ip[1],
			&host_ip[2],
			&host_ip[3]);
		memcpy(keepalive_2_packet + 28, host_ip, 4);
	}
}

int keepalive_2(int sockfd, struct sockaddr_in addr, int *keepalive_counter, int *first, int *encrypt_type) {
	unsigned char keepalive_2_packet[40], recv_packet[1024], tail[4];
	socklen_t addrlen = sizeof(addr);

	if (*first) {
		// send the file packet
		memset(keepalive_2_packet, 0, 40);
		keepalive_2_packetbuilder(keepalive_2_packet, *keepalive_counter % 0xFF, *first, 1, 0);
		(*keepalive_counter)++;

		sendto(sockfd, (const char*)keepalive_2_packet, 40, 0, (struct sockaddr *)&addr, sizeof(addr));

		print_packet("[Keepalive2_file sent] ", keepalive_2_packet, 40);
		if (recvfrom(sockfd, (char*)recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) < 0) {
			get_lasterror("Failed to recv data");
			return 1;
		}
		print_packet("[Keepalive2_file recv] ", recv_packet, 40);

		if (recv_packet[0] == 0x07) {
			if (recv_packet[2] == 0x10) {
				logiA("Filepacket received.\n");
			}
			else if (recv_packet[2] != 0x28) {
				logiA("Bad keepalive2 response received.\n");
				return 1;
			}
		}
		else {
			logiA("Bad keepalive2 response received.\n");
			return 1;
		}
	}

	// send the first packet
	*first = 0;
	memset(keepalive_2_packet, 0, 40);
	keepalive_2_packetbuilder(keepalive_2_packet, *keepalive_counter % 0xFF, *first, 1, 0);
	(*keepalive_counter)++;
	sendto(sockfd, (const char*)keepalive_2_packet, 40, 0, (struct sockaddr *)&addr, sizeof(addr));

	print_packet("[Keepalive2_A sent] ", keepalive_2_packet, 40);

	if (recvfrom(sockfd, (char*)recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) < 0) {
		get_lasterror("Failed to recv data");
		return 1;
	}
	print_packet("[Keepalive2_B recv] ", recv_packet, 40);

	if (recv_packet[0] == 0x07) {
		if (recv_packet[2] != 0x28) {
			logiA("Bad keepalive2 response received.\n");
			return 1;
		}
	}
	else {
		logiA("Bad keepalive2 response received.\n");
		return 1;
	}
	memcpy(tail, &recv_packet[16], 4);

	// send the third packet
	memset(keepalive_2_packet, 0, 40);
	keepalive_2_packetbuilder(keepalive_2_packet, *keepalive_counter % 0xFF, *first, 3, 0);
	memcpy(keepalive_2_packet + 16, tail, 4);
	(*keepalive_counter)++;
	sendto(sockfd, (const char*)keepalive_2_packet, 40, 0, (struct sockaddr *)&addr, sizeof(addr));

	print_packet("[Keepalive2_C sent] ", keepalive_2_packet, 40);


	if (recvfrom(sockfd, (char*)recv_packet, 1024, 0, (struct sockaddr *)&addr, &addrlen) < 0) {
		get_lasterror("Failed to recv data");
		return 1;
	}
	print_packet("[Keepalive2_D recv] ", recv_packet, 40);

	if (recv_packet[0] == 0x07) {
		if (recv_packet[2] != 0x28) {
			logiA("Bad keepalive2 response received.\n");
			return 1;
		}
	}
	else {
		logiA("Bad keepalive2 response received.\n");
		return 1;
	}

	return 0;
}