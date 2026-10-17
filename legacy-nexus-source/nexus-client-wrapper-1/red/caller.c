#include "nexus_client.h"
int main(void) { return mb_nexus_client_request_v1(NULL, NULL) == MB_NEXUS_CLIENT_BAD_ARGUMENT ? 0 : 1; }
