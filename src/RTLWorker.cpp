#include "RTLWorker.h"
#include <QDebug>
#include <cmath>
#include <cstring>

RTLWorker::RTLWorker(QObject* parent) : QThread(parent) {}

RTLWorker::~RTLWorker() {
    stop();
    wait(3000);
    closeDevice();
    QMutexLocker lk(&m_fftMutex);
    if (m_plan)  { fftwf_destroy_plan(m_plan); m_plan = nullptr; }
    if (m_fftIn)  { fftwf_free(m_fftIn);  m_fftIn = nullptr; }
    if (m_fftOut) { fftwf_free(m_fftOut); m_fftOut = nullptr; }
}

bool RTLWorker::openDevice(int index) {
    if (rtlsdr_open(&m_dev, index) < 0) {
        emit errorOccurred("Cannot open RTL-SDR device");
        return false;
    }
    rtlsdr_set_sample_rate(m_dev, m_sampleRate);
    rtlsdr_set_center_freq(m_dev, m_freq);
    rtlsdr_set_tuner_gain_mode(m_dev, 0); // auto gain
    rtlsdr_reset_buffer(m_dev);

    // Build Hann window
    m_window.resize(m_fftSize);
    for (int i = 0; i < m_fftSize; i++)
        m_window[i] = 0.5f * (1.f - std::cos(2.f * M_PI * i / (m_fftSize - 1)));

    // Allocate FFTW
    QMutexLocker lk(&m_fftMutex);
    m_fftIn  = fftwf_alloc_complex(m_fftSize);
    m_fftOut = fftwf_alloc_complex(m_fftSize);
    m_plan   = fftwf_plan_dft_1d(m_fftSize, m_fftIn, m_fftOut, FFTW_FORWARD, FFTW_MEASURE);
    m_iqBuf.resize(m_fftSize);
    m_iqBufPos = 0;
    return true;
}

void RTLWorker::closeDevice() {
    if (m_dev) { rtlsdr_close(m_dev); m_dev = nullptr; }
}

void RTLWorker::setFrequency(uint64_t hz) {
    m_freq = hz;
    if (m_dev) rtlsdr_set_center_freq(m_dev, hz);
}

void RTLWorker::setSampleRate(uint32_t rate) {
    m_sampleRate = rate;
    if (m_dev) rtlsdr_set_sample_rate(m_dev, rate);
}

void RTLWorker::setFFTSize(int size) {
    QMutexLocker lk(&m_fftMutex);
    m_fftSize = size;
    if (m_plan)  { fftwf_destroy_plan(m_plan); m_plan = nullptr; }
    if (m_fftIn)  { fftwf_free(m_fftIn);  m_fftIn = nullptr; }
    if (m_fftOut) { fftwf_free(m_fftOut); m_fftOut = nullptr; }
    m_fftIn  = fftwf_alloc_complex(size);
    m_fftOut = fftwf_alloc_complex(size);
    m_plan   = fftwf_plan_dft_1d(size, m_fftIn, m_fftOut, FFTW_FORWARD, FFTW_MEASURE);
    m_window.resize(size);
    for (int i = 0; i < size; i++)
        m_window[i] = 0.5f * (1.f - std::cos(2.f * M_PI * i / (size - 1)));
    m_iqBuf.resize(size);
    m_iqBufPos = 0;
}

void RTLWorker::stop() {
    m_running = false;
    if (m_dev) rtlsdr_cancel_async(m_dev);
}

void RTLWorker::rtlCallback(unsigned char* buf, uint32_t len, void* ctx) {
    auto* self = static_cast<RTLWorker*>(ctx);
    if (!self->m_running) return;
    self->processBuffer(buf, len);
}

void RTLWorker::processBuffer(unsigned char* buf, uint32_t len) {
    // Convert uint8 IQ -> complex float [-1,1]
    uint32_t samples = len / 2;
    std::vector<std::complex<float>> iq(samples);
    for (uint32_t i = 0; i < samples; i++) {
        iq[i] = { (buf[2*i]   - 127.5f) / 127.5f,
                  (buf[2*i+1] - 127.5f) / 127.5f };
    }

    // Emit IQ for demodulator
    QVector<std::complex<float>> qiq(iq.begin(), iq.end());
    emit iqReady(qiq);

    // Accumulate for FFT
    for (auto& s : iq) {
        m_iqBuf[m_iqBufPos++] = s;
        if (m_iqBufPos >= m_fftSize) {
            computeFFT(m_iqBuf);
            m_iqBufPos = 0;
        }
    }
}

void RTLWorker::computeFFT(const std::vector<std::complex<float>>& iq) {
    QMutexLocker lk(&m_fftMutex);
    if (!m_plan) return;

    int N = m_fftSize;
    for (int i = 0; i < N; i++) {
        m_fftIn[i][0] = iq[i].real() * m_window[i];
        m_fftIn[i][1] = iq[i].imag() * m_window[i];
    }
    fftwf_execute(m_plan);

    // Compute magnitude in dBFS, shift so DC is center
    QVector<float> mag(N);
    float scale = 1.0f / N;
    for (int i = 0; i < N; i++) {
        int k = (i + N/2) % N; // fftshift
        float re = m_fftOut[k][0] * scale;
        float im = m_fftOut[k][1] * scale;
        float pwr = re*re + im*im;
        mag[i] = (pwr > 1e-20f) ? 10.f * std::log10(pwr) : -120.f;
    }

    emit spectrumReady(mag, static_cast<double>(m_freq.load()), static_cast<double>(m_sampleRate.load()));
}

void RTLWorker::run() {
    if (!m_dev) return;
    m_running = true;
    // 16 * fftSize bytes per callback
    rtlsdr_read_async(m_dev, rtlCallback, this, 0, m_fftSize * 8);
    m_running = false;
}
