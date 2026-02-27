#include "stdafx.h"
#include "SoundEngine.h"

#include "EterBase/Random.h"
#include "EterBase/Timer.h"
#include "PackLib/PackManager.h"

SoundEngine::SoundEngine()
{
}

SoundEngine::~SoundEngine()
{
    ClearAmbienceEmitters();

	for (auto& [name, instance] : m_Sounds2D)
		instance.Destroy();

	for (auto& instance : m_Sounds3D)
		instance.Destroy();

	ma_engine_uninit(&m_Engine);
	m_Files.clear();
	m_Sounds2D.clear();
}

bool SoundEngine::Initialize()
{
	if (!MD_ASSERT(ma_engine_init(NULL, &m_Engine) == MA_SUCCESS))
	{
		TraceError("SoundEngine::Initialize: Failed to initialize engine.");
		return false;
	}

	ma_engine_listener_set_position(&m_Engine, 0, 0, 0, 0); // engine
	SetListenerPosition(0.0f, 0.0f, 0.0f); // character
	SetListenerOrientation(0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);
	return true;
}

void SoundEngine::SetSoundVolume(float volume)
{
	m_SoundVolume = std::clamp<float>(volume, 0.0, 1.0);
}

bool SoundEngine::PlaySound2D(const std::string& name)
{
	if (!Internal_LoadSoundFromPack(name))
		return false;

	auto& instance = m_Sounds2D[name]; // 2d sounds are persistent, no need to destroy
	instance.InitFromBuffer(m_Engine, m_Files[name], name);
	instance.Config3D(false);
	instance.SetVolume(m_SoundVolume);
	return instance.Play();
}

MaSoundInstance* SoundEngine::PlaySound3D(const std::string& name, float fx, float fy, float fz, bool loop)
{
    if (auto instance = Internal_GetInstance3D(name))
    {
        constexpr float minDist = 100.0f;
        constexpr float maxDist = 5000.0f;

        instance->SetPosition(fx - m_CharacterPosition.x,
                              fy - m_CharacterPosition.y,
                              fz - m_CharacterPosition.z);
        instance->Config3D(true, minDist, maxDist);
        instance->SetVolume(m_SoundVolume);
        if (loop)
            instance->Loop();
        instance->Play();
        return instance;
    }
    return nullptr;
}

MaSoundInstance* SoundEngine::PlayAmbienceSound3D(float fx, float fy, float fz, const std::string& name, bool loop)
{
    return PlaySound3D(name, fx, fy, fz, loop);
}

void SoundEngine::StopAllSound3D()
{
	for (auto& instance : m_Sounds3D)
		instance.Stop();

    m_PlaySoundHistoryMap.clear();
}

void SoundEngine::UpdateSoundInstance(float fx, float fy, float fz, uint32_t dwcurFrame, const NSound::TSoundInstanceVector* c_pSoundInstanceVector, bool checkFrequency)
{
	for (uint32_t i = 0; i < c_pSoundInstanceVector->size(); ++i)
	{
		const NSound::TSoundInstance& c_rSoundInstance = c_pSoundInstanceVector->at(i);
		if (c_rSoundInstance.dwFrame == dwcurFrame)
		{
			if (checkFrequency)
			{
				float& lastPlay = m_PlaySoundHistoryMap[c_rSoundInstance.strSoundFileName];
                const float now = CTimer::Instance().GetCurrentSecond();
				const float diff = now - lastPlay;

                if (diff >= 0.0f && diff < 0.3f)
                    continue;

                lastPlay = now;
			}

			PlaySound3D(c_rSoundInstance.strSoundFileName, fx, fy, fz);
		}
	}
}

bool SoundEngine::FadeInMusic(const std::string& path, float targetVolume /* 1.0f by default */, float fadeInDurationSecondsFromMin)
{
	if (path.empty())
		return false;

	auto& fadeOutMusic = m_Music[m_CurrentMusicIndex];
	if (fadeOutMusic.IsPlaying() && path == fadeOutMusic.GetIdentity())
	{
		fadeOutMusic.Fade(targetVolume, fadeInDurationSecondsFromMin);
		return fadeOutMusic.Resume();
	}

	// We're basically just swapping
	FadeOutMusic(fadeOutMusic.GetIdentity());
	m_CurrentMusicIndex = int(!m_CurrentMusicIndex);

	auto& music = m_Music[m_CurrentMusicIndex];
	music.Destroy();
	music.InitFromFile(m_Engine, path);
	music.Config3D(false);
	music.Loop();
	music.SetVolume(0.0f);
	music.Fade(targetVolume, fadeInDurationSecondsFromMin);
	return music.Play();
}

void SoundEngine::FadeOutMusic(const std::string& name, float targetVolume, float fadeOutDurationSecondsFromMax)
{
	for (auto& music : m_Music)
	{
		if (music.GetIdentity() == name)
			music.Fade(targetVolume, fadeOutDurationSecondsFromMax);
	}
}

