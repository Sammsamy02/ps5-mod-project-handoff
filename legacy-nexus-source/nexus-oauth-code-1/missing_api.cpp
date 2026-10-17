#include "oauth_code.h"
#include "download/oauth_code_transfer.h"
int main() {
    mb_nexus_oauth_code_fields_v1 fields{};
    mb_nexus_oauth_exchange_params_v1 exchange{};
    mb_nexus_oauth_exchange_result_v1 exchanged{};
    mb_curl_oauth_code_transfer_params_v1 transfer{};
    mb_curl_oauth_code_transfer_result_v1 transferred{};
    char output[32]{};
    size_t written=0;
    return mb_nexus_oauth_write_code_form_v1(&fields,output,sizeof(output),&written)
         + mb_nexus_oauth_exchange_code_v1(&exchange,&exchanged)
         + mb_curl_oauth_code_transfer_v1(&transfer,&transferred);
}
