// Host test: staged PS5PadNative.borrow.cpp + real pinned PS5PadSource.cpp (d2f14de1)
// over fake libScePad/libSceUserService. Simulates the live case (game holds the
// standard port -> ALREADY_OPENED) and checks borrow, reads, OPTIONS->F7 edge,
// and that the game's handle is never closed.
#include "PS5PadSource.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace RC::Input;
namespace fake {
int open_result = 0;            // what scePadOpen returns
int get_handle_result = 7;      // what scePadGetHandle returns
int get_handle_calls = 0, open_calls = 0, read_calls = 0, last_read_handle = -1;
std::vector<int> closed;
uint32_t buttons = 0; uint64_t ts = 1;
std::vector<std::pair<std::string, unsigned>> diags;
}
extern "C" {
int scePadInit(void) { return 0; }
int scePadOpen(int, int, int, void*) { ++fake::open_calls; return fake::open_result; }
int scePadGetHandle(int user, int type, int index) {
    ++fake::get_handle_calls;
    return (user == 12 && type == PS5_PAD_PORT_TYPE_STANDARD && index == 0) ? fake::get_handle_result : -1;
}
int scePadReadState(int handle, PS5_PadData* d) {
    ++fake::read_calls; fake::last_read_handle = handle;
    std::memset(d, 0, sizeof *d); d->buttons = fake::buttons; d->connected = 1; d->timestamp = fake::ts++;
    return 0;
}
int scePadClose(int handle) { fake::closed.push_back(handle); return 0; }
int sceUserServiceInitialize(void*) { return static_cast<int>(0x80960003u); } // already initialized by the game
int sceUserServiceTerminate(void) { return 0; }
int sceUserServiceGetLoginUserIdList(int u[4]) { u[0] = 12; u[1] = u[2] = u[3] = -1; return 0; }
void diagnostic_event(const char* tag, unsigned int v) { fake::diags.emplace_back(tag, v); }
}
static int failures = 0;
static void check(bool ok, const char* what) { std::printf("%s %s\n", ok ? "PASS" : "FAIL", what); failures += !ok; }
static bool has_diag(const char* tag, unsigned v) {
    for (auto& d : fake::diags) if (d.first == tag && d.second == v) return true;
    return false;
}
static void reset(int open_result, int get_handle_result) {
    fake::open_result = open_result; fake::get_handle_result = get_handle_result;
    fake::get_handle_calls = fake::open_calls = fake::read_calls = 0; fake::last_read_handle = -1;
    fake::closed.clear(); fake::diags.clear(); fake::buttons = 0;
}
static std::vector<PadBinding> bindings() {
    return {{PS5_PAD_BUTTON_OPTIONS, F7}, {PS5_PAD_BUTTON_UP, UP_ARROW}, {PS5_PAD_BUTTON_CROSS, RETURN}};
}
int main() {
    const int already = static_cast<int>(PS5_PAD_ERROR_ALREADY_OPENED);
    Handler handler; handler.subs[F7] = handler.subs[UP_ARROW] = handler.subs[RETURN] = true;
    {   // A: live case - game holds the port, GetHandle returns its handle
        reset(already, 7);
        auto src = make_native_ps5_pad_source(bindings(), -1);
        check(src->is_available(), "A available after ALREADY_OPENED via borrowed handle");
        check(src->status().handle == 7 && src->status().phase == PadPhase::Active, "A status handle==7 Active");
        check(fake::get_handle_calls == 1, "A scePadGetHandle called once");
        check(has_diag("pad_open_class", 1), "A pad_open_class=1 (borrowed) emitted");
        src->process_event(&handler);                       // baseline sample
        fake::buttons = PS5_PAD_BUTTON_OPTIONS;
        auto& ev = src->process_event(&handler);            // rising OPTIONS edge
        check(ev.size() == 1 && ev[0].key == F7, "A OPTIONS edge -> one F7 event");
        check(fake::last_read_handle == 7, "A reads use the borrowed handle");
        auto& ev2 = src->process_event(&handler);           // held: no repeat
        check(ev2.empty(), "A held OPTIONS does not repeat");
        src->deactivate();
        check(fake::closed.empty(), "A deactivate never closes the game's handle");
    }
    {   // B: pad free - normal owned open, no borrow, closed on deactivate
        reset(3, 7);
        auto src = make_native_ps5_pad_source(bindings(), -1);
        check(src->is_available() && src->status().handle == 3, "B owned open handle 3");
        check(fake::get_handle_calls == 0, "B no GetHandle when open succeeds");
        check(!has_diag("pad_open_class", 1), "B no borrowed class tag");
        src->deactivate();
        check(fake::closed.size() == 1 && fake::closed[0] == 3, "B owned handle closed");
    }
    {   // C: game holds the port and GetHandle fails - honest deferral, original error kept
        reset(already, -2137915391);
        auto src = make_native_ps5_pad_source(bindings(), -1);
        check(!src->is_available(), "C unavailable when GetHandle fails");
        check(src->status().phase == PadPhase::Open && src->status().error == already, "C status keeps ALREADY_OPENED");
        check(has_diag("pad_borrow_failed", static_cast<unsigned>(-2137915391)), "C pad_borrow_failed carries raw rc");
        src->deactivate();
        check(fake::closed.empty(), "C nothing closed");
    }
    {   // D: after a borrowed session ends, an owned handle with the same value closes normally
        reset(already, 7);
        auto a = make_native_ps5_pad_source(bindings(), -1);
        a->deactivate();
        reset(7, 7);
        auto b = make_native_ps5_pad_source(bindings(), -1);
        b->deactivate();
        check(fake::closed.size() == 1 && fake::closed[0] == 7, "D owned handle 7 closed after borrow released");
    }
    std::printf("%s: %d failure(s)\n", failures ? "FAILED" : "ALL PASS", failures);
    return failures ? 1 : 0;
}
