#pragma once
#include "EterBase/Singleton.h"
#include "Type.h"
#include "MaSoundInstance.h"

//#include <miniaudio.h>
#include <array>
#include <unordered_map>

struct SoundFile
{
	std::string name;
	std::vector<std::byte> buffer; // raw file data. 
};

class SoundEngine : public CSingleton<SoundEngine>
{
public:
	enum ESoundConfig
	{
		SOUND_INSTANCE_3D_MAX_NUM = 32,
	};
	//	enum ESoundType
	//	{
	//		SOUND_TYPE_INTERFACE, // Interface sounds. Loaded on game opening, unloaded when the game ends.
	//		SOUND_TYPE_CHARACTER, // Character sounds(hit, damage, etc). Loaded on login, unloaded when the game ends.
	//		SOUND_TYPE_MONSTER,   // monster attacks, hits, etc. Loaded and unloaded on warp
	//		SOUND_TYPE_AMBIENCE,  // Wind, rain, birds, etc. Loaded and unloaded on warp
	//		SOUND_TYPE_MUSIC,     // Bg music played on demand
	//		SOUND_TYPE_MAX_NUM,
	//	};
public:
	SoundEngine();

	~SoundEngine();
	
    enum class AmbiencePlayType
    {
        Once,
        Step,
        Loop
    };

    struct AmbienceEmitterDesc
    {
        std::string soundName;
        float x{ 0.0f };
        float y{ 0.0f };
        float z{ 0.0f };
        float range{ 0.0f };
        float maxVolumeAreaPercentage{ 0.0f }; // 0..1

        AmbiencePlayType playType{ AmbiencePlayType::Loop };
        float playInterval{ 0.0f };            // STEP only
        float playIntervalVariation{ 0.0f };   // STEP only
    };

    using AmbienceId = std::uint32_t;

	bool Initialize();

	void SetSoundVolume(float volume);

	bool PlaySound2D(const std::string& name);

	MaSoundInstance* PlaySound3D(const std::string& name, float fx, float fy, float fz, int loopCount = 1);

	MaSoundInstance* PlayAmbienceSound3D(float fx, float fy, float fz, const std::string& name, int loopCount = 1);

	void StopAllSound3D();

	void UpdateSoundInstance(float fx, float fy, float fz, uint32_t dwcurFrame, const NSound::TSoundInstanceVector* c_pSoundInstanceVector, bool checkFrequency = false);

	bool FadeInMusic(const std::string& path, float targetVolume = 1.0f, float fadeInDurationSecondsFromMin = 1.5f);

	void FadeOutMusic(const std::string& name, float targetVolume = 0.0f, float fadeOutDurationSecondsFromMax = 1.5f);

	void FadeOutAllMusic();

	void SetMusicVolume(float volume);

	float GetMusicVolume() const;

	void SaveVolume(bool isMinimized);

	void RestoreVolume();

	void SetMasterVolume(float volume);

	void SetListenerPosition(float x, float y, float z);

	void SetListenerOrientation(float forwardX, float forwardY, float forwardZ,
								float upX, float upY, float upZ);

	void Update();

    AmbienceId RegisterAmbienceEmitter(const AmbienceEmitterDesc& desc);

    void UnregisterAmbienceEmitter(AmbienceId id);

    void ClearAmbienceEmitters();

    void UpdateAmbience();

private:
	MaSoundInstance* Internal_GetInstance3D(const std::string& name);

	bool Internal_LoadSoundFromPack(const std::string& name);

    struct AmbienceEmitterInternal
    {
        AmbienceEmitterDesc desc;
        MaSoundInstance* instance{ nullptr };
        float nextPlayTime{ 0.0f };
    };

    float ComputeAmbienceVolume(float distance,
                                float range,
                                float maxVolumeAreaPercentage) const;

    void UpdateEmitterLoop(AmbienceEmitterInternal& e,
                           float distance,
                           float now);

    void UpdateEmitterOnce(AmbienceEmitterInternal& e,
                           float distance,
                           float now);

    void UpdateEmitterStep(AmbienceEmitterInternal& e,
                           float distance,
                           float now);

private:
	struct { float x, y, z; } m_CharacterPosition{};

	ma_engine m_Engine{};
	std::unordered_map<std::string, std::vector<uint8_t>> m_Files;
	std::unordered_map<std::string, MaSoundInstance> m_Sounds2D;
	std::array<MaSoundInstance, SOUND_INSTANCE_3D_MAX_NUM> m_Sounds3D;
	std::unordered_map<std::string, float> m_PlaySoundHistoryMap;

	// One song at a time, but holding both current and previous for graceful fading
	std::array<MaSoundInstance, 2> m_Music;
	int m_CurrentMusicIndex{};
	float m_MusicVolume{ 1.0f };
	float m_SoundVolume{ 1.0f };

	float m_MasterVolume{ 1.0f };
	float m_MasterVolumeFadeTarget{};
	float m_MasterVolumeFadeRatePerFrame{};

    std::unordered_map<AmbienceId, AmbienceEmitterInternal> m_AmbienceEmitters;
    AmbienceId m_NextAmbienceId{ 1 };
};
