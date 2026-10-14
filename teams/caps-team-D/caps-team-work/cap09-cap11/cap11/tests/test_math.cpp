// test_math.cpp -- accuracy of the ImGui math hooks (mbm_imgui_math.h) against libm. Mac-only.
#include <Platform/mbm_imgui_math.h>
#include <cstdio>
#include <cmath>
int main(){ double worst_s=0,worst_l=0,worst_a=0,worst_c=0; int n=0;
 for(double x=1e-30;x<1e30;x*=1.37){ ++n; worst_s=std::fmax(worst_s,std::fabs(mbm_imgui_math::sqrt_d(x)-std::sqrt(x))/std::sqrt(x)); worst_l=std::fmax(worst_l,std::fabs(mbm_imgui_math::log_d(x)-std::log(x))/std::fmax(1.0,std::fabs(std::log(x))));}
 for(float y=-10;y<=10;y+=0.37f) for(float x=-10;x<=10;x+=0.41f){ ++n; worst_a=std::fmax(worst_a,(double)std::fabs(mbm_imgui_math::atan2_f(y,x)-std::atan2(y,x)));}
 for(float x=-1e6f;x<1e6f;x+=123.37f){ ++n; worst_c=std::fmax(worst_c,(double)std::fabs(mbm_imgui_math::ceil_f(x)-std::ceil(x)));}
 for(float x=-3.9f;x<4;x+=0.1f){ ++n; worst_c=std::fmax(worst_c,(double)std::fabs(mbm_imgui_math::ceil_f(x)-std::ceil(x)));}
 std::printf("test_math items_checked=%d sqrt_rel=%.3g log_err=%.3g atan2_abs=%.3g ceil_abs=%.3g\n",n,worst_s,worst_l,worst_a,worst_c); return (worst_s<1e-14&&worst_l<1e-12&&worst_a<1e-5&&worst_c==0&&n>0)?0:1;}
