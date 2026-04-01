#pragma once
#include <QWidget>
#include <QVector>
#include <QTimer>

class SpectrumWidget : public QWidget {
    Q_OBJECT
public:
    explicit SpectrumWidget(QWidget* parent = nullptr);

    void setSpectrum(const QVector<float>& mags, double centerHz, double bwHz);
    void setCursorFreq(double hz);
    double markerFreq() const { return m_markerHz; }

signals:
    void frequencyClicked(double hz);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    double pixToFreq(int x) const;
    int    freqToPix(double hz) const;

    QVector<float> m_mags;
    QVector<float> m_avgMags;   // exponential average
    double m_centerHz = 98.1e6;
    double m_bwHz     = 2048000.0;
    double m_markerHz = 98.1e6;
    float  m_refLevel = 0.f;
    float  m_dbRange  = 80.f;

    // Smoothed peaks for background glow effect
    QVector<float> m_peakHold;
    int m_peakDecay = 0;
};
