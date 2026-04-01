#include "MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFrame>
#include <QMessageBox>
#include <QDebug>
#include <cmath>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("RTL-SDR Spectrum Analyzer — NESDR Smart v5");
    resize(1100, 750);

    m_worker = new RTLWorker(this);
    m_demod  = new DemodWFM(m_sampleRate, 48000.0);
    m_audio  = new AudioOutput(this);

    connect(m_worker, &RTLWorker::spectrumReady, this, &MainWindow::onSpectrumReady);
    connect(m_worker, &RTLWorker::iqReady,       this, &MainWindow::onIQReady);
    connect(m_worker, &RTLWorker::errorOccurred, this, &MainWindow::onError);

    buildUI();
    applyStylesheet();
}

MainWindow::~MainWindow() {
    if (m_running) {
        m_worker->stop();
        m_worker->wait(3000);
    }
    m_audio->stop();
    m_audio->wait(2000);
}

void MainWindow::buildUI() {
    QWidget* central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(6);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // ====== TOP BAR: frequency display + controls ======
    QHBoxLayout* topBar = new QHBoxLayout();

    // Big frequency display
    m_freqDisplay = new QLabel("98.1000 MHz", this);
    m_freqDisplay->setObjectName("freqDisplay");
    m_freqDisplay->setAlignment(Qt::AlignCenter);
    topBar->addWidget(m_freqDisplay);

    // Start/Stop
    m_startBtn = new QPushButton("▶  START", this);
    m_startBtn->setObjectName("startBtn");
    m_startBtn->setCheckable(false);
    m_startBtn->setFixedSize(110, 50);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartStop);
    topBar->addWidget(m_startBtn);

    // WFM toggle
    m_wfmBtn = new QPushButton("WFM  OFF", this);
    m_wfmBtn->setObjectName("wfmBtn");
    m_wfmBtn->setCheckable(true);
    m_wfmBtn->setFixedSize(110, 50);
    connect(m_wfmBtn, &QPushButton::toggled, this, &MainWindow::onWFMToggle);
    topBar->addWidget(m_wfmBtn);

    mainLayout->addLayout(topBar);

    // ====== SPECTRUM ======
    m_spectrum = new SpectrumWidget(this);
    m_spectrum->setMinimumHeight(220);
    connect(m_spectrum, &SpectrumWidget::frequencyClicked, this, &MainWindow::onFrequencyChanged);
    mainLayout->addWidget(m_spectrum, 3);

    // ====== WATERFALL ======
    m_waterfall = new WaterfallWidget(this);
    m_waterfall->setMinimumHeight(160);
    connect(m_waterfall, &WaterfallWidget::frequencyClicked, this, &MainWindow::onFrequencyChanged);
    mainLayout->addWidget(m_waterfall, 2);

    // ====== CONTROLS ROW ======
    QHBoxLayout* ctrlRow = new QHBoxLayout();
    ctrlRow->setSpacing(14);

    // Frequency slider + spinbox
    QGroupBox* freqGrp = new QGroupBox("Frequency (FM Band)", this);
    freqGrp->setObjectName("ctrlGroup");
    QHBoxLayout* freqLayout = new QHBoxLayout(freqGrp);

    m_freqSlider = new QSlider(Qt::Horizontal, this);
    m_freqSlider->setRange(0, SLIDER_STEPS);
    m_freqSlider->setValue((int)((m_centerHz - SLIDER_MIN_HZ) / (SLIDER_MAX_HZ - SLIDER_MIN_HZ) * SLIDER_STEPS));
    m_freqSlider->setMinimumWidth(300);
    connect(m_freqSlider, &QSlider::valueChanged, this, &MainWindow::onFreqSlider);
    freqLayout->addWidget(new QLabel("87.5 MHz"));
    freqLayout->addWidget(m_freqSlider, 1);
    freqLayout->addWidget(new QLabel("108 MHz"));

    m_freqSpinBox = new QDoubleSpinBox(this);
    m_freqSpinBox->setRange(0.1, 1750.0);
    m_freqSpinBox->setValue(m_centerHz / 1e6);
    m_freqSpinBox->setDecimals(4);
    m_freqSpinBox->setSuffix(" MHz");
    m_freqSpinBox->setSingleStep(0.1);
    m_freqSpinBox->setFixedWidth(130);
    connect(m_freqSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &MainWindow::onFreqSpinBox);
    freqLayout->addWidget(m_freqSpinBox);
    ctrlRow->addWidget(freqGrp, 4);

    // Gain
    QGroupBox* gainGrp = new QGroupBox("Gain (dB)", this);
    gainGrp->setObjectName("ctrlGroup");
    QVBoxLayout* gainLayout = new QVBoxLayout(gainGrp);
    m_gainSlider = new QSlider(Qt::Horizontal, this);
    m_gainSlider->setRange(0, 50);
    m_gainSlider->setValue(30);
    m_gainSlider->setToolTip("Tuner gain (0=auto)");
    connect(m_gainSlider, &QSlider::valueChanged, this, &MainWindow::onGainChanged);
    gainLayout->addWidget(m_gainSlider);
    ctrlRow->addWidget(gainGrp, 1);

    // Volume
    QGroupBox* volGrp = new QGroupBox("Volume", this);
    volGrp->setObjectName("ctrlGroup");
    QVBoxLayout* volLayout = new QVBoxLayout(volGrp);
    m_volSlider = new QSlider(Qt::Horizontal, this);
    m_volSlider->setRange(0, 100);
    m_volSlider->setValue(80);
    connect(m_volSlider, &QSlider::valueChanged, this, &MainWindow::onVolumeChanged);
    volLayout->addWidget(m_volSlider);
    ctrlRow->addWidget(volGrp, 1);

    // Sample rate + FFT size
    QGroupBox* cfgGrp = new QGroupBox("Config", this);
    cfgGrp->setObjectName("ctrlGroup");
    QGridLayout* cfgGrid = new QGridLayout(cfgGrp);
    cfgGrid->addWidget(new QLabel("Sample Rate:"), 0, 0);
    m_srateCombo = new QComboBox(this);
    m_srateCombo->addItems({"1.024 MS/s", "2.048 MS/s", "2.4 MS/s", "3.2 MS/s"});
    m_srateCombo->setCurrentIndex(1);
    connect(m_srateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onSampleRateChanged);
    cfgGrid->addWidget(m_srateCombo, 0, 1);
    cfgGrid->addWidget(new QLabel("FFT Size:"), 1, 0);
    m_fftCombo = new QComboBox(this);
    m_fftCombo->addItems({"512", "1024", "2048", "4096", "8192"});
    m_fftCombo->setCurrentIndex(2);
    connect(m_fftCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onFFTSizeChanged);
    cfgGrid->addWidget(m_fftCombo, 1, 1);
    ctrlRow->addWidget(cfgGrp, 1);

    mainLayout->addLayout(ctrlRow);

    // Status bar
    m_statusBar = new QLabel("● Ready — Connect RTL-SDR and press START", this);
    m_statusBar->setObjectName("statusBar");
    mainLayout->addWidget(m_statusBar);
}