void SoundEngine::FadeOutAllMusic()
{
	for (auto& music : m_Music)
		FadeOutMusic(music.GetIdentity());
}

void SoundEngine::SetMusicVolume(float volume)
{
	m_MusicVolume = std::clamp<float>(volume, 0.0f, 1.0f);
	m_Music[m_CurrentMusicIndex].StopFading();
	m_Music[m_CurrentMusicIndex].SetVolume(m_MusicVolume);
}

float SoundEngine::GetMusicVolume() const
{
	return m_MusicVolume;
}

void SoundEngine::SaveVolume(bool isMinimized)
{
	constexpr float ratePerSecond = 1.0f / CS_CLIENT_FPS;
	// 1.0 to 0 in 1s if minimized, 3s if just out of focus
	const float durationOnFullVolume = isMinimized ? 1.0f : 3.0f;

	float outOfFocusVolume = 0.35f;
	if (m_MasterVolume <= outOfFocusVolume)
		outOfFocusVolume = m_MasterVolume;

	m_MasterVolumeFadeTarget = isMinimized ? 0.0f : outOfFocusVolume;
	m_MasterVolumeFadeRatePerFrame = -ratePerSecond / durationOnFullVolume;
}

void SoundEngine::RestoreVolume()
{
	constexpr float ratePerSecond = 1.0f / CS_CLIENT_FPS;
	constexpr float durationToFullVolume = 4.0f; // 0 to 1.0 in 4s
	m_MasterVolumeFadeTarget = m_MasterVolume;
	m_MasterVolumeFadeRatePerFrame = ratePerSecond / durationToFullVolume;
}

void SoundEngine::SetMasterVolume(float volume)
{
	m_MasterVolume = volume;
	ma_engine_set_volume(&m_Engine, volume);
}

void SoundEngine::SetListenerPosition(float x, float y, float z)
{
	m_CharacterPosition.x = x;
	m_CharacterPosition.y = y;
	m_CharacterPosition.z = z;
}

void SoundEngine::SetListenerOrientation(float forwardX, float forwardY, float forwardZ,
										 float upX, float upY, float upZ)
{
	ma_engine_listener_set_direction(&m_Engine, 0, forwardX, forwardY, -forwardZ);
	ma_engine_listener_set_world_up(&m_Engine, 0, upX, -upY, upZ);
}

void SoundEngine::Update()
{
	for (auto& music : m_Music)
		music.Update();

	if (m_MasterVolumeFadeRatePerFrame)
	{
		float volume = ma_engine_get_volume(&m_Engine) + m_MasterVolumeFadeRatePerFrame;
		if ((m_MasterVolumeFadeRatePerFrame > 0.0f && volume >= m_MasterVolumeFadeTarget) || (m_MasterVolumeFadeRatePerFrame < 0.0f && volume <= m_MasterVolumeFadeTarget))
		{
			volume = m_MasterVolumeFadeTarget;
			m_MasterVolumeFadeRatePerFrame = 0.0f;
		}
		ma_engine_set_volume(&m_Engine, volume);
	}
}

MaSoundInstance* SoundEngine::Internal_GetInstance3D(const std::string& name)
{
	if (Internal_LoadSoundFromPack(name))
	{
		for (auto& instance : m_Sounds3D)
		{
			if (!instance.IsPlaying())
			{
				instance.Destroy();
				instance.InitFromBuffer(m_Engine, m_Files[name], name);
				return &instance;
			}
		}
	}
	return nullptr;
}

bool SoundEngine::Internal_LoadSoundFromPack(const std::string& name)
{
	if (m_Files.find(name) == m_Files.end())
	{
		TPackFile soundFile;
		if (!CPackManager::Instance().GetFile(name, soundFile))
		{
			TraceError("Internal_LoadSoundFromPack: SoundEngine: Failed to register file '%s' - not found.", name.c_str());
			return false;
		}

		auto& buffer = m_Files[name];
		buffer.resize(soundFile.size());
		memcpy(buffer.data(), soundFile.data(), soundFile.size());
	}
	return true;
}

SoundEngine::AmbienceId SoundEngine::RegisterAmbienceEmitter(const AmbienceEmitterDesc& desc)
{
    AmbienceEmitterInternal emitter;
    emitter.desc = desc;
    emitter.instance = nullptr;
    emitter.nextPlayTime = 0.0f;

    AmbienceId id = m_NextAmbienceId++;
    m_AmbienceEmitters.emplace(id, std::move(emitter));
    return id;
}

void SoundEngine::UnregisterAmbienceEmitter(AmbienceId id)
{
    auto it = m_AmbienceEmitters.find(id);
    if (it == m_AmbienceEmitters.end())
        return;

    AmbienceEmitterInternal& e = it->second;
    if (e.instance)
    {
        e.instance->Stop();
        e.instance = nullptr;
    }

    m_AmbienceEmitters.erase(it);
}

