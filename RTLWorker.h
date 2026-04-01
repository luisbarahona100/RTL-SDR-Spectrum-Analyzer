#pragma once
#include <QThread>
#include <QMutex>
#include <atomic>
#include <vector>
#include <complex>
#include <fftw3.h>
#include <rtl-sdr.h>

class RTLWorker : public QThread {
    Q_OBJECT
public:
    explicit RTLWorker(QObject* parent = nullptr);
    ~RTLWorker();

    bool openDevice(int index = 0);
    void closeDevice();
    bool isOpen() const { return m_dev != nullptr; }

    void setFrequency(uint64_t hz);
    void setSampleRate(uint32_t rate);
    void setFFTSize(int size);
    void stop();

    uint64_t frequency() const { return m_freq; }
    uint32_t sampleRate() const { return m_sampleRate; }

signals:
    void spectrumReady(QVector<float> magnitudes, double centerFreq, double sampleRate);
    void iqReady(QVector<std::complex<float>> samples);
    void errorOccurred(const QString& msg);

protected:
    void run() override;

private:
    static void rtlCallback(unsigned char* buf, uint32_t len, void* ctx);
    void processBuffer(unsigned char* buf, uint32_t len);
    void computeFFT(const std::vector<std::complex<float>>& iq);

    rtlsdr_dev_t* m_dev = nullptr;
    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_freq{98100000};
    std::atomic<uint32_t> m_sampleRate{2048000};

    // FFT
    int m_fftSize = 2048;
    fftwf_plan m_plan = nullptr;
    fftwf_complex* m_fftIn = nullptr;
    fftwf_complex* m_fftOut = nullptr;
    std::vector<float> m_window;
    QMutex m_fftMutex;

    // IQ buffer accumulator
    std::vector<std::complex<float>> m_iqBuf;
    int m_iqBufPos = 0;
};