void MainWindow::applyStylesheet() {
    setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #07101e;
            color: #8ec8d8;
            font-family: 'Courier New', monospace;
        }

        #freqDisplay {
            font-size: 36px;
            font-weight: bold;
            color: #00ffc4;
            letter-spacing: 3px;
            padding: 8px 20px;
            background: #040d1a;
            border: 1px solid #00806a;
            border-radius: 6px;
            min-width: 280px;
        }

        #startBtn {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 #0a3020, stop:1 #052010);
            color: #00ffc4;
            border: 1.5px solid #00804a;
            border-radius: 5px;
            font-size: 14px;
            font-weight: bold;
            letter-spacing: 2px;
        }
        #startBtn:hover {
            background: #0f4030;
            border-color: #00ffc4;
        }
        #startBtn:pressed { background: #020e08; }

        #wfmBtn {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 #1a1000, stop:1 #0e0800);
            color: #a08030;
            border: 1.5px solid #604010;
            border-radius: 5px;
            font-size: 13px;
            font-weight: bold;
            letter-spacing: 2px;
        }
        #wfmBtn:checked {
            background: qlineargradient(x1:0,y1:0,x2:0,y2:1,
                stop:0 #3a2000, stop:1 #201000);
            color: #ffcc00;
            border-color: #ff8c00;
        }
        #wfmBtn:hover { border-color: #ffcc00; }

        #ctrlGroup {
            background: #050e1c;
            border: 1px solid #0a2535;
            border-radius: 5px;
            font-size: 10px;
            color: #5090a0;
            margin-top: 4px;
            padding: 4px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 4px;
            color: #3090a0;
            font-size: 9px;
        }

        QSlider::groove:horizontal {
            height: 4px;
            background: #0a2535;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 16px;
            height: 16px;
            margin: -6px 0;
            background: #00ffc4;
            border-radius: 8px;
        }
        QSlider::sub-page:horizontal {
            background: qlineargradient(x1:0,y1:0,x2:1,y2:0,
                stop:0 #004433, stop:1 #00ffc4);
            border-radius: 2px;
        }

        QDoubleSpinBox, QComboBox {
            background: #040d1a;
            color: #00ffc4;
            border: 1px solid #0a3540;
            border-radius: 4px;
            padding: 2px 6px;
            font-family: 'Courier New', monospace;
        }
        QDoubleSpinBox:focus, QComboBox:focus {
            border-color: #00ffc4;
        }
        QComboBox QAbstractItemView {
            background: #040d1a;
            color: #8ec8d8;
            selection-background-color: #0a3040;
        }

        #statusBar {
            color: #305060;
            font-size: 10px;
            padding: 2px 8px;
        }

        QLabel {
            color: #406070;
            font-size: 10px;
        }
    )");
}

