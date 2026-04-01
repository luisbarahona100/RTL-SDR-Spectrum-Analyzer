#pragma once
#include <vector>
#include <complex>

// Wide-band FM (mono) demodulator using atan2 discriminator
// Produces audio samples at output_rate from IQ at input_rate
class DemodWFM {
public:
    DemodWFM(double inputRate = 2048000.0, double audioRate = 48000.0);

    // Process a block of IQ samples, returns audio PCM [-1,1]
    std::vector<float> process(const std::vector<std::complex<float>>& iq);

    void setInputRate(double rate);
    void setAudioRate(double rate);

private:
    void buildDecimationFilter();
    std::vector<float> lowpass(const std::vector<float>& in, int decimation);

    double m_inputRate;
    double m_audioRate;
    std::complex<float> m_prev{1.f, 0.f};

    // FIR decimation filter taps (simple windowed sinc)
    std::vector<float> m_firTaps;

    // Decimation stages
    int m_decimFirst  = 8;   // IQ -> intermediate (~256kHz)
    int m_decimSecond = 6;   // intermediate -> ~42kHz, then resample

    // History for FIR filter
    std::vector<float> m_histI, m_histQ;
};
