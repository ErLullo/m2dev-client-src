#include "stdafx.h"
#include "MaSoundInstance.h"

#include <miniaudio.c>

bool MaSoundInstance::InitFromBuffer(ma_engine& engine, const std::vector<uint8_t>& buffer, const std::string& identity)
{
	if (!m_Initialized)
	{
		ma_decoder_config decoderConfig = ma_decoder_config_init_default();
		ma_result result = ma_decoder_init_memory(buffer.data(), buffer.size(),
												  &decoderConfig, &m_Decoder);
		if (!MD_ASSERT(result == MA_SUCCESS))
		{
			TraceError("Failed to initialize decoder memory.");
			return false;
		}

		ma_sound_config soundConfig = ma_sound_config_init();
		soundConfig.pDataSource = &m_Decoder;

		result = ma_sound_init_ex(&engine, &soundConfig, &m_Sound);
		if (!MD_ASSERT(result == MA_SUCCESS))
		{
			TraceError("Failed to initialize sound.");
			return false;
		}

		m_Identity = identity;
		m_Initialized = true;
	}
	return m_Initialized;
}

// Basically c&p
bool MaSoundInstance::InitFromFile(ma_engine& engine, const std::string& filePathOnDisk)
{
	if (!m_Initialized)
	{
		ma_decoder_config decoderConfig = ma_decoder_config_init_default();
		ma_result result = ma_decoder_init_file(filePathOnDisk.c_str(), &decoderConfig, &m_Decoder);
		if (!MD_ASSERT(result == MA_SUCCESS))
		{
			TraceError("Failed to initialize sound file decoder.");
			return false;
		}

		ma_sound_config soundConfig = ma_sound_config_init();
		soundConfig.pDataSource = &m_Decoder;

		result = ma_sound_init_ex(&engine, &soundConfig, &m_Sound);
		if (!MD_ASSERT(result == MA_SUCCESS))
		{
			TraceError("Failed to initialize sound.");
			return false;
		}

		m_Identity = filePathOnDisk;
		m_Initialized = true;
	}
	return m_Initialized;
}

void MaSoundInstance::Destroy()
{
	m_Identity.clear();
	if (m_Initialized)
	{
		ma_sound_uninit(&m_Sound);
		ma_decoder_uninit(&m_Decoder);
	}
	m_Initialized = false;
	m_FadeRatePerFrame = 0.0f;
	m_BaseVolume = 0.0f;
	m_FadeCurrentPercent = 1.0f;
	m_FadeTargetPercent = 1.0f;
}

bool MaSoundInstance::IsInitialized() const
{
	return m_Initialized;
}

bool MaSoundInstance::IsPlaying() const
{
	return ma_sound_is_playing(&m_Sound) == MA_TRUE;
}

bool MaSoundInstance::Play()
{
	return m_Initialized && ma_sound_seek_to_pcm_frame(&m_Sound, 0) == MA_SUCCESS && ma_sound_start(&m_Sound) == MA_SUCCESS;
}

bool MaSoundInstance::Resume()
{
	return m_Initialized && ma_sound_start(&m_Sound) == MA_SUCCESS;
}

bool MaSoundInstance::Stop()
{
	return m_Initialized && ma_sound_stop(&m_Sound) == MA_SUCCESS;
}

void MaSoundInstance::Loop()
{
	ma_sound_set_looping(&m_Sound, MA_TRUE);
}

float MaSoundInstance::GetVolume() const
{
	return ma_sound_get_volume(&m_Sound);
}

void MaSoundInstance::SetVolume(float volume, std::optional<float> gain)
{
    m_BaseVolume = volume;

    if (gain.has_value())
        m_FadeCurrentPercent = *gain;

    ma_sound_set_volume(&m_Sound, m_BaseVolume * m_FadeCurrentPercent);
}

void MaSoundInstance::SetPitch(float pitch)
{
	ma_sound_set_pitch(&m_Sound, pitch);
}

void MaSoundInstance::SetPosition(float x, float y, float z)
{
	ma_sound_set_position(&m_Sound, x, y, z);
}

const std::string& MaSoundInstance::GetIdentity() const
{
	return m_Identity;
}

void MaSoundInstance::Config3D(bool toggle, float minDist, float maxDist)
{
	ma_sound_set_spatialization_enabled(&m_Sound, toggle);
	ma_sound_set_rolloff(&m_Sound, 1.0f);
	ma_sound_set_min_distance(&m_Sound, minDist);
	ma_sound_set_max_distance(&m_Sound, maxDist);
	ma_sound_set_attenuation_model(&m_Sound, ma_attenuation_model_linear);
}

void MaSoundInstance::Fade(float targetPercent, float secDurationFromMinMax)
{
    targetPercent = std::clamp(targetPercent, 0.0f, 1.0f);
    if (secDurationFromMinMax <= 0.0f || targetPercent == m_FadeCurrentPercent)
    {
        m_FadeCurrentPercent = targetPercent;
        ma_sound_set_volume(&m_Sound, m_BaseVolume * m_FadeCurrentPercent);
        m_FadeRatePerFrame = 0.0f;

		if (m_FadeCurrentPercent <= 0.0f)
			ma_sound_stop(&m_Sound);

        return;
    }

    m_FadeTargetPercent = targetPercent;

    //m_FadeRatePerFrame = (m_FadeTargetPercent > m_FadeCurrentPercent ? 1.0f : -1.0f) * (1.0f / CS_CLIENT_FPS / secDurationFromMinMax);
	m_FadeRatePerFrame = (m_FadeTargetPercent - m_FadeCurrentPercent) * (1.0f / CS_CLIENT_FPS / secDurationFromMinMax);
}

void MaSoundInstance::StopFading()
{
	m_FadeRatePerFrame = 0.0f;
}

bool MaSoundInstance::IsFading() const
{
	return m_FadeRatePerFrame != 0.0f;
}

void MaSoundInstance::Update()
{
	if (IsFading())
	{
		m_FadeCurrentPercent += m_FadeRatePerFrame;

		if ((m_FadeRatePerFrame > 0.0f && m_FadeCurrentPercent >= m_FadeTargetPercent) ||
			(m_FadeRatePerFrame < 0.0f && m_FadeCurrentPercent <= m_FadeTargetPercent))
		{
			m_FadeCurrentPercent = m_FadeTargetPercent;
			m_FadeRatePerFrame = 0.0f;

			if (m_FadeTargetPercent <= 0.0f)
				ma_sound_stop(&m_Sound);
		}

		ma_sound_set_volume(&m_Sound, m_BaseVolume * m_FadeCurrentPercent);
	}
}
