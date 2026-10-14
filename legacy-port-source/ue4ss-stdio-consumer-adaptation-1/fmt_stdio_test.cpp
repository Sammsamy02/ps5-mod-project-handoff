#include <fmt/format.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <set>
#include <system_error>

static bool output_test(int buffering) {
    FILE *f=std::tmpfile();
    if (!f) return false;
    char buf[4096];
    if (std::setvbuf(f,buf,buffering,sizeof(buf))!=0) return false;
    // Standard stream locks are recursive; exercise calls while the caller
    // already holds the real host stream lock.
    flockfile(f);
    fmt::print(f,"{} {:08x} {:.3f}\n","hello",0xab,1.25);
    funlockfile(f);
    std::string large(17000,'z');
    fmt::print(f,"{}\n",large);
    std::fflush(f);
    std::rewind(f);
    std::string got;
    int c;
    while ((c=std::fgetc(f))!=EOF) got.push_back(static_cast<char>(c));
    bool ok=got=="hello 000000ab 1.250\n"+large+"\n" && !std::ferror(f);
    std::fclose(f);
    return ok;
}

static bool concurrent_test() {
    FILE *f=std::tmpfile();
    if (!f) return false;
    std::vector<std::thread> threads;
    for (int t=0;t<4;++t) threads.emplace_back([=]{
        for (int n=0;n<100;++n) fmt::print(f,"record:{}:{:03}:{}\n",t,n,"complete");
    });
    for(auto &t:threads)t.join();
    std::fflush(f);std::rewind(f);
    std::set<std::string> actual,expected;
    for(int t=0;t<4;++t)for(int n=0;n<100;++n)
        expected.insert(fmt::format("record:{}:{:03}:{}\n",t,n,"complete"));
    char line[128];size_t lines=0;
    while(std::fgets(line,sizeof(line),f)){actual.insert(line);++lines;}
    bool ok=lines==400 && actual==expected && !std::ferror(f);
    std::fclose(f);return ok;
}

int main(int argc,char **argv) {
    if(argc!=2)return 2;
    for(int mode:{_IOFBF,_IOLBF,_IONBF})if(!output_test(mode))return 1;
    if(!concurrent_test())return 1;
    FILE *f=std::fopen(argv[1],"rb");
    if(!f)return 3;
    std::setvbuf(f,nullptr,_IONBF,0);
    bool threw=false;
    try {fmt::print(f,"this write must fail");}
    catch(const std::system_error &){threw=true;}
    bool error=std::ferror(f)!=0;
    std::fclose(f);
    if(!threw || !error)return 1;
    std::puts("PASS actual fmt: full/line/unbuffered output, wide buffer boundary, recursive stream lock, 400 concurrent whole records, write-error exception");
    return 0;
}
