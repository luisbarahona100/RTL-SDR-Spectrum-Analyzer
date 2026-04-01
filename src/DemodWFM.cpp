#include "DemodWFM.h"
#include <cmath>
#include <algorithm>

static std::vector<float> makeSincFilter(int taps, double cutoff, double sampleRate) {
    // Windowed sinc (Hann window)
    std::vector<float> h(taps);
    int M = taps - 1;
    double fc = cutoff / sampleRate; // normalized
    for (int i = 0; i <= M; i++) {
        double w = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / M)); // Hann
        double sinc = (i == M/2) ? 2.0 * fc
                                  : std::sin(2.0 * M_PI * fc * (i - M/2)) / (M_PI * (i - M/2));
        h[i] = static_cast<float>(w * sinc);
    }
    // Normalize
    float sum = 0;
    for (auto v : h) sum += v;
    for (auto& v : h) v /= sum;
    return h;
}

DemodWFM::DemodWFM(double inputRate, double audioRate)
    : m_inputRate(inputRate), m_audioRate(audioRate)
{
    buildDecimationFilter();
}

void DemodWFM::setInputRate(double rate) {
    m_inputRate = rate;
    buildDecimationFilter();
}

void DemodWFM::setAudioRate(double rate) {
    m_audioRate = rate;
}

void DemodWFM::buildDecimationFilter() {
    // Low-pass at 80kHz (audio BW for WFM)
    int taps = 63;
    double cutoff = 80000.0;
    m_firTaps = makeSincFilter(taps, cutoff, m_inputRate);
    m_histI.assign(taps, 0.f);
    m_histQ.assign(taps, 0.f);
}

// Apply FIR filter then decimate
std::vector<float> DemodWFM::lowpass(const std::vector<float>& in, int decimation) {
    int taps = (int)m_firTaps.size();
    std::vector<float> hist(taps, 0.f);
    // We'll just do direct convolution with decimation
    int outLen = (int)in.size() / decimation;
    std::vector<float> out(outLen, 0.f);

    // Use a ring buffer approach
    std::vector<float> buf(taps, 0.f);
    int pos = 0;
    int outIdx = 0;
    for (int i = 0; i < (int)in.size(); i++) {
        buf[pos % taps] = in[i];
        if (i % decimation == 0 && outIdx < outLen) {
            float acc = 0.f;
            for (int k = 0; k < taps; k++)
                acc += m_firTaps[k] * buf[(pos - k + taps * 2) % taps];
            out[outIdx++] = acc;
        }
        pos++;
    }
    return out;
}

std::vector<float> DemodWFM::process(const std::vector<std::complex<float>>& iq) {
    if (iq.empty()) return {};

    // --- Step 1: FM discriminator (atan2 of conjugate product) ---
    int N = (int)iq.size();
    std::vector<float> demod(N);
    std::complex<float> prev = m_prev;
    // Max freq deviation for WFM is ~75kHz, gain = sampleRate / (2*pi*maxDev)
    float gain = (float)(m_inputRate / (2.0 * M_PI * 75000.0));
    for (int i = 0; i < N; i++) {
        std::complex<float> prod = iq[i] * std::conj(prev);
        demod[i] = std::atan2(prod.imag(), prod.real()) * gain;
        prev = iq[i];
    }
    m_prev = prev;

    // --- Step 2: Low-pass + decimate to ~256kHz (first stage) ---
    // Decimation factor = floor(inputRate / 256000)
    int dec1 = std::max(1, (int)(m_inputRate / 256000.0));
    std::vector<float> stage1 = lowpass(demod, dec1);
    double rate1 = m_inputRate / dec1;

    // --- Step 3: Second decimation to near audioRate ---
    int dec2 = std::max(1, (int)(rate1 / m_audioRate));
    std::vector<float> stage2;
    if (dec2 > 1) {
        // Simple average decimation for speed
        int outLen = (int)stage1.size() / dec2;
        stage2.resize(outLen);
        for (int i = 0; i < outLen; i++) {
            float acc = 0.f;
            for (int k = 0; k < dec2; k++) acc += stage1[i*dec2 + k];
            stage2[i] = acc / dec2;
        }
    } else {
        stage2 = stage1;
    }

    // --- Step 4: Simple de-emphasis (tau = 75us for Europe/75us Americas) ---
    // H(z) = (1 - alpha) / (1 - alpha*z^-1),  alpha = exp(-1/(tau * audioRate))
    float tau = 75e-6f;
    double finalRate = m_audioRate; // approximate
    float alpha = std::exp(-1.0f / (tau * (float)finalRate));
    float y = 0.f;
    for (auto& s : stage2) {
        y = (1.f - alpha) * s + alpha * y;
        s = y;
    }

    // Clip
    for (auto& s : stage2)
        s = std::max(-1.f, std::min(1.f, s));

    return stage2;
}
