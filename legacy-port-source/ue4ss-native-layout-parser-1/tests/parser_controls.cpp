#include <NativeLayoutFiles.hpp>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <limits>
using namespace RC::Platform::NativeLayouts;
static size_t checks{};
#define CHECK(x) do{++checks;if(!(x))throw std::runtime_error("parser check: " #x);}while(false)
static auto identity()->std::string{return "[NativeLayout]\nFormat=1\nTarget=prospero-x86_64-sysv\nPointerSize=8\nEngineMajor=5\nEngineMinor=7\nNonShipping=0\nStats=1\nEngineRevision=owned-test-revision\nBuildId=owned-test-build\n";}
static void refused(std::string m,std::string v){bool failed=false;try{(void)ParsePair(m,v);}catch(const std::invalid_argument&e){failed=std::string(e.what()).find("Layout")!=std::string::npos;}CHECK(failed);}
int main(){try{
 const auto meta=identity();
 auto b=ParsePair("\xef\xbb\xbf  # owned test\r\n"+meta+"\n[UObjectBase]\nUEP_TotalSize=0x30\nFlags=0x8:3\nWide=16:32:32:8\nByte=1:0:8:1\n",meta+"[UScriptStruct::ICppStructOps]\n__vecDelDtor=0x10\nZero=0\n");
 CHECK(b.identity.format==1&&b.identity.target=="prospero-x86_64-sysv"&&b.identity.pointer_size==8&&b.identity.engine_major==5&&b.identity.engine_minor==7&&!b.identity.non_shipping&&b.identity.stats);CHECK(b.members.size()==4&&b.vtables.size()==2);CHECK(b.members[0].offset==48&&!b.members[0].bitfield);CHECK(b.members[1].bitfield==(Bitfield{3,1,1}));CHECK(b.members[2].bitfield==(Bitfield{32,32,8}));CHECK(b.members[3].bitfield==(Bitfield{0,8,1}));CHECK(b.vtables[0].section=="UScriptStruct::ICppStructOps"&&b.vtables[0].key=="__vecDelDtor"&&b.vtables[0].offset==16);CHECK(!b.vtables[0].bitfield&&b.vtables[1].offset==0);
 auto edge=ParsePair(meta+"[A]\nx=2147483647\nb=0:0:64:8\n",meta+"[A]\nx=2147483640\n");CHECK(edge.members[0].offset==2147483647u&&edge.vtables[0].offset==2147483640u);CHECK(edge.members[1].bitfield==(Bitfield{0,64,8}));
 for(const auto text:{"[A]\nx=1\nx=2\n","[A]\nx=1\n[A]\ny=2\n","[A]\nx=0x\n","[A]\nx=12tail\n","[A]\nx=0xffffffff\n","[A]\nx=4294967296\n","[A]\nx=-1\n","[A]\nx=+1\n","[A]\nx=1:8\n","[A]\nx=1:0:0\n","[A]\nx=1:0:1:3\n","[A]\nx=1:7:2:1\n","[A]\nx=1:0:1:0\n","[A]\nx=1:64:1:8\n","[A]\nx=1:0:65:8\n","[A]\nx=1:0:1:8:4\n","[A]\nx=1::1\n","[A]\nUEP_TotalSize=0\n","[A]\n[]\n","[A]\n1=2\n","[A]\nitem[]=1\n","[A]\nx=[1,2]\n","[A] garbage\n","[A:B]\nx=1\n","[A::]\nx=1\n","[A]\n\xc3\xa9=1\n","[A\xc3\xa9]\nx=1\n","[A]\nx=1;comment\n"})refused(meta+text,meta);
 for(const auto text:{"[A]\nx=1\n","[A]\nx=8:0\n","[A]\nx=2147483648\n"})refused(meta,meta+text);
 for(const auto key:{"Format","Target","PointerSize","EngineMajor","EngineMinor","NonShipping","Stats","EngineRevision","BuildId"}){auto incomplete=meta;auto begin=incomplete.find(std::string(key)+"=");auto end=incomplete.find('\n',begin);incomplete.erase(begin,end-begin+1);refused(incomplete,meta);}
 for(const auto change:{"Format=2","Target=windows-x86_64","PointerSize=4","EngineMajor=3","EngineMinor=-1","NonShipping=2","Stats=2","EngineRevision=","BuildId="}){auto bad=meta;auto key=std::string(change).substr(0,std::string(change).find('='));auto start=bad.find(key+"=");auto end=bad.find('\n',start);bad.replace(start,end-start,change);refused(bad,meta);}
 refused(meta+"Unknown=1\n",meta);refused(meta+"Format=1\n",meta);refused(meta+"[NativeLayout]\n",meta);refused("[A]\nx=1\n",meta);refused("x=1\n"+meta,meta);
 for(const auto change:{"EngineMajor=6","EngineMinor=8","NonShipping=1","Stats=0","EngineRevision=another","BuildId=another"}){auto other=meta;auto key=std::string(change).substr(0,std::string(change).find('='));auto start=other.find(key+"=");auto end=other.find('\n',start);other.replace(start,end-start,change);refused(meta,other);}
 refused(meta+"[A]\nx=1\rmalformed\n",meta);auto nul=meta;nul.push_back(0);refused(nul,meta);auto controlled=meta;controlled.replace(controlled.find("owned-test-build"),16,"bad\tbuild");refused(controlled,meta);
 refused(meta+"["+std::string(513,'A')+"]\n",meta);refused(meta+"[A]\n"+std::string(513,'k')+"=1\n",meta);refused(meta+"[A]\nx="+std::string(513,'1')+"\n",meta);refused(std::string(8*1024*1024+1,' '),meta);
 auto many=meta+"[A]\n";for(unsigned i=0;i<16384;++i)many+="k"+std::to_string(i)+"=0\n";CHECK(ParsePair(many,meta).members.size()==16384);refused(many+"extra=0\n",meta);

 auto minimal=meta;minimal.replace(minimal.find("EngineMajor=5"),13,"EngineMajor=4");minimal.replace(minimal.find("EngineMinor=7"),13,"EngineMinor=0");CHECK(ParsePair(minimal,minimal).identity.engine_minor==0);
 auto literal=meta;literal.replace(literal.find("owned-test-build"),16,"opaque #;= build");CHECK(ParsePair(literal,literal).identity.build_id=="opaque #;= build");
 auto utf8=meta;utf8.replace(utf8.find("owned-test-build"),16,"bad\xc3\xa9");refused(utf8,meta);
 auto crlf=meta;for(size_t p=0;(p=crlf.find('\n',p))!=crlf.npos;p+=2)crlf.insert(p,"\r");CHECK(ParsePair(crlf,crlf).identity==ParsePair(meta,meta).identity);
 auto exact=meta;exact.resize(8*1024*1024,' ');CHECK(ParsePair(exact,meta).identity==b.identity);
 auto owned=meta+"[A]\nx=0X20:2:2:1\n";auto staged=ParsePair(owned,meta);owned.clear();CHECK(staged.members[0].section=="A"&&staged.members[0].key=="x"&&staged.members[0].offset==32&&staged.members[0].bitfield==(Bitfield{2,2,1}));
 auto old_identity=staged.identity;auto old_entries=staged.members;try{staged=ParsePair(meta,meta+"[A]\nx=7\n");CHECK(false);}catch(const std::invalid_argument&){}CHECK(staged.identity==old_identity&&staged.members==old_entries);
 refused(meta+"[A]\nx=1\r",meta);
 auto empty_sections=meta;for(unsigned i=0;i<16385;++i)empty_sections+="[S"+std::to_string(i)+"]\n";refused(empty_sections,meta);
 CHECK(ParsePair(meta,meta).members.empty());std::printf("native layout parser: %zu owned assertions passed; declared metadata only\n",checks);
 return 0;
}catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
