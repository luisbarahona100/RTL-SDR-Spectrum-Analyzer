#include "WaterfallWidget.h"
#include <QPainter>
#include <QMouseEvent>
#include <cmath>

WaterfallWidget::WaterfallWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(150);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void WaterfallWidget::resizeEvent(QResizeEvent*) {
    m_image = QImage(width(), height(), QImage::Format_RGB32);
    m_image.fill(QColor(4, 8, 16));
}

QRgb WaterfallWidget::magToColor(float db) const {
    // Map db to [0,1]
    float t = (db - m_minDb) / (m_maxDb - m_minDb);
    t = std::max(0.f, std::min(1.f, t));

    // Thermal colormap: black -> blue -> cyan -> green -> yellow -> red -> white
    struct Stop { float p; uint8_t r, g, b; };
    static const Stop stops[] = {
        {0.00f,   0,   0,  20},
        {0.15f,   0,   0, 160},
        {0.30f,   0, 180, 220},
        {0.50f,   0, 220,  80},
        {0.65f, 220, 220,   0},
        {0.80f, 255,  80,   0},
        {1.00f, 255, 255, 255},
    };
    int nStops = sizeof(stops)/sizeof(stops[0]);
    for (int i = 0; i < nStops-1; i++) {
        if (t >= stops[i].p && t <= stops[i+1].p) {
            float u = (t - stops[i].p) / (stops[i+1].p - stops[i].p);
            auto lerp = [&](uint8_t a, uint8_t b) { return (uint8_t)(a + u*(b-a)); };
            return qRgb(lerp(stops[i].r, stops[i+1].r),
                        lerp(stops[i].g, stops[i+1].g),
                        lerp(stops[i].b, stops[i+1].b));
        }
    }
    return qRgb(255,255,255);
}

void WaterfallWidget::addLine(const QVector<float>& mags, double centerHz, double bwHz) {
    m_centerHz = centerHz;
    m_bwHz     = bwHz;

    if (m_image.isNull() || m_image.width() != width() || m_image.height() != height()) {
        m_image = QImage(width(), height(), QImage::Format_RGB32);
        m_image.fill(QColor(4, 8, 16));
    }

    // Scroll image down by 1 pixel
    memmove(m_image.bits() + m_image.bytesPerLine(),
            m_image.bits(),
            (size_t)(m_image.bytesPerLine() * (m_image.height() - 1)));

    // Render new line at top
    int W = m_image.width();
    int N = mags.size();
    QRgb* line = reinterpret_cast<QRgb*>(m_image.bits());
    for (int x = 0; x < W; x++) {
        int idx = (int)((float)x / W * N);
        idx = std::max(0, std::min(N-1, idx));
        line[x] = magToColor(mags[idx]);
    }

    update();
}

double WaterfallWidget::pixToFreq(int x) const {
    double startHz = m_centerHz - m_bwHz / 2.0;
    return startHz + (double)x / width() * m_bwHz;
}

void WaterfallWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    if (!m_image.isNull()) p.drawImage(0, 0, m_image);

    // Marker
    double startHz = m_centerHz - m_bwHz / 2.0;
    double norm = (m_markerHz - startHz) / m_bwHz;
    int mx = (int)(norm * width());
    if (mx >= 0 && mx < width()) {
        p.setPen(QPen(QColor(255, 140, 0, 200), 1.5));
        p.drawLine(mx, 0, mx, height());
    }

    // Border
    p.setPen(QPen(QColor(0, 180, 140, 80), 1));
    p.drawRect(0, 0, width()-1, height()-1);
}

void WaterfallWidget::mousePressEvent(QMouseEvent* e) {
    double hz = pixToFreq(e->pos().x());
    m_markerHz = hz;
    emit frequencyClicked(hz);
    update();
}
