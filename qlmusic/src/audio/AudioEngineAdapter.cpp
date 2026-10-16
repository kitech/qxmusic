#include "AudioEngineAdapter.h"

#include <qapplication.h>

namespace {

const EventType34 kAudioEngineEventType = toEventType34(QEvent::User + 130);

class AudioEngineEvent : public CustomEventBase {
public:
    AudioEngineEvent(int ev, int64_t a, int64_t b, const QString& msg)
        : CustomEventBase(kAudioEngineEventType), ev(ev), a(a), b(b), msg(msg) {}
    int ev;
    int64_t a;
    int64_t b;
    QString msg;
};

} // namespace

AudioEngineAdapter::AudioEngineAdapter(QObject* parent)
    : QObject(parent)
{
    m_player.setCallback([this](int ev, int64_t a, int64_t b, const std::string& msg) {
        QApplication::postEvent(this, new AudioEngineEvent(ev, a, b, qFromUtf8(msg)));
    });
}

AudioEngineAdapter::~AudioEngineAdapter()
{
    m_player.setCallback(cxaudio::Player::Callback());
    m_player.stop();
}

void AudioEngineAdapter::setBackend(const QString& id)
{
    m_player.setBackend(qToUtf8(id).data());
}

QString AudioEngineAdapter::backendId() const
{
    return qFromUtf8(m_player.backendId());
}

QString AudioEngineAdapter::lastError() const
{
    return qFromUtf8(m_player.lastError());
}

void AudioEngineAdapter::open(const QString& uri)
{
    m_player.open(qToUtf8(uri).data());
}

void AudioEngineAdapter::play() { m_player.play(); }
void AudioEngineAdapter::pause() { m_player.pause(); }

void AudioEngineAdapter::toggle()
{
    int st = m_player.state();
    if (st == cxaudio::Playing)
        m_player.pause();
    else
        m_player.play();
}

void AudioEngineAdapter::stop() { m_player.stop(); }
void AudioEngineAdapter::seek(int ms) { m_player.seek(ms); }
void AudioEngineAdapter::setVolume(int v) { m_player.setVolume(v); }

int AudioEngineAdapter::position() const { return (int)m_player.position(); }
int AudioEngineAdapter::duration() const { return (int)m_player.duration(); }
int AudioEngineAdapter::state() const { return m_player.state(); }

void AudioEngineAdapter::customEvent(CustomEventBase* e)
{
    if (e->type() != kAudioEngineEventType) {
        QObject::customEvent(e);
        return;
    }
    AudioEngineEvent* ae = static_cast<AudioEngineEvent*>(e);
    switch (ae->ev) {
    case cxaudio::EvPosition:
        emit positionChanged((int)ae->a);
        if (ae->b > 0) emit durationChanged((int)ae->b);
        break;
    case cxaudio::EvDuration:
        emit durationChanged((int)ae->a);
        break;
    case cxaudio::EvState:
        emit stateChanged((int)ae->a);
        break;
    case cxaudio::EvEof:
        emit finished();
        break;
    case cxaudio::EvError:
        emit errorOccurred(ae->msg);
        break;
    default:
        break;
    }
}
