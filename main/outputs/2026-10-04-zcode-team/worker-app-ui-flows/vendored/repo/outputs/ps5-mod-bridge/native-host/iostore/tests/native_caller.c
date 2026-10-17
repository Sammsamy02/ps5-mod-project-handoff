#include "toc_reader.h"
/* ABI link-only caller; this file and the resulting target are never executed. */
int main(void) {
    static const uint8_t header[144]={
        [0]='-',[1]='=',[2]='=',[3]='-',[4]='-',[5]='=',[6]='=',[7]='-',
        [8]='-',[9]='=',[10]='=',[11]='-',[12]='-',[13]='=',[14]='=',[15]='-',
        [16]=8,[20]=144,[24]=1,[28]=1,[32]=12,[40]=32,[44]=64,[52]=1,
        [88]=255,[89]=255,[90]=255,[91]=255,[92]=255,[93]=255,[94]=255,[95]=255
    };
    static const uint8_t id[12]={0},length[10]={0},block[12]={[5]=1,[8]=1};
    const mb_iostore_view_v1 view={sizeof(view),1,{header,sizeof(header),0},202};
    const mb_iostore_span_v1 ids={id,sizeof(id),144},logical={length,sizeof(length),156},row={block,sizeof(block),166};
    const uint64_t paired=1;
    mb_iostore_limits_v1 limits; mb_iostore_limits_v1_default(&limits);
    mb_iostore_header_result_v1 h; mb_iostore_chunk_result_v1 c; mb_iostore_block_result_v1 b;
    return mb_iostore_read_header_v1(&view,&limits,&h) |
        mb_iostore_read_chunk_v1(&view,&limits,0,&ids,&logical,&c) |
        mb_iostore_read_block_v1(&view,&limits,0,&row,&paired,1,&b);
}
