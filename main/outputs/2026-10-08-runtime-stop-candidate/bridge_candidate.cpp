#include <memory>
#include <stdexcept>
#include <string>
static int mode, destructs, releases, residence;
constexpr int MRS_ERR_STOP_EXN=-83;
namespace modbridge::native {struct RuntimeOwner {bool stop(){if(mode==2)throw std::runtime_error("stop");return mode!=1;}~RuntimeOwner(){++destructs;}};}
static std::unique_ptr<modbridge::native::RuntimeOwner> slot;
static auto& owner_slot(){return slot;}
static void bridge_startup_resources_release(){++releases;}
extern "C" int bridge_native_code_residence_required(void){return residence;}
namespace modbridge::native {
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

}
extern "C" int modbridge_runtime_stop(void)
{
    // Close hook publication atomically before any owner destruction, even if
    // partial startup has no owner left. Nonzero/unknown means keep residence.
    if (bridge_native_code_residence_required() != 0) return 1;
    auto &slot = owner_slot();
    if (!slot)
    {
        bridge_startup_resources_release();
        return 0;  // nothing started; nothing to join
    }
    bool stopped = false;
    try
    {
        // Loader control thread (module_stop), never the init thread.
        // Emits at most one 'stopped' ack line, then destroys the owner.
        stopped = modbridge::native::StopRuntimeWithAck(slot);
    }
    catch (...)
    {
        // A stop exception is not proof of join, callback quiescence or safe
        // destruction. Keep the owner and its resources for a later stop attempt.
        return MRS_ERR_STOP_EXN;
    }
    if (stopped || !slot) bridge_startup_resources_release();
    return stopped ? 0 : 1;
}
int main(int argc,char**argv){if(argc!=2)return 99;const std::string c=argv[1];mode=c=="exception"?2:c=="refusal"?1:0;residence=c=="resident"||c=="resident_no_owner"?1:c=="unknown"?-1:0;if(c!="resident_no_owner"&&c!="no_owner")slot=std::make_unique<modbridge::native::RuntimeOwner>();auto* identity=slot.get();int r=modbridge_runtime_stop();if(residence)return r!=0&&slot.get()==identity&&destructs==0&&releases==0?0:10;if(mode==1)return r!=0&&slot.get()==identity&&destructs==0&&releases==0?0:11;if(mode==2)return r==MRS_ERR_STOP_EXN&&slot.get()==identity&&destructs==0&&releases==0?0:12;return r==0&&!slot&&releases==1?0:13;}
