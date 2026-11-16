#include "audio/AudioManager.hpp"
#include "AssetCache.hpp"
#include <fstream>
#include <iostream>

static const char *KNOWN_SFX[] = {
    "sparkle", "bounce", "shimmer_hold", "burst", "reveal", "lensflare", "twinkle",
    "whoosh", "click", "levelup", "hatch", "incant", "death", "pickup",
    "mew_cry", "mew_float", "mew_fly", "mew_meow_a", "mew_meow_b", "mew_meow_c",
};

bool AudioManager::_resolve(const std::string &name, std::string &outPath) const
{

    for (const char *ext : { ".ogg", ".wav", ".flac" }) {
        std::string p = _dir + "/" + name + ext;
        std::ifstream f(p);
        if (f.good()) { outPath = p; return true; }
    }
    outPath = _dir + "/" + name + ".wav";
    return true;
}

void AudioManager::init(const std::string &dir)
{
    _dir = dir;
    _voices.resize(16);

    int loaded = 0;
    for (const char *name : KNOWN_SFX) {
        std::string path;
        if (!_resolve(name, path)) continue;
        std::string bytes = zappy::assetBytes(path);
        sf::SoundBuffer buf;
        if (!bytes.empty() && buf.loadFromMemory(bytes.data(), bytes.size())) {
            _buffers[name] = std::move(buf); ++loaded;
        }
    }
    std::cout << "[audio] " << loaded << " sfx loaded from " << dir << "\n";
}

void AudioManager::playMusic(const std::string &name, bool loop, float volume)
{
    if (_muted) return;
    std::string path;
    if (!_resolve(name, path)) return;

    const std::string &b = zappy::assetBytesKept(path);
    if (b.empty() || !_music.openFromMemory(b.data(), b.size())) return;
    _music.setLoop(loop);
    _music.setVolume(volume);
    _music.play();
}

void AudioManager::stopMusic() { _music.stop(); }

void AudioManager::setMusicVolume(float v)
{
    _musicMaster = v < 0.f ? 0.f : v > 100.f ? 100.f : v;
    _music.setVolume(_musicMaster);
}

void AudioManager::playLoop(const std::string &name, float volume)
{
    if (_muted) return;
    auto it = _buffers.find(name);
    if (it == _buffers.end()) return;
    _loopVoice.setBuffer(it->second);
    _loopVoice.setLoop(true);
    _loopVoice.setVolume(volume);
    _loopVoice.play();
}

void AudioManager::setLoopVolume(float volume) { _loopVoice.setVolume(volume); }

void AudioManager::stopLoop() { _loopVoice.stop(); }

void AudioManager::play(const std::string &name, float volume)
{
    if (_muted) return;
    auto it = _buffers.find(name);
    if (it == _buffers.end()) return;

    sf::Sound *voice = nullptr;
    for (auto &v : _voices)
        if (v.getStatus() != sf::Sound::Playing) { voice = &v; break; }
    if (!voice) voice = &_voices.front();

    voice->setBuffer(it->second);
    voice->setVolume(volume * _sfxMaster / 100.f);
    voice->play();
}
