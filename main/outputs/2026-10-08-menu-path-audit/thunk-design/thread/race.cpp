#include <thread>
#include <array>
#include <iostream>
using namespace RC::Unreal;
void original(UObject*,FFrame&,void*) { ++native_calls; }
int main() {
 UFunction a; a.func=original; a.GetFunctionHookData(); auto loaded=a.func;
 std::array<UFunction,6> other;for(auto& f:other)f.func=original;
 FFrame frame;frame.current=&a;UObject context;
 std::atomic<bool> go{};
 std::thread worker([&]{while(!go.load()){}for(auto& f:other)f.GetFunctionHookData();});
 go=true;for(int i=0;i<10000;++i)loaded(&context,frame,nullptr);
 worker.join();if(native_calls!=10000)return 1;
 std::cout << "10000 dispatches / 6 registrations complete\n";
}
