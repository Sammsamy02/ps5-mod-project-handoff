#include <memory>
#include <stdexcept>
#include <string>
static int destructs;
struct RuntimeOwner{int mode;bool stop(){if(mode==2)throw std::runtime_error("stop");return mode==0;}~RuntimeOwner(){++destructs;}};
auto StopRuntimeWithAck(std::unique_ptr<RuntimeOwner>& owner) -> bool
{
    if (!owner)
    {
        return false;
    }
    const bool stopped = owner->stop();
    if (stopped) owner.reset();  // preserve the owner when stop refuses
    return stopped;
}
int main(int argc,char**argv){if(argc!=2)return 99;int mode=std::stoi(argv[1]);auto p=std::make_unique<RuntimeOwner>();p->mode=mode;auto* identity=p.get();if(mode==2){try{StopRuntimeWithAck(p);return 2;}catch(...){return p.get()==identity&&destructs==0?0:3;}}bool r=StopRuntimeWithAck(p);return mode==0?(r&&!p&&destructs==1?0:4):(!r&&p.get()==identity&&destructs==0?0:5);}
