#include "SpectrumWidget.h"
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QFontDatabase>
#include <cmath>

SpectrumWidget::SpectrumWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(200);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void SpectrumWidget::setSpectrum(const QVector<float>& mags, double centerHz, double bwHz) {
    m_centerHz = centerHz;
    m_bwHz     = bwHz;

    if (m_avgMags.size() != mags.size()) {
        m_avgMags  = mags;
        m_peakHold = mags;
    } else {
        // Exponential average (alpha = 0.3)
        for (int i = 0; i < mags.size(); i++) {
            m_avgMags[i]  = 0.3f * mags[i] + 0.7f * m_avgMags[i];
            if (mags[i] > m_peakHold[i])
                m_peakHold[i] = mags[i];
            else
                m_peakHold[i] -= 0.15f; // decay dB/frame
        }
    }
    m_mags = mags;
    update();
}

void SpectrumWidget::setCursorFreq(double hz) {
    m_markerHz = hz;
    update();
}

double SpectrumWidget::pixToFreq(int x) const {
    double startHz = m_centerHz - m_bwHz / 2.0;
    return startHz + (double)x / width() * m_bwHz;
}

int SpectrumWidget::freqToPix(double hz) const {
    double startHz = m_centerHz - m_bwHz / 2.0;
    return (int)((hz - startHz) / m_bwHz * width());
}

void SpectrumWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    int W = width(), H = height();
    const int MARGIN_L = 48, MARGIN_B = 22, MARGIN_T = 10, MARGIN_R = 8;
    int plotW = W - MARGIN_L - MARGIN_R;
    int plotH = H - MARGIN_T - MARGIN_B;

    // Background
    QLinearGradient bg(0, 0, 0, H);
    bg.setColorAt(0, QColor(8, 14, 26));
    bg.setColorAt(1, QColor(4, 8, 16));
    p.fillRect(rect(), bg);

    // Grid
    p.setPen(QPen(QColor(30, 60, 80), 1, Qt::DotLine));
    // Horizontal dB lines
    int dbSteps = 10;
    for (int i = 0; i <= dbSteps; i++) {
        int y = MARGIN_T + (int)((float)i / dbSteps * plotH);
        p.drawLine(MARGIN_L, y, W - MARGIN_R, y);
        float db = m_refLevel - (float)i / dbSteps * m_dbRange;
        p.setPen(QColor(60, 120, 150));
        p.setFont(QFont("Courier", 7));
        p.drawText(2, y + 4, QString::number((int)db));
        p.setPen(QPen(QColor(30, 60, 80), 1, Qt::DotLine));
    }

    // Vertical freq lines (5 divisions)
    double startHz = m_centerHz - m_bwHz / 2.0;
    int freqDivs = 8;
    for (int i = 0; i <= freqDivs; i++) {
        int x = MARGIN_L + (int)((float)i / freqDivs * plotW);
        p.drawLine(x, MARGIN_T, x, H - MARGIN_B);
        double fMHz = (startHz + (double)i / freqDivs * m_bwHz) / 1e6;
        p.setPen(QColor(60, 120, 150));
        p.setFont(QFont("Courier", 7));
        p.drawText(x - 18, H - MARGIN_B + 14, QString::number(fMHz, 'f', 2));
        p.setPen(QPen(QColor(30, 60, 80), 1, Qt::DotLine));
    }

    if (m_avgMags.isEmpty()) return;

    int N = m_avgMags.size();
    auto dbToY = [&](float db) -> int {
        float norm = (m_refLevel - db) / m_dbRange;
        norm = std::max(0.f, std::min(1.f, norm));
        return MARGIN_T + (int)(norm * plotH);
    };
    auto idxToX = [&](int i) -> int {
        return MARGIN_L + (int)((float)i / N * plotW);
    };

    // Peak hold (subtle cyan dots)
    p.setPen(QPen(QColor(0, 255, 220, 80), 1));
    for (int i = 0; i < N; i++) {
        int x = idxToX(i);
        int y = dbToY(m_peakHold[i]);
        p.drawPoint(x, y);
    }

    // Spectrum fill gradient
    QPainterPath fill;
    fill.moveTo(MARGIN_L, H - MARGIN_B);
    for (int i = 0; i < N; i++) {
        int x = idxToX(i);
        int y = dbToY(m_avgMags[i]);
        if (i == 0) fill.lineTo(x, y);
        else fill.lineTo(x, y);
    }
    fill.lineTo(W - MARGIN_R, H - MARGIN_B);
    fill.closeSubpath();

    QLinearGradient fillGrad(0, MARGIN_T, 0, H - MARGIN_B);
    fillGrad.setColorAt(0.0, QColor(0, 220, 180, 160));
    fillGrad.setColorAt(0.4, QColor(0, 150, 255, 80));
    fillGrad.setColorAt(1.0, QColor(0, 80, 160, 10));
    p.fillPath(fill, fillGrad);

    // Spectrum line
    QPainterPath line;
    for (int i = 0; i < N; i++) {
        int x = idxToX(i);
        int y = dbToY(m_avgMags[i]);
        if (i == 0) line.moveTo(x, y);
        else line.lineTo(x, y);
    }
    // Glow pass
    p.setPen(QPen(QColor(0, 255, 200, 40), 4));
    p.drawPath(line);
    // Sharp line
    p.setPen(QPen(QColor(0, 255, 180, 230), 1.5));
    p.drawPath(line);

    // Marker line (tuned frequency)
    int mx = freqToPix(m_markerHz) + MARGIN_L - (int)((float)MARGIN_L / W * plotW);
    // Recalculate properly
    double mxNorm = (m_markerHz - startHz) / m_bwHz;
    mx = MARGIN_L + (int)(mxNorm * plotW);
    if (mx >= MARGIN_L && mx <= W - MARGIN_R) {
        // Glow
        QLinearGradient markerGlow(mx-6, 0, mx+6, 0);
        markerGlow.setColorAt(0, Qt::transparent);
        markerGlow.setColorAt(0.5, QColor(255, 120, 0, 100));
        markerGlow.setColorAt(1, Qt::transparent);
        p.fillRect(mx-6, MARGIN_T, 12, plotH, markerGlow);
        // Line
        p.setPen(QPen(QColor(255, 140, 0, 200), 1.5, Qt::SolidLine));
        p.drawLine(mx, MARGIN_T, mx, H - MARGIN_B);
        // Frequency label
        p.setPen(QColor(255, 200, 80));
        p.setFont(QFont("Courier", 8, QFont::Bold));
        QString flabel = QString::number(m_markerHz / 1e6, 'f', 4) + " MHz";
        int lx = std::min(mx + 4, W - MARGIN_R - 80);
        p.drawText(lx, MARGIN_T + 14, flabel);
    }

    // Border
    p.setPen(QPen(QColor(0, 180, 140, 120), 1));
    p.drawRect(MARGIN_L, MARGIN_T, plotW, plotH);
}

void SpectrumWidget::mousePressEvent(QMouseEvent* e) {
    double hz = pixToFreq(e->pos().x() - 48);
    m_markerHz = hz;
    emit frequencyClicked(hz);
    update();
}

void SpectrumWidget::mouseMoveEvent(QMouseEvent* e) {
    if (e->buttons() & Qt::LeftButton) {
        double hz = pixToFreq(e->pos().x() - 48);
        m_markerHz = hz;
        emit frequencyClicked(hz);
        update();
    }
}

void SpectrumWidget::resizeEvent(QResizeEvent*) { update(); }
