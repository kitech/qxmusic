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
    : QObject(parent), m_requested(qFromUtf8("auto"))
{
    m_player.setCallback([this](int ev, int64_t a, int64_t b, const std::string& msg) {
        QApplication::postEvent(this, new AudioEngineEvent(ev, a, b, qFromUtf8(msg)));
    });
}

AudioEngineAdapter::~AudioEngineAdapter()
{
    m_player.setCallback(cxaudio::Player::Callback());
    joinOpen();
    std::lock_guard<std::mutex> lk(m_mx);
    m_player.stop();
}

void AudioEngineAdapter::joinOpen()
{
    if (m_openThread.joinable())
        m_openThread.join();
}

void AudioEngineAdapter::setBackend(const QString& id)
{
    m_requested = id;
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

void AudioEngineAdapter::setOutputDevice(const QString& name)
{
    m_player.setOutputDeviceName(std::string(qToUtf8(name).data()));
}

std::vector<AudioEngineAdapter::OutputDevice> AudioEngineAdapter::outputDevices()
{
    std::vector<OutputDevice> list;
    std::vector<cxaudio::AudioDeviceInfo> devs = m_player.listOutputDevices();
    for (size_t i = 0; i < devs.size(); ++i) {
        OutputDevice d;
        d.name = qFromUtf8(devs[i].name.c_str());
        d.isDefault = devs[i].isDefault;
        list.push_back(d);
    }
    return list;
}

void AudioEngineAdapter::open(const QString& uri)
{
    std::lock_guard<std::mutex> lk(m_mx);
    /* Player::open 返回 bool，失败原因在 lastError()。经 EvError 事件
     * 走既有 onAudioError → 状态栏 + 停止播放态，避免“UI 已播放
     * 但引擎静默失败”的不可见状态。 */
    if (!m_player.open(qToUtf8(uri).data())) {
        QApplication::postEvent(this, new AudioEngineEvent(
            cxaudio::EvError, 0, 0, qFromUtf8(m_player.lastError())));
    }
}

void AudioEngineAdapter::openAsync(const QString& uri)
{
    const int gen = ++m_openGen;
    {
        std::lock_guard<std::mutex> lk(m_mx);
        m_player.stop();
    }
    joinOpen();
    const QByteArray ua = qToUtf8(uri);
    const std::string u(ua.data(), (size_t)ua.size());
    m_openThread = std::thread([this, u, gen]() {
        std::lock_guard<std::mutex> lk(m_mx);
        if (gen != m_openGen.load()) return;      /* 已被更新的点击替代 */
        if (m_player.open(u.c_str())) {
            if (gen == m_openGen.load()) m_player.play();
        } else {
            QApplication::postEvent(this, new AudioEngineEvent(
                cxaudio::EvError, 0, 0, qFromUtf8(m_player.lastError())));
        }
    });
}

void AudioEngineAdapter::play() { std::lock_guard<std::mutex> lk(m_mx); m_player.play(); }
void AudioEngineAdapter::pause() { std::lock_guard<std::mutex> lk(m_mx); m_player.pause(); }

void AudioEngineAdapter::toggle()
{
    std::lock_guard<std::mutex> lk(m_mx);
    int st = m_player.state();
    if (st == cxaudio::Playing)
        m_player.pause();
    else
        m_player.play();
}

void AudioEngineAdapter::stop() { std::lock_guard<std::mutex> lk(m_mx); m_player.stop(); }
void AudioEngineAdapter::seek(int ms) { std::lock_guard<std::mutex> lk(m_mx); m_player.seek(ms); }
void AudioEngineAdapter::setVolume(int v) { std::lock_guard<std::mutex> lk(m_mx); m_player.setVolume(v); }

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
