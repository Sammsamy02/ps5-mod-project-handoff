#include "auth.h"
int main(void) { mb_nexus_auth_context_v1 *ctx=0; return mb_nexus_auth_create_v1(0,&ctx,0)==MB_NEXUS_AUTH_BAD_ARGUMENT?0:1; }
