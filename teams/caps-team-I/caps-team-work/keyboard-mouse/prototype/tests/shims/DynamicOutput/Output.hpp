#pragma once
// Host-test stand-in for UE4SS DynamicOutput (the only symbol the real
// Handler.cpp / QueueInputSource.cpp use is Output::send<LogLevel>(fmt, ...)).
// Logging is dropped; behavior under test is unaffected.
namespace RC
{
    namespace LogLevel
    {
        enum LogLevel : int { Default, Normal, Verbose, Warning, Error };
    }
    namespace Output
    {
        template <LogLevel::LogLevel L = LogLevel::Default, typename... Args>
        inline void send(Args&&...) {}
    } // namespace Output
} // namespace RC