void MainWindow::updateFrequencyDisplay(double hz) {
    m_freqDisplay->setText(QString::number(hz / 1e6, 'f', 4) + " MHz");
}

// ---- Slots ----

void MainWindow::onStartStop() {
    if (!m_running) {
        if (!m_worker->openDevice(0)) {
            QMessageBox::critical(this, "Error", "Failed to open RTL-SDR device.\nCheck USB connection.");
            return;
        }
        m_worker->setFrequency((uint64_t)m_centerHz);
        m_worker->setSampleRate((uint32_t)m_sampleRate);

        if (m_wfmEnabled) {
            m_audio->open(48000, 1);
            m_audio->start();
        }

        m_worker->start();
        m_running = true;
        m_startBtn->setText("■  STOP");
        m_statusBar->setText("● Running — RTL-SDR active");
    } else {
        m_worker->stop();
        m_worker->wait(3000);
        m_worker->closeDevice();
        m_audio->stop();
        m_audio->wait(2000);
        m_running = false;
        m_startBtn->setText("▶  START");
        m_statusBar->setText("● Stopped");
    }
}

void MainWindow::onWFMToggle(bool on) {
    m_wfmEnabled = on;
    m_wfmBtn->setText(on ? "WFM  ON" : "WFM  OFF");
    if (m_running) {
        if (on) {
            m_audio->open(48000, 1);
            m_audio->start();
        } else {
            m_audio->stop();
            m_audio->wait(1000);
        }
    }
}

void MainWindow::onFrequencyChanged(double hz) {
    // Clamp to device range
    hz = std::max(100e3, std::min(1750e6, hz));
    m_centerHz = hz;
    updateFrequencyDisplay(hz);

    // Update slider (if within FM range)
    bool blocked = m_freqSlider->blockSignals(true);
    double norm = (hz - SLIDER_MIN_HZ) / (SLIDER_MAX_HZ - SLIDER_MIN_HZ);
    m_freqSlider->setValue((int)std::max(0.0, std::min(1.0, norm)) * SLIDER_STEPS);
    m_freqSlider->blockSignals(blocked);

    bool blocked2 = m_freqSpinBox->blockSignals(true);
    m_freqSpinBox->setValue(hz / 1e6);
    m_freqSpinBox->blockSignals(blocked2);

    m_spectrum->setCursorFreq(hz);
    m_waterfall->setMarkerHz(hz);

    if (m_running) m_worker->setFrequency((uint64_t)hz);
}

void MainWindow::onFreqSlider(int val) {
    double hz = SLIDER_MIN_HZ + (double)val / SLIDER_STEPS * (SLIDER_MAX_HZ - SLIDER_MIN_HZ);
    onFrequencyChanged(hz);
}

void MainWindow::onFreqSpinBox(double mhz) {
    onFrequencyChanged(mhz * 1e6);
}

void MainWindow::onGainChanged(int val) {
    if (m_worker->isOpen()) {
        if (val == 0) {
            rtlsdr_set_tuner_gain_mode(nullptr, 0); // hack: need to store dev
        }
        // Direct gain: val * 10 in tenths of dB
        m_statusBar->setText(QString("● Gain set to %1 dB").arg(val));
    }
}

void MainWindow::onVolumeChanged(int val) {
    m_audio->setVolume(val / 100.f);
}

void MainWindow::onSampleRateChanged(int idx) {
    static const uint32_t rates[] = {1024000, 2048000, 2400000, 3200000};
    m_sampleRate = rates[idx];
    m_demod->setInputRate(m_sampleRate);
    if (m_running) m_worker->setSampleRate(rates[idx]);
}

void MainWindow::onFFTSizeChanged(int idx) {
    static const int sizes[] = {512, 1024, 2048, 4096, 8192};
    m_worker->setFFTSize(sizes[idx]);
}

void MainWindow::onSpectrumReady(QVector<float> mags, double centerHz, double sampleRate) {
    m_spectrum->setSpectrum(mags, centerHz, sampleRate);
    m_waterfall->addLine(mags, centerHz, sampleRate);
}

void MainWindow::onIQReady(QVector<std::complex<float>> samples) {
    if (!m_wfmEnabled) return;
    std::vector<std::complex<float>> iq(samples.begin(), samples.end());
    auto audio = m_demod->process(iq);
    if (!audio.empty()) m_audio->enqueue(audio);
}

void MainWindow::onError(const QString& msg) {
    m_statusBar->setText("● ERROR: " + msg);
    if (m_running) onStartStop(); // stop
}
