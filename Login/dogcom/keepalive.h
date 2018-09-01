#ifndef KEEPALIVE_H_
#define KEEPALIVE_H_

int keepalive_1(int sockfd, struct sockaddr_in addr, unsigned char seed[], unsigned char auth_information[]);
int keepalive_2(int sockfd, struct sockaddr_in addr, int *keepalive_counter, int *first, int *encrypt_type);

#endif // KEEPALIVE_H_