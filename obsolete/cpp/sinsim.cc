#include <time.h>

long long getTimestampMs() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((long long)tv.tv_sec * 1000LL) + (tv.tv_usec / 1000);
}

float sinsim(float avr, float ampl, float period) {
    float arg = 2.0 * 3.14 * millis() * 0.001 / period;
    return avr + ampl * sin(arg);
}  

