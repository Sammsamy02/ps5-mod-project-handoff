#include "metadata.h"
#include <stdio.h>
#include <string.h>
_Static_assert(sizeof(((mb_title_metadata_result_v1 *)0)->title_id)==65,"title ID field");
_Static_assert(sizeof(((mb_title_metadata_result_v1 *)0)->name)==257,"name field");
_Static_assert(sizeof(((mb_title_metadata_result_v1 *)0)->version)==129,"version field");
_Static_assert(sizeof(((mb_title_metadata_result_v1 *)0)->locale)==65,"locale field");
_Static_assert(sizeof(((mb_title_metadata_result_v1 *)0)->metadata_sha256)==65,"metadata identity field");
int main(void) {
    static const uint8_t json[]="{\"titleId\":\"Homebrew_42\",\"masterVersion\":\"not-a-build\"}";
    mb_title_metadata_input_v1 input={sizeof(input),1,json,sizeof(json)-1,1,0};
    mb_title_metadata_result_v1 result;
    if(mb_title_metadata_decode_v1(&input,&result)!=MB_TITLE_METADATA_OK) return 1;
    if(strcmp(result.title_id,"Homebrew_42") || result.version_available || result.name_available) return 1;
    if(!result.metadata_complete || !result.metadata_sha256_available || strlen(result.metadata_sha256)!=64) return 1;
    if(result.installed_observed || result.running_observed || result.build_fingerprint_available ||
        result.engine_acceptance_verified || result.runtime_supported || result.installation_supported ||
        result.authenticity_verified || result.game_acceptance_verified) return 1;
    printf("C_ABI PASS input_size=%zu result_size=%zu metadata_only=true\n",sizeof(input),sizeof(result));
    return 0;
}
