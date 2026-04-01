#pragma once
#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <vector>
#include <deque>
#include <alsa/asoundlib.h>

class AudioOutput : public QThread {
    Q_OBJECT
public:
    explicit AudioOutput(QObject* parent = nullptr);
    ~AudioOutput();

    bool open(unsigned int rate = 48000, int channels = 1);
    void close();
    void enqueue(const std::vector<float>& samples);
    void stop();
    void setVolume(float v) { m_volume = v; }

protected:
    void run() override;

private:
    snd_pcm_t* m_pcm = nullptr;
    std::atomic<bool> m_running{false};
    std::atomic<float> m_volume{0.8f};
    unsigned int m_rate = 48000;
    int m_channels = 1;

    QMutex m_mutex;
    QWaitCondition m_cond;
    std::deque<float> m_queue;
    static constexpr size_t MAX_QUEUE = 48000 * 2; // 2 sec max
};
