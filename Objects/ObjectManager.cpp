#include "ObjectManager.h"
#include "RenderEngine.h"
#include "Logger.h"
#include "../Engine/GLUtils.h"

#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

namespace ed
{
	ObjectManager::ObjectManager(ProjectParser* parser, RenderEngine* rnd) :
		m_parser(parser), m_renderer(rnd)
	{
		m_rts.clear();
		m_binds.clear();
		m_imgSize.clear();
	}
	ObjectManager::~ObjectManager()
	{
		Clear();
	}
	void ObjectManager::Clear()
	{
		Logger::Get().Log("Clearing ObjectManager contents...");

		for (auto str : m_items) {
			glDeleteTextures(1, &m_texs[str]);

			if (IsAudio(str)) {
				if (m_audioPlayer[str].getStatus() == sf::Sound::Playing)
					m_audioPlayer[str].stop();
			}
			else if (IsRenderTexture(str)) {
				glDeleteTextures(1, &m_rts[str]->DepthStencilBuffer);
			}
		}
		
		m_rts.clear();
		m_imgSize.clear();
		m_texs.clear();
		m_binds.clear();

		m_audioData.clear();
		m_audioPlayer.clear();
		m_audioMute.clear();

		m_items.clear();
		m_isCube.clear();
	}
	bool ObjectManager::CreateRenderTexture(const std::string & name)
	{
		Logger::Get().Log("Creating a render texture " + name + " ...");

		if (Exists(name)) {
			Logger::Get().Log("Cannot create a render texture " + name + " because a rt with such name already exists", true);
			return false;
		}

		m_isCube[name] = false;
		m_items.push_back(name);

		ed::RenderTextureObject* rtObj = m_rts[name] = new ed::RenderTextureObject();
		glm::ivec2 size = m_renderer->GetLastRenderSize();

		// color texture
		glGenTextures(1, &m_texs[name]);
		glBindTexture(GL_TEXTURE_2D, m_texs[name]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		// depth texture
		glGenTextures(1, &rtObj->DepthStencilBuffer);
		glBindTexture(GL_TEXTURE_2D, rtObj->DepthStencilBuffer);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, size.x, size.y, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glBindTexture(GL_TEXTURE_2D, 0);

		m_rts[name]->FixedSize = size;
		m_rts[name]->ClearColor = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
		m_rts[name]->Name = name;

		return true;
	}
	void ObjectManager::CreateTexture(const std::string& file)
	{
		Logger::Get().Log("Creating a texture " + file + " ...");

		if (Exists(file)) {
			Logger::Get().Log("Cannot create a texture " + file + " because that texture is already added to the project", true);
			return;
		}

		m_items.push_back(file);

		std::string path = m_parser->GetProjectPath(file);
		int width, height, nrChannels;
		unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
		
		if (data == nullptr)
			Logger::Get().Log("Failed to load a texture " + file + " from file", true);

		m_isCube[file] = false;

		GLenum fmt = GL_RGB;
		if (nrChannels == 4)
			fmt = GL_RGBA;
		else if (nrChannels == 1)
			fmt = GL_LUMINANCE;

		glGenTextures(1, &m_texs[file]);
		glBindTexture(GL_TEXTURE_2D, m_texs[file]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D,0, GL_RGBA, width, height, 0, fmt, GL_UNSIGNED_BYTE, data);
		glBindTexture(GL_TEXTURE_2D, 0);

		m_imgSize[file] = std::make_pair(width, height);

		stbi_image_free(data);
	}
	bool ObjectManager::CreateCubemap(const std::string& name, const std::string& left, const std::string& top, const std::string& front, const std::string& bottom, const std::string& right, const std::string& back)
	{
		Logger::Get().Log("Creating a cubemap " + name + " ...");

		if (Exists(name)) {
			Logger::Get().Log("Cannot create a cubemap " + name + " because cubemap with such name already exists in the project", true);
			return false;
		}

		m_isCube[name] = true;
		m_items.push_back(name);
		int width, height, nrChannels;

		glGenTextures(1, &m_texs[name]);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_texs[name]);


		// left face
		unsigned char* data = stbi_load(m_parser->GetProjectPath(left).c_str(), &width, &height, &nrChannels, 0);
		GLenum fmt = GL_RGB; // get format only once -- maybe fix this in future
		if (nrChannels == 4)
			fmt = GL_RGBA;
		else if (nrChannels == 1)
			fmt = GL_LUMINANCE;
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
			0, GL_RGBA, width, height, 0, fmt, GL_UNSIGNED_BYTE, data
		);
		m_cubemaps[name].push_back(left);
		stbi_image_free(data);

