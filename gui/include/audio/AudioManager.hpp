#pragma once

#include <SFML/Audio.hpp>
#include <string>
#include <unordered_map>
#include <vector>

class AudioManager {
public:

    void init(const std::string &dir);

    void playMusic(const std::string &name, bool loop = true, float volume = 55.f);
    void stopMusic();

    void play(const std::string &name, float volume = 100.f);

    void playLoop(const std::string &name, float volume = 100.f);
    void setLoopVolume(float volume);
    void stopLoop();

    void setMuted(bool m) { _muted = m; if (m) stopMusic(); }
    bool muted() const { return _muted; }

    void  setMusicVolume(float v);
    float musicVolume() const { return _musicMaster; }
    void  setSfxVolume(float v) { _sfxMaster = v < 0.f ? 0.f : v > 100.f ? 100.f : v; }
    float sfxVolume()   const { return _sfxMaster; }

private:
    std::string _dir;
    bool        _muted      = false;
    float       _musicMaster = 35.f;
    float       _sfxMaster   = 80.f;
    std::unordered_map<std::string, sf::SoundBuffer> _buffers;
    std::vector<sf::Sound> _voices;
    sf::Sound   _loopVoice;
    sf::Music   _music;

    bool _resolve(const std::string &name, std::string &outPath) const;
};
