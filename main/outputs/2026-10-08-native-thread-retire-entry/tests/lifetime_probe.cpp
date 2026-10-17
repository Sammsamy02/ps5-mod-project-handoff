#include "NativeThreadAuthorityRetire.h"
#include <cstdio>
#include <cstdlib>
int pre_constructor_result=99;
// This TU is linked first: its constructor precedes the provider initializer,
// and its destructor follows provider state destruction. Actual results tested.
struct LifetimeProbe {
 LifetimeProbe(){pre_constructor_result=bridge_native_thread_authority_retire();}
 ~LifetimeProbe(){
  const int result=bridge_native_thread_authority_retire();
  std::fprintf(stderr,"%s postconstructor call refuses with -1 (actual=%d)\n",result==-1?"PASS":"FAIL",result);
  if(result!=-1)std::_Exit(3);
 }
} lifetime_probe;