		// top
		data = stbi_load(m_parser->GetProjectPath(top).c_str(), &width, &height, &nrChannels, 0);
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
			0, GL_RGBA, width, height, 0, fmt, GL_UNSIGNED_BYTE, data
		);
		m_cubemaps[name].push_back(top);
		stbi_image_free(data);

		// front
		data = stbi_load(m_parser->GetProjectPath(front).c_str(), &width, &height, &nrChannels, 0);
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
			0, GL_RGBA, width, height, 0, fmt, GL_UNSIGNED_BYTE, data
		);
		m_cubemaps[name].push_back(front);
		stbi_image_free(data);

		// bottom
		data = stbi_load(m_parser->GetProjectPath(bottom).c_str(), &width, &height, &nrChannels, 0);
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
			0, GL_RGBA, width, height, 0, fmt, GL_UNSIGNED_BYTE, data
		);
		m_cubemaps[name].push_back(bottom);
		stbi_image_free(data);

		// right
		data = stbi_load(m_parser->GetProjectPath(right).c_str(), &width, &height, &nrChannels, 0);
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_X,
			0, GL_RGBA, width, height, 0, fmt, GL_UNSIGNED_BYTE, data
		);
		m_cubemaps[name].push_back(right);
		stbi_image_free(data);

		// back
		data = stbi_load(m_parser->GetProjectPath(back).c_str(), &width, &height, &nrChannels, 0);
		glTexImage2D(
			GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
			0, GL_RGBA, width, height, 0, fmt, GL_UNSIGNED_BYTE, data
		);
		m_cubemaps[name].push_back(back);
		stbi_image_free(data);

		// properties
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		
		// clean up
		glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
		m_imgSize[name] = std::make_pair(width, height);
	}
	bool ObjectManager::CreateAudio(const std::string& file)
	{
		Logger::Get().Log("Creating audio object from file " + file + " ...");

		if (Exists(file)) {
			Logger::Get().Log("Audio object " + file + " already exists in the project", true);
			return false;
		}

		bool loaded = m_audioData[file].loadFromFile(m_parser->GetProjectPath(file));
		if (!loaded) {
			m_audioData.erase(file);
			ed::Logger::Get().Log("Failed to load an audio file " + file, true);
			return false;
		}

		glGenTextures(1, &m_texs[file]);
		glBindTexture(GL_TEXTURE_2D, m_texs[file]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 512, 2, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);

		m_items.push_back(file);

		m_audioPlayer[file].setBuffer(m_audioData[file]);
		m_audioPlayer[file].setLoop(true);
		m_audioPlayer[file].play();
		m_audioMute[file] = false;

		return true;
	}
	void ObjectManager::Update(float delta)
	{
		for (auto& it : m_audioData) {
			// get samples and fft data
			sf::Sound& player = m_audioPlayer[it.first];
			int channels = it.second.getChannelCount();
			int perChannel = it.second.getSampleCount() / channels;
			int curSample = (int)((player.getPlayingOffset().asSeconds() / it.second.getDuration().asSeconds()) * perChannel);

			double* fftData = m_audioAnalyzer.FFT(it.second, curSample);

			const sf::Int16* samples = it.second.getSamples();
			for (int i = 0; i < ed::AudioAnalyzer::SampleCount; i++) {
				sf::Int16 s = samples[std::min<int>(i + curSample, perChannel)];
				float sf = (float)s / (float)INT16_MAX;

				m_audioTempTexData[i] = fftData[i / 2];
				m_audioTempTexData[i + ed::AudioAnalyzer::SampleCount] = sf* 0.5f + 0.5f;
			}

			glBindTexture(GL_TEXTURE_2D, m_texs[it.first]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, 512, 2, 0, GL_RED, GL_FLOAT, m_audioTempTexData);
			glBindTexture(GL_TEXTURE_2D, 0);
		}
	}
	void ObjectManager::Bind(const std::string & file, PipelineItem * pass)
	{
		if (IsBound(file, pass) == -1)
			m_binds[pass].push_back(m_texs[file]);
	}
	void ObjectManager::Unbind(const std::string & file, PipelineItem * pass)
	{
		std::vector<GLuint>& srvs = m_binds[pass];
		GLuint srv = m_texs[file];

		for (int i = 0; i < srvs.size(); i++)
			if (srvs[i] == srv) {
				srvs.erase(srvs.begin() + i);
				return;
			}
	}
	void ObjectManager::Remove(const std::string & file)
	{
		GLuint srv = m_texs[file];

		for (auto& i : m_binds)
			for (int j = 0; j < i.second.size(); j++)
				if (i.second[j] == srv) {
					i.second.erase(i.second.begin() + j);
					j--;
				}

		glDeleteTextures(1, &m_texs[file]);
		if (IsRenderTexture(file)) {
			glDeleteTextures(1, &m_rts[file]->DepthStencilBuffer);

			delete m_rts[file];
			m_rts.erase(file);
		} else if (IsAudio(file)) {
			if (m_audioPlayer[file].getStatus() == sf::Sound::Playing)
				m_audioPlayer[file].stop();

			m_audioData.erase(file);
			m_audioPlayer.erase(file);
			m_audioMute.erase(file);
		} else
			m_imgSize.erase(file);


		for (int i = 0; i < m_items.size(); i++)
			if (m_items[i] == file) {
				m_items.erase(m_items.begin() + i);
				break;
			}

		m_texs.erase(file);
		m_isCube.erase(file);
	}
	int ObjectManager::IsBound(const std::string & file, PipelineItem * pass)
	{
		if (m_binds.count(pass) == 0)
			return -1;

		for (int i = 0; i < m_binds[pass].size(); i++)
			if (m_binds[pass][i] == m_texs[file]) {
				return i;
			}

		return -1;
	}
	glm::ivec2 ObjectManager::GetRenderTextureSize(const std::string & name)
	{
		if (m_rts[name]->FixedSize.x < 0) return glm::ivec2(m_rts[name]->RatioSize.x * m_renderer->GetLastRenderSize().x, m_rts[name]->RatioSize.y * m_renderer->GetLastRenderSize().y);
		return m_rts[name]->FixedSize;
	}
	bool ObjectManager::IsCubeMap(GLuint id)
	{
		for (auto& i : m_texs)
			if (i.second == id)
				return m_isCube[i.first];
		return false;
	}
	RenderTextureObject* ObjectManager::GetRenderTexture(GLuint tex)
	{
		for (auto str : m_items)
			if (m_texs[str] == tex)
				return m_rts[str];
		return nullptr;
	}
	void ObjectManager::Mute(const std::string& name)
	{
		m_audioPlayer[name].setVolume(0);
		m_audioMute[name] = true;
	}
	void ObjectManager::Unmute(const std::string& name)
	{
		m_audioPlayer[name].setVolume(100);
		m_audioMute[name] = false;
	}
	void ObjectManager::ResizeRenderTexture(const std::string & name, glm::ivec2 size)
	{
		RenderTextureObject* rtObj = this->GetRenderTexture(m_texs[name]);

		glBindTexture(GL_TEXTURE_2D, m_texs[name]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

		glBindTexture(GL_TEXTURE_2D, rtObj->DepthStencilBuffer);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, size.x, size.y, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}