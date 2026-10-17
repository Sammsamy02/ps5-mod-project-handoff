__thread void *tls_value;
void **get_tls_address(void) { return &tls_value; }
