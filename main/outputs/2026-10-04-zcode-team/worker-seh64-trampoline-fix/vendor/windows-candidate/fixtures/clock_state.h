/* Executed inside freshly compiled owned PE instructions, never a host clock
 * substitution. Frequency is the adapter count scale, not timer resolution. */
__declspec(dllimport) int QueryPerformanceCounter(long long *);
__declspec(dllimport) int QueryPerformanceFrequency(long long *);
static long long clock_previous;
static int clock_check(void){
 long long frequency=-1,first=-1,second=-1;
 u32 saved=GetLastError();
 if(!QueryPerformanceFrequency(&frequency) || frequency!=1000000000ll || GetLastError()!=saved)return 0;
 if(!QueryPerformanceCounter(&first) || first<=0 || first<clock_previous || GetLastError()!=saved)return 0;
 if(!QueryPerformanceCounter(&second) || second<first || GetLastError()!=saved)return 0;
 clock_previous=second;return 1;
}
