#include "AudioOutput.h"
#include <QDebug>
#include <cstring>
#include <algorithm>

AudioOutput::AudioOutput(QObject* parent) : QThread(parent) {}

AudioOutput::~AudioOutput() {
    stop();
    wait(2000);
    close();
}

bool AudioOutput::open(unsigned int rate, int channels) {
    m_rate     = rate;
    m_channels = channels;

    if (snd_pcm_open(&m_pcm, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        qWarning() << "ALSA: cannot open default device";
        return false;
    }

    snd_pcm_hw_params_t* params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(m_pcm, params);
    snd_pcm_hw_params_set_access(m_pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(m_pcm, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(m_pcm, params, channels);
    snd_pcm_hw_params_set_rate_near(m_pcm, params, &m_rate, nullptr);

    snd_pcm_uframes_t bufSize = 4096;
    snd_pcm_hw_params_set_buffer_size_near(m_pcm, params, &bufSize);

    if (snd_pcm_hw_params(m_pcm, params) < 0) {
        qWarning() << "ALSA: hw_params failed";
        return false;
    }
    return true;
}

void AudioOutput::close() {
    if (m_pcm) { snd_pcm_close(m_pcm); m_pcm = nullptr; }
}

void AudioOutput::enqueue(const std::vector<float>& samples) {
    QMutexLocker lk(&m_mutex);
    if (m_queue.size() < MAX_QUEUE) {
        for (float s : samples) m_queue.push_back(s);
        m_cond.wakeOne();
    }
    // else drop (queue full = too slow)
}

void AudioOutput::stop() {
    m_running = false;
    m_cond.wakeAll();
}

void AudioOutput::run() {
    if (!m_pcm) return;
    m_running = true;

    const int PERIOD = 1024;
    std::vector<int16_t> buf(PERIOD);

    while (m_running) {
        // Pull PERIOD samples from queue
        {
            QMutexLocker lk(&m_mutex);
            while (m_running && (int)m_queue.size() < PERIOD)
                m_cond.wait(&m_mutex, 50);
            if (!m_running) break;
            float vol = m_volume.load();
            for (int i = 0; i < PERIOD; i++) {
                float s = m_queue.front() * vol;
                m_queue.pop_front();
                s = std::max(-1.f, std::min(1.f, s));
                buf[i] = static_cast<int16_t>(s * 32767.f);
            }
        }

        int rc = snd_pcm_writei(m_pcm, buf.data(), PERIOD);
        if (rc == -EPIPE) {
            snd_pcm_prepare(m_pcm);
        } else if (rc < 0) {
            qWarning() << "ALSA write error:" << snd_strerror(rc);
        }
    }
}
