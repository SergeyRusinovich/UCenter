#include "core/cpu_time.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <sys/resource.h>
#include <time.h>
#endif

namespace cpu_time {

#ifdef _WIN32

namespace {

double seconds(const FILETIME &t)
{
    return double((static_cast<unsigned long long>(t.dwHighDateTime) << 32) | t.dwLowDateTime)
           * 1e-7;  // единицы по 100 нс
}

}  // namespace

double thread()
{
    FILETIME created, exited, kernel, user;
    return GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user) ? seconds(kernel) + seconds(user)
                                                                                 : 0.0;
}

double process()
{
    FILETIME created, exited, kernel, user;
    return GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) ? seconds(kernel) + seconds(user)
                                                                                   : 0.0;
}

#else

double thread()
{
    timespec ts{};
    return clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0 ? double(ts.tv_sec) + double(ts.tv_nsec) * 1e-9 : 0.0;
}

double process()
{
    rusage r{};
    if (getrusage(RUSAGE_SELF, &r) != 0)
        return 0.0;
    const auto sec = [](const timeval &t) { return double(t.tv_sec) + double(t.tv_usec) * 1e-6; };
    return sec(r.ru_utime) + sec(r.ru_stime);
}

#endif

}  // namespace cpu_time
