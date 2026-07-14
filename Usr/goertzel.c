#include "goertzel.h"
#include <math.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

void Goertzel_Detect(const uint16_t *samples, uint32_t n,
                     float target_hz, float fs_hz,
                     float *mag, float *phase_rad)
{
    float k = 0.5f + ((float)n * target_hz) / fs_hz;
    float w = (2.0f * PI / (float)n) * k;
    float cosine = cosf(w);
    float sine = sinf(w);
    float coeff = 2.0f * cosine;
    float q0 = 0.0f, q1 = 0.0f, q2 = 0.0f;
    float real, imag;
    uint32_t i;

    /* 先去掉直流分量的均值，避免直流泄漏影响相位结果 */
    float mean = 0.0f;
    for (i = 0; i < n; i++)
    {
        mean += (float)samples[i];
    }
    mean /= (float)n;

    for (i = 0; i < n; i++)
    {
        float x = (float)samples[i] - mean;
        q0 = coeff * q1 - q2 + x;
        q2 = q1;
        q1 = q0;
    }

    real = q1 - q2 * cosine;
    imag = q2 * sine;

    *mag = sqrtf(real * real + imag * imag);
    *phase_rad = atan2f(imag, real);
}
