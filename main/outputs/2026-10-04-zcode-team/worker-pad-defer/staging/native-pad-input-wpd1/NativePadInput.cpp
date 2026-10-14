#include "NativePadInput.hpp"
#include "PS5PadConfig.hpp"
#include <Platform/NativeModSource.hpp>
#include <stdexcept>
#include <string>
#include <utility>

// worker-pad-defer-1 (launch-10 pad-defer floor), staged replacement for the
// composition unit `native-pad-input`. The composition's shared diagnostic
// sink (diagnostic.c, the BOOTSTRAP_UNITS diagnostic substitution recorded in
// launch10-build/EDITION.json edit n=3) already defines this C symbol inside
// the same final link; declaring it here adds no new import, no new header
// dependency, and no new compile flag.
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
        // pad_input_defer (launch-10 floor): unavailable controller input must
        // never abort runtime startup. Live evidence (pids 317/341/355/370,
        // 4 consecutive boots): at the earliest practical injection moment the
        // host process already holds the exclusive pad handle, so the single
        // eager scePadOpen is refused with 0x80920004 ALREADY_OPENED at
        // PadPhase::Open. Emit the exact phase/error evidence on the existing
        // supplier-diag channel and start WITHOUT controller input.
        //
        // Dropping the source runs PS5PadSource::~PS5PadSource -> deactivate()
        // with the owned/borrowed discipline: a handle we never acquired is
        // never closed (close is skipped while handle < 0), and
        // sceUserServiceTerminate runs only when THIS object's
        // sceUserServiceInitialize returned 0 (already-initialized
        // 0x80960003 keeps m_owns_user_service false, so a service borrowed
        // from the host is never torn down).
        const auto status=source->status();
        diagnostic_event("pad_input_deferred", static_cast<unsigned int>(status.phase));
        diagnostic_event("pad_input_deferred_error", static_cast<unsigned int>(status.error));
        return;
    }
    input=std::move(source);
}
}
