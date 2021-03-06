#pragma once
#include "Objects/KeyboardShortcuts.h"

#include <SDL2/SDL_video.h>
#include <SDL2/SDL_events.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <map>

namespace ed
{
	class InterfaceManager;
	class CreateItemUI;
	class UIView;
	class Settings;

	enum class ViewID
	{
		Pinned,
		Preview,
		Code,
		Output,
		Objects,
		Pipeline,
		Properties,
		Options
	};

	class GUIManager
	{
	public:
		GUIManager(ed::InterfaceManager* objs, SDL_Window* wnd, SDL_GLContext* gl);
		~GUIManager();

		void OnEvent(const SDL_Event& e);
		void Update(float delta);
		void Render();

		UIView* Get(ViewID view);
		inline SDL_Window* GetSDLWindow() { return m_wnd; }

		void SaveSettings();
		void LoadSettings();

		void CreateNewShaderPass();
		void CreateNewTexture();
		inline void CreateNewCubemap() { m_isCreateCubemapOpened = true; }
		void CreateNewAudio();
		inline void CreateNewRenderTexture() { m_isCreateRTOpened = true; }

		bool SaveAsProject(bool restoreCached = false);
		void Open(const std::string& file);

	private:
		void m_setupShortcuts();

		void m_imguiHandleEvent(const SDL_Event& e);

		void m_renderOptions();
		bool m_optionsOpened;
		int m_optGroup;
		UIView* m_options;

		std::string m_cachedFont;
		int m_cachedFontSize;
		bool m_fontNeedsUpdate;
		float m_cacheUIScale;

		bool m_isCreateItemPopupOpened, m_isCreateRTOpened, m_isCreateCubemapOpened, m_isNewProjectPopupOpened, m_isAboutOpen;

		Settings* m_settingsBkp;
		std::map<std::string, KeyboardShortcuts::Shortcut> m_shortcutsBkp;

		bool m_savePreviewPopupOpened;
		std::string m_previewSavePath;
		glm::ivec2 m_previewSaveSize;

		bool m_performanceMode, m_perfModeFake;

		std::string m_selectedTemplate;
		std::vector<std::string> m_templates;
		void m_loadTemplateList();

		std::vector<UIView*> m_views;
		CreateItemUI* m_createUI;

		InterfaceManager* m_data;
		SDL_Window* m_wnd;
		SDL_GLContext* m_gl;
	};
}