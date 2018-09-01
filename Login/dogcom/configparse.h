#ifndef CONFIGPARSE_H_
#define CONFIGPARSE_H_

struct config {
	char server[20];
	char username[36];
	char password[20];
	unsigned char CONTROLCHECKSTATUS;
	unsigned char ADAPTERNUM;
	char host_ip[20];
	unsigned char IPDOG;
	char host_name[20];
	char PRIMARY_DNS[20];
	char dhcp_server[20];
	unsigned char AUTH_VERSION[2];
	unsigned char mac[6];
	char host_os[20];
	unsigned char KEEP_ALIVE_VERSION[2];
};

extern struct config drcom_config;
extern char *log_path;


void fillConfig(const char *username,
	const char *password,
	const unsigned char mac[6]);

#endif // CONFIGPARSE_H_