#pragma once
#include <QWidget>
#include <QImage>
#include <QVector>

class WaterfallWidget : public QWidget {
    Q_OBJECT
public:
    explicit WaterfallWidget(QWidget* parent = nullptr);
    void addLine(const QVector<float>& mags, double centerHz, double bwHz);
    void setMarkerHz(double hz) { m_markerHz = hz; update(); }

signals:
    void frequencyClicked(double hz);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent*) override;

private:
    QRgb magToColor(float db) const;
    double pixToFreq(int x) const;

    QImage m_image;
    double m_centerHz = 98.1e6;
    double m_bwHz     = 2048000.0;
    double m_markerHz = 98.1e6;
    float  m_minDb    = -80.f;
    float  m_maxDb    = 0.f;
};
