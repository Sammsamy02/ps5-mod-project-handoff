#include "pak_writer.h"
/* Compile/link only: no execution/entry dispatch. Pure-memory fixture closure. */
static mb_pak_writer_workspace_v1 scratch;
static uint8_t archive[4096];
int main(void) {
    static const uint8_t name[] = "Example/Config/default.ini", data[] = "synthetic=true\n";
    const mb_pak_writer_profile_v1 p = {sizeof(p),1,(const uint8_t *)"../../../",9,0,123};
    const mb_pak_writer_file_v1 f = {sizeof(f),1,name,sizeof(name)-1,0,data,sizeof(data)-1};
    mb_pak_writer_limits_v1 l; mb_pak_writer_result_v1 r;
    mb_pak_writer_limits_v1_default(&l);
    return mb_pak_write_v11_none(&p,&f,1,&l,&scratch,archive,sizeof(archive),&r) == MB_PAK_WRITE_OK ? 0 : 1;
}