void SoundEngine::ClearAmbienceEmitters()
{
    for (auto& [id, e] : m_AmbienceEmitters)
    {
        if (e.instance)
        {
            e.instance->Stop();
            e.instance = nullptr;
        }
    }

    m_AmbienceEmitters.clear();
    m_NextAmbienceId = 1;
}

float SoundEngine::ComputeAmbienceVolume(float distance,
                                         float range,
                                         float maxVolumeAreaPercentage) const
{
    if (range <= 0.0f)
        return 1.0f;

    float p = std::clamp(maxVolumeAreaPercentage, 0.0f, 0.999f);
    float maxRadius = range * p;

    if (maxRadius <= 0.0f)
        return 1.0f;

    if (distance <= maxRadius)
        return 1.0f;

    float denom = (range - maxRadius);
    if (denom <= 0.0f)
        return 1.0f;

    float t = (distance - maxRadius) / denom;
    float vol = 1.0f - t;
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;
    return vol;
}

void SoundEngine::UpdateAmbience()
{
    if (m_AmbienceEmitters.empty())
        return;

    float listenerX = m_CharacterPosition.x;
    float listenerY = m_CharacterPosition.y;
    float listenerZ = m_CharacterPosition.z;
    float now = CTimer::Instance().GetCurrentSecond();

    for (auto& [id, e] : m_AmbienceEmitters)
    {
        float dx = e.desc.x - listenerX;
        float dy = e.desc.y - listenerY;
        float dz = e.desc.z - listenerZ;
        float distance = sqrtf(dx * dx + dy * dy + dz * dz);

        switch (e.desc.playType)
        {
        case AmbiencePlayType::Loop:
            UpdateEmitterLoop(e, distance, now);
            break;
        case AmbiencePlayType::Once:
            UpdateEmitterOnce(e, distance, now);
            break;
        case AmbiencePlayType::Step:
            UpdateEmitterStep(e, distance, now);
            break;
        }
    }
}

void SoundEngine::UpdateEmitterLoop(AmbienceEmitterInternal& e,
                                    float distance,
                                    float now)
{
    bool inside = (distance < e.desc.range);

    if (inside)
    {
        bool needStart = false;

        if (!e.instance)
        {
            needStart = true;
        }
        else if (!e.instance->IsPlaying())
        {
            needStart = true;
            e.instance->Stop();
            e.instance = nullptr;
        }

        if (needStart)
        {
            if (!e.desc.soundName.empty())
            {
                e.instance = PlayAmbienceSound3D(e.desc.x, e.desc.y, e.desc.z,
                                                 e.desc.soundName, true);
            }

            if (!e.instance)
            {
                return;
            }
        }

        if (e.instance && e.instance->IsPlaying())
        {
            e.instance->SetPosition(e.desc.x - m_CharacterPosition.x,
                                    e.desc.y - m_CharacterPosition.y,
                                    e.desc.z - m_CharacterPosition.z);

            float vol = ComputeAmbienceVolume(distance,
                                              e.desc.range,
                                              e.desc.maxVolumeAreaPercentage);
            e.instance->SetVolume(vol * m_SoundVolume);
        }
    }
    else
    {
        if (e.instance)
        {
            e.instance->Stop();
            e.instance = nullptr;
        }
    }
}

void SoundEngine::UpdateEmitterOnce(AmbienceEmitterInternal& e,
                                    float distance,
                                    float now)
{
    bool inside = (distance < e.desc.range);

    if (inside)
    {
        if (!e.instance || !e.instance->IsPlaying())
        {
            if (!e.desc.soundName.empty())
            {
                e.instance = PlayAmbienceSound3D(e.desc.x, e.desc.y, e.desc.z,
                                                 e.desc.soundName, false);
            }
        }
    }
    else
    {
        if (e.instance)
        {
            e.instance->Stop();
            e.instance = nullptr;
        }
    }
}

void SoundEngine::UpdateEmitterStep(AmbienceEmitterInternal& e,
                                    float distance,
                                    float now)
{
    bool inside = (distance < e.desc.range);

    if (inside)
    {
        if (now > e.nextPlayTime)
        {
            if (!e.desc.soundName.empty())
            {
                e.instance = PlayAmbienceSound3D(e.desc.x, e.desc.y, e.desc.z,
                                                 e.desc.soundName, false);
            }

            float interval = e.desc.playInterval +
                             frandom(0.0f, e.desc.playIntervalVariation);
            e.nextPlayTime = now + interval;
        }
    }
    else
    {
        if (e.instance)
        {
            e.instance->Stop();
            e.instance = nullptr;
        }
        e.nextPlayTime = 0.0f;
    }
}
