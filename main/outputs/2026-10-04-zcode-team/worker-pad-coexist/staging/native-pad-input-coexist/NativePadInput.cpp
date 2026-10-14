#include "NativePadInput.hpp"
#include "PS5PadConfig.hpp"
#include <Platform/NativeModSource.hpp>
#include <stdexcept>
#include <string>
#include <utility>

// worker-pad-coexist (wpd2 lineage over worker-pad-defer-1), staged replacement
// for the composition unit `native-pad-input`. The composition's shared
// diagnostic sink (diagnostic.c, the BOOTSTRAP_UNITS diagnostic substitution
// recorded in launch10-build/EDITION.json edit n=3) already defines this C
// symbol inside the same final link; declaring it here adds no new import, no
// new header dependency, and no new compile flag.
extern "C" void diagnostic_event(const char* tag, unsigned int value);

namespace modbridge::native
{
void ConfigureRetainedPS5PadInput(const RC::Platform::NativeModSource& files,
    std::shared_ptr<RC::Input::PlatformInputSource>& input)
{
    const auto file=files.read("ue4ss/PS5Pad.ini");
    if (!file) return;
    if (input) throw std::invalid_argument("PS5Pad.ini conflicts with the supplied native input source");
    auto config=RC::Input::parse_ps5_pad_config(file->bytes);
    auto source=RC::Input::make_native_ps5_pad_source(std::move(config.bindings),config.user_id);
    if (!source) throw std::runtime_error("PS5Pad provider factory returned no source");
    if (!source->is_available())
    {
        // pad_input_defer (launch-10 floor, wpd1): unavailable controller input
        // must never abort runtime startup. Live evidence (pids 317/341/355/370,
        // 4 consecutive boots): at the earliest practical injection moment the
        // host process already holds the exclusive pad handle, so the single
        // eager scePadOpen is refused with 0x80920004 ALREADY_OPENED at
        // PadPhase::Open. Emit the exact phase/error evidence on the existing
        // supplier-diag channel.
        const auto status=source->status();
        diagnostic_event("pad_input_deferred", static_cast<unsigned int>(status.phase));
        diagnostic_event("pad_input_deferred_error", static_cast<unsigned int>(status.error));
        // worker-pad-coexist DELTA vs wpd1: the deferred source is now
        // ATTACHED, not dropped. wpd1 returned without assigning, which made
        // any later recovery unreachable (Handler::init refuses unavailable
        // sources, process_event skipped them, RegisterKeyBind threw, and no
        // retry existed). This staging pairs with the coexist Handler and the
        // provider's poll()-side bounded retry: Handler::init deferred-attaches
        // this source, RegisterKeyBind accepts it, and poll() retries
        // activate() event-driven on the sole processing thread until the
        // exclusive handle frees (or forever, bounded - outcome A unchanged:
        // startup never aborts either way). Lifecycle discipline unchanged:
        // a handle we never acquired is never closed, and
        // sceUserServiceTerminate runs only when THIS object's
        // sceUserServiceInitialize returned 0 (the already-initialized
        // 0x80960003 path keeps m_owns_user_service false).
    }
    input=std::move(source);
}
}
