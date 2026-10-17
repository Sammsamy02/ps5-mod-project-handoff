#include <memory>
#include <string>
#include <cstring>
static int retire_result,retire_checks,release_checks,stops;
static int bridge_native_code_residence_required(void){return 0;}
static int bridge_native_thread_authority_retire(void){++retire_checks;return retire_result;}
static void bridge_startup_resources_release(){++release_checks;}
static constexpr int MRS_ERR_STOP_EXN=-30;
struct Owner{};static std::unique_ptr<Owner> owner;
static auto& owner_slot(){return owner;}
namespace modbridge::native{bool StopRuntimeWithAck(std::unique_ptr<Owner>&o){++stops;o.reset();return true;}}
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
int main(int argc,char**argv){
 if(argc!=2)return 99;
 bool active=std::strcmp(argv[1],"refuse-inactive")!=0;if(active)owner=std::make_unique<Owner>();
 retire_result=!std::strcmp(argv[1],"unknown")?-1:!std::strcmp(argv[1],"success")?0:1;
 int r=modbridge_runtime_stop();
 if(retire_result){if(!(r!=0&&retire_checks==1&&static_cast<bool>(owner)==active&&stops==0&&release_checks==0))return 1;}
 else if(!(r==0&&retire_checks==1&&!owner&&stops==1&&release_checks==1))return 2;
 return 0;
}
