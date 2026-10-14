#include <stdio.h>
#include <errno.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *f=tmpfile();
    if (!f) return 3;
    unsigned char bytes[256];
    for (int i=0;i<256;++i) bytes[i]=(unsigned char)i;
    if (fwrite(bytes,1,sizeof(bytes),f)!=sizeof(bytes)) return 4;
    rewind(f);
    flockfile(f);
    if (fgetc(f)!=0) return 1;
    funlockfile(f);
    for (int i=1;i<256;++i) if (fgetc(f)!=i) return 1;
    if (fgetc(f)!=EOF || !feof(f) || ferror(f)) return 1;
    if (fgetc(f)!=EOF || !feof(f) || ferror(f)) return 1;
    if (ungetc(0xfe,f)!=0xfe || feof(f)) return 1;
    if (fgetc(f)!=0xfe || fgetc(f)!=EOF || !feof(f) || ferror(f)) return 1;
    clearerr(f);
    if (feof(f) || ferror(f)) return 1;
    rewind(f);
    if (ungetc(0x80,f)!=0x80 || fgetc(f)!=0x80 || fgetc(f)!=0) return 1;
    fclose(f);
    f=fopen(argv[1],"wb");
    if (!f) return 3;
    errno=0;
    if (fgetc(f)!=EOF || !ferror(f) || feof(f) || errno==0) return 1;
    clearerr(f);
    if (feof(f) || ferror(f)) return 1;
    fclose(f);
    puts("PASS fgetc real streams: all256 bytes, recursive stream lock, pushback, EOF/repeated EOF, error/errno/clearerr");
    return 0;
}
