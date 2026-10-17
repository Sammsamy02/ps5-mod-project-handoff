
#include <stdexcept>
#include <cstdlib>
namespace RC::Unreal::ThunkDetail {
enum class Residence { Unpublished,RequiresCodeResidence };
struct TeardownSnapshot {Residence residence;};
bool ready=false,published=false,throw_on_begin=false;int begin_calls=0;
bool RegistryReady(){return ready;}
TeardownSnapshot BeginTeardown(){++begin_calls;if(!ready||throw_on_begin)throw std::runtime_error("lifetime");return {published?Residence::RequiresCodeResidence:Residence::Unpublished};}
}

extern "C" int bridge_native_code_residence_required(void) {
    if (!RC::Unreal::ThunkDetail::RegistryReady()) return -1;
    try {
        return RC::Unreal::ThunkDetail::BeginTeardown().residence ==
            RC::Unreal::ThunkDetail::Residence::RequiresCodeResidence ? 1 : 0;
    } catch (...) { return -1; }
}


int main(){using namespace RC::Unreal::ThunkDetail;
 if(bridge_native_code_residence_required()!=-1||begin_calls!=0)return 1;
 ready=true;if(bridge_native_code_residence_required()!=0||begin_calls!=1)return 2;
 published=true;if(bridge_native_code_residence_required()!=1||begin_calls!=2)return 3;
 ready=false;if(bridge_native_code_residence_required()!=-1||begin_calls!=2)return 4;
 ready=true;throw_on_begin=true;if(bridge_native_code_residence_required()!=-1||begin_calls!=3)return 5;
 return 0;
}
