#include "cxaudio.hpp"

#include "core.h"

#include <vector>

namespace cxaudio {

struct Player::Impl {
    cx::Core core;
};

Player::Player() : d_(new Impl()) {}

Player::~Player() { delete d_; }

bool Player::setBackend(const std::string& id) { return d_->core.setBackend(id); }

bool Player::open(const std::string& uri) { return d_->core.open(uri); }

void Player::play() { d_->core.play(); }
void Player::pause() { d_->core.pause(); }
void Player::stop() { d_->core.stop(); }
void Player::seek(int64_t ms) { d_->core.seek(ms); }
void Player::setVolume(int volume) { d_->core.setVolume(volume); }

int64_t Player::position() const { return d_->core.position(); }
int64_t Player::duration() const { return d_->core.duration(); }
int Player::state() const { return d_->core.state(); }

std::string Player::backendId() const { return d_->core.backendId(); }
std::string Player::lastError() const { return d_->core.lastError(); }

std::vector<AudioDeviceInfo> Player::listOutputDevices() {
    std::vector<AudioDeviceInfo> out;
    std::vector<cx::AudioDeviceInfo> devs = d_->core.listOutputDevices();
    for (size_t i = 0; i < devs.size(); ++i) {
        AudioDeviceInfo d;
        d.name = devs[i].name;
        d.id = devs[i].id;
        d.isDefault = devs[i].isDefault;
        out.push_back(d);
    }
    return out;
}

void Player::setOutputDeviceName(const std::string& name) {
    d_->core.setOutputDeviceName(name);
}

void Player::setCallback(Callback cb) {
    if (cb) {
        d_->core.setCallback(
            [cb](int ev, int64_t a, int64_t b, const std::string& msg) { cb(ev, a, b, msg); });
    } else {
        d_->core.setCallback(nullptr);
    }
}

} /* namespace cxaudio */
