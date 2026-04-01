#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTimer>
#include "RTLWorker.h"
#include "SpectrumWidget.h"
#include "WaterfallWidget.h"
#include "DemodWFM.h"
#include "AudioOutput.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onSpectrumReady(QVector<float> mags, double centerHz, double sampleRate);
    void onIQReady(QVector<std::complex<float>> samples);
    void onFrequencyChanged(double hz);
    void onStartStop();
    void onWFMToggle(bool on);
    void onFreqSlider(int val);
    void onFreqSpinBox(double mhz);
    void onGainChanged(int val);
    void onVolumeChanged(int val);
    void onSampleRateChanged(int idx);
    void onFFTSizeChanged(int idx);
    void onError(const QString& msg);

private:
    void buildUI();
    void applyStylesheet();
    void updateFrequencyDisplay(double hz);

    // Widgets
    SpectrumWidget*  m_spectrum  = nullptr;
    WaterfallWidget* m_waterfall = nullptr;

    QLabel*         m_freqDisplay  = nullptr;
    QSlider*        m_freqSlider   = nullptr;
    QDoubleSpinBox* m_freqSpinBox  = nullptr;

    QPushButton*    m_startBtn     = nullptr;
    QPushButton*    m_wfmBtn       = nullptr;
    QSlider*        m_gainSlider   = nullptr;
    QSlider*        m_volSlider    = nullptr;
    QComboBox*      m_srateCombo   = nullptr;
    QComboBox*      m_fftCombo     = nullptr;
    QLabel*         m_statusBar    = nullptr;

    // Backend
    RTLWorker*   m_worker   = nullptr;
    DemodWFM*    m_demod    = nullptr;
    AudioOutput* m_audio    = nullptr;

    bool   m_running    = false;
    bool   m_wfmEnabled = false;
    double m_centerHz   = 98100000.0;
    double m_sampleRate = 2048000.0;

    // Freq slider range: 88-108 MHz initially
    static constexpr double SLIDER_MIN_HZ = 87500000.0;
    static constexpr double SLIDER_MAX_HZ = 108000000.0;
    static constexpr int    SLIDER_STEPS  = 100000; // 100kHz per step = fine control
};
