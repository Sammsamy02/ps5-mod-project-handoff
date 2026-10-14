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
    std::shared_ptr<RuntimeAckSink> ack_;
    bool mods_startup_verified_{false};
public:
    explicit UE4SSBackend(RC::Platform::NativeProgramContext context, bool enable_buffered_mods = false,
                          std::shared_ptr<RuntimeAckSink> ack = nullptr)
        : ack_(std::move(ack))
    {
        RC::Platform::ValidateNativeProgramContext(context);
        if (enable_buffered_mods && context.mod_source)
            ConfigureRetainedPS5PadInput(*context.mod_source, context.input_source);
        context.mod_startup_policy = RC::Platform::NativeModStartupPolicy::Deferred;
        auto previous_callback = std::move(context.on_engine_initialized);
        // The program and backend both retain signal_; it owns neither one.
        // The ack is emitted only from inside this chain, which the program
        // invokes solely from its real on_engine_initialized site after
        // Unreal setup, properties and Lua sharing (deferred-startup policy).
        context.on_engine_initialized = [this, signal = signal_, previous = std::move(previous_callback)] {
            if (previous) previous();
            if (signal->ready) signal->ready();
            // RUNTIME-ACK points 2+3: engine-ready first, then mods-started,
            // so the last complete state line makes running observable exactly
            // when the engine callback ran AND the deferred mod startup from
            // the activation views had already completed (constructor
            // start_native_mods; mods_startup_verified_).
            if (ack_)
            {
                ack_->engine_ready();
                if (mods_startup_verified_) ack_->mods_started();
            }
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
            mods_startup_verified_ = true;
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
    std::string_view expected_title, const std::filesystem::path& activation_root,
    const RuntimeAckConfig& runtime_ack)
{
    if (context.mod_source)
        throw std::invalid_argument("Native UE4SS context already has a mod source");
    // Reject the supplied runtime context before native plugin module_start can
    // run. Actual initialization still happens only through RuntimeOwner.start.
    RC::Platform::ValidateNativeProgramContext(context);
    context.mod_source = MakeActivatedUE4SSModSource(std::move(activation), expected_title, activation_root);
    // RUNTIME-ACK point 1: only now has the activation actually been consumed
    // (title matched, unreal/ue4ss-ps5 declared, every startup file preloaded),
    // so the session status file is opened here, during module_start, while
    // /data is still reachable, and activated is written once the buffered
    // inventory (deferred native mod startup included) also succeeded.
    auto ack = runtime_ack.status_root.empty()
        ? std::shared_ptr<RuntimeAckSink>{}
        : RuntimeAckSink::Open(runtime_ack);
    std::unique_ptr<RuntimeOwner> owner;
    try
    {
        auto backend = std::make_unique<UE4SSBackend>(std::move(context), true, ack);
        if (ack) ack->activated();
        owner = std::make_unique<RuntimeOwner>(std::move(backend), std::move(ack));
    }
    catch (const std::exception& error)
    {
        // RUNTIME-ACK point 4 (preparation-time failure): acknowledge, then
        // let the original error fly; the ack must never mask or replace it.
        if (ack) ack->failed(1, error.what());
        throw;
    }
    catch (...)
    {
        if (ack) ack->failed(1, "Native UE4SS preparation failed with an unknown exception");
        throw;
    }
    return owner;
}
}
