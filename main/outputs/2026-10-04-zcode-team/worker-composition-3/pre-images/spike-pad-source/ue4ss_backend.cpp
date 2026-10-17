#include "ue4ss_backend.hpp"
#include "NativePadInput.hpp"
#include <UE4SSProgram.hpp>
#include <stdexcept>
#include <utility>

namespace modbridge::native
{
namespace
{
struct EngineSignal { std::function<void()> ready; };

class UE4SSBackend final : public RuntimeBackend
{
    std::shared_ptr<EngineSignal> signal_{std::make_shared<EngineSignal>()};
    std::unique_ptr<RC::UE4SSProgram> program_;
public:
    explicit UE4SSBackend(RC::Platform::NativeProgramContext context, bool enable_buffered_mods = false)
    {
        RC::Platform::ValidateNativeProgramContext(context);
        if (enable_buffered_mods && context.mod_source)
            ConfigureRetainedPS5PadInput(*context.mod_source, context.input_source);
        context.mod_startup_policy = RC::Platform::NativeModStartupPolicy::Deferred;
        auto previous_callback = std::move(context.on_engine_initialized);
        // The program and backend both retain signal_; it owns neither one.
        context.on_engine_initialized = [signal = signal_, previous = std::move(previous_callback)] {
            if (previous) previous();
            if (signal->ready) signal->ready();
        };
        program_ = std::make_unique<RC::UE4SSProgram>(std::move(context),
            std::initializer_list<RC::MProgram::BinaryOptions>{});
        if (const auto error = program_->native_startup_error_message())
            throw std::runtime_error(std::string("Native UE4SS construction rejected: ") + error);
        if (enable_buffered_mods)
        {
            program_->start_native_mods();
            if (const auto error = program_->native_startup_error_message())
                throw std::runtime_error(std::string("Native UE4SS inventory rejected: ") + error);
        }
    }
    void initialize(std::function<void()> ready) override
    {
        signal_->ready = std::move(ready);
        // Unlike the successful buffered-Lua probe, this calls the real UE4SS
        // engine bootstrap. It requires validated live context; never use fake
        // mapped bytes here as a console/host success demonstration.
        program_->init();
        signal_->ready = {};
        if (const auto error = program_->native_startup_error_message())
            // Never hide a genuine error that happened concurrently with stop.
            // Program::init handles typed cooperative cancellation separately.
            throw std::runtime_error(error);
    }
    void request_stop() noexcept override
    {
        program_->request_shutdown();
    }
};
}

std::unique_ptr<RuntimeOwner> PrepareDeferredUE4SSRuntime(RC::Platform::NativeProgramContext context)
{
    return std::make_unique<RuntimeOwner>(std::make_unique<UE4SSBackend>(std::move(context)));
}

std::unique_ptr<RuntimeOwner> PrepareActivatedUE4SSRuntime(
    RC::Platform::NativeProgramContext context, ActivatedMods activation,
    std::string_view expected_title)
{
    if (context.mod_source)
        throw std::invalid_argument("Native UE4SS context already has a mod source");
    context.mod_source = MakeActivatedUE4SSModSource(std::move(activation), expected_title);
    return std::make_unique<RuntimeOwner>(std::make_unique<UE4SSBackend>(std::move(context), true));
}
}
