#ifndef AUDIOENGINEADAPTER_H
#define AUDIOENGINEADAPTER_H

#include "compat34.h"
#include "cxaudio.hpp"

/* Qt wrapper around cxaudio::Player. Engine callbacks arrive on worker threads;
 * they are marshalled to this object's thread (the UI thread) via posted events,
 * then re-emitted as Qt signals. */
class AudioEngineAdapter : public QObject {
    Q_OBJECT
public:
    explicit AudioEngineAdapter(QObject* parent = 0);
    ~AudioEngineAdapter();

    /* id: "auto" | "ffmpeg" | "faad" | "mpv" — takes effect on next open() */
    void setBackend(const QString& id);
    QString backendId() const;
    QString lastError() const;

    void open(const QString& uri);
    void play();
    void pause();
    void toggle();
    void stop();
    void seek(int ms);
    void setVolume(int v);

    int position() const;
    int duration() const;
    int state() const;

signals:
    void positionChanged(int ms);
    void durationChanged(int ms);
    void stateChanged(int state);
    void finished();
    void errorOccurred(const QString& msg);

protected:
    void customEvent(CustomEventBase* e);

private:
    cxaudio::Player m_player;
};

#endif // AUDIOENGINEADAPTER_H
