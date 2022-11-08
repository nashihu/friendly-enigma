#include <stdio.h>

#ifndef P3_H
#define P3_H 1
#endif

void stopandwait_server(char* iface, long port, FILE* fp);
void stopandwait_client(char* host, long port, FILE* fp);
