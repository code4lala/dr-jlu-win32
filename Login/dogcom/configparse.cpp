#include <cstring>
#include "configparse.h"

char *log_path = nullptr;


void fillConfig(const char *username,
	const char *password,
	const unsigned char mac[6]) {
	strcpy(drcom_config.server, "10.100.61.3");
	strcpy(drcom_config.username, username);
	strcpy(drcom_config.password, password);
	drcom_config.CONTROLCHECKSTATUS = 0x20;
	drcom_config.ADAPTERNUM = 0x03;
	strcpy(drcom_config.host_ip, "0.0.0.0");
	drcom_config.IPDOG = 0x01;
	strcpy(drcom_config.host_name, "LIYUANYUAN");
	strcpy(drcom_config.PRIMARY_DNS, "10.10.10.10");
	strcpy(drcom_config.dhcp_server, "0.0.0.0");
	drcom_config.AUTH_VERSION[0] = 0x68;
	drcom_config.AUTH_VERSION[1] = 0x00;
	for (int i = 0; i < 6; i++) {
		drcom_config.mac[i] = mac[i];
	}
	strcpy(drcom_config.host_os, "Windows 10");
	drcom_config.KEEP_ALIVE_VERSION[0] = 0xDC;
	drcom_config.KEEP_ALIVE_VERSION[1] = 0x02;
}