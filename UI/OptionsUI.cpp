#include "OptionsUI.h"
#include "CodeEditorUI.h"
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include "../Objects/Logger.h"
#include "../Objects/Settings.h"
#include "../Objects/ThemeContainer.h"
#include "../Objects/KeyboardShortcuts.h"
#include "UIHelper.h"

#include <algorithm>
#include <ghc/filesystem.hpp>
#include <glm/gtc/type_ptr.hpp>

#define REFRESH_BUTTON_SPACE -80 * Settings::Instance().DPIScale

namespace ed
{
	void OptionsUI::OnEvent(const SDL_Event& e)
	{
		if (m_page == Page::Shortcuts && m_selectedShortcut != -1) {
			if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
				m_newShortcut.Alt = e.key.keysym.mod & KMOD_ALT;
				m_newShortcut.Ctrl = e.key.keysym.mod & KMOD_CTRL;
				m_newShortcut.Shift = e.key.keysym.mod & KMOD_SHIFT;

				if (e.key.keysym.sym != SDLK_LALT && e.key.keysym.sym != SDLK_LSHIFT && e.key.keysym.sym != SDLK_LCTRL && e.key.keysym.sym != SDLK_RALT && e.key.keysym.sym != SDLK_RSHIFT && e.key.keysym.sym != SDLK_RCTRL) {
					std::string name = KeyboardShortcuts::Instance().GetNameList()[m_selectedShortcut];
					if (name.find("Editor") == std::string::npos || m_newShortcut.Alt == true || m_newShortcut.Ctrl == true || m_newShortcut.Shift == true) {
						if (m_newShortcut.Key1 == -1)
							m_newShortcut.Key1 = e.key.keysym.sym;
						else if (m_newShortcut.Key2 == -1)
							m_newShortcut.Key2 = e.key.keysym.sym;
						else {
							m_newShortcut.Key1 = e.key.keysym.sym;
							m_newShortcut.Key2 = -1;
						}
					}
					else {
						m_newShortcut.Key1 = -1;
						m_newShortcut.Key2 = -1;
					}
				}
				else {
					m_newShortcut.Key1 = -1;
					m_newShortcut.Key2 = -1;
				}
			}
		}
	}

	void OptionsUI::Update(float delta)
	{
		ImGui::BeginChild("##opt_container", ImVec2(0, -30));

		if (m_page == Page::General)
			m_renderGeneral();
		else if (m_page == Page::Editor)
			m_renderEditor();
		else if (m_page == Page::Shortcuts)
			m_renderShortcuts();
		else if (m_page == Page::Preview)
			m_renderPreview();
		else if (m_page == Page::Project)
			m_renderProject();

		ImGui::EndChild();
	}

	void OptionsUI::ApplyTheme()
	{
		Logger::Get().Log("Applying a UI theme to SHADERed...");

		std::string theme = Settings::Instance().Theme;
		CodeEditorUI* editor = ((CodeEditorUI*)m_ui->Get(ViewID::Code));

		if (theme == "Dark") {
			ImGuiStyle& style = ImGui::GetStyle();
			style = ImGuiStyle();

			ImGui::StyleColorsDark();

			style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
		}
		else if (theme == "Light") {
			ImGuiStyle& style = ImGui::GetStyle();
			style = ImGuiStyle();

			ImGui::StyleColorsLight();
		}
		else
			ImGui::GetStyle() = ThemeContainer::Instance().GetUIStyle(theme);

		editor->SetTheme(ThemeContainer::Instance().GetTextEditorStyle(theme));
	}

	std::string OptionsUI::m_getShortcutString()
	{
		std::string ret = "";

		if (m_newShortcut.Ctrl)
			ret += "CTRL+";
		if (m_newShortcut.Alt)
			ret += "ALT+";
		if (m_newShortcut.Shift)
			ret += "SHIFT+";
		if (m_newShortcut.Key1 != -1)
			ret += std::string(SDL_GetKeyName(m_newShortcut.Key1)) + "+";
		if (m_newShortcut.Key2 != -1)
			ret += std::string(SDL_GetKeyName(m_newShortcut.Key2)) + "+";

		if (ret.size() == 0)
			return "";

		return ret.substr(0, ret.size() - 1);
	}

	void OptionsUI::m_loadThemeList()
	{
		Logger::Get().Log("Loading a theme list...");

		m_themes.clear();
		m_themes.push_back("Dark");
		m_themes.push_back("Light");

		for (const auto & entry : ghc::filesystem::directory_iterator("./themes/")) {
			std::string file = entry.path().filename().native();
			m_themes.push_back(ThemeContainer::Instance().LoadTheme(file));
		}
	}

	void OptionsUI::m_renderGeneral()
	{
		Settings* settings = &Settings::Instance();

		/* VSYNC: */
		ImGui::Text("VSync: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##optg_vsync", &settings->General.VSync))
			SDL_GL_SetSwapInterval(settings->General.VSync);

		/* THEME */
		ImGui::Text("Theme: "); ImGui::SameLine();
		ImGui::PushItemWidth(REFRESH_BUTTON_SPACE);
		if (ImGui::BeginCombo("##optg_theme", settings->Theme.c_str())) {
			for (int i = 0; i < 2; i++)
				if (ImGui::Selectable(m_themes[i].c_str(), m_themes[i] == settings->Theme)) {
					settings->Theme = m_themes[i];
					ApplyTheme();
				}
			if (m_themes.size() > 2) ImGui::Separator();

			for (int i = 2; i < m_themes.size(); i++)
				if (ImGui::Selectable(m_themes[i].c_str(), m_themes[i] == settings->Theme)) {
					settings->Theme = m_themes[i];
					ApplyTheme();
				}

			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("REFRESH", ImVec2(-1, 0))) {
			m_loadThemeList();
			ApplyTheme();
		}



		/* AUTO ERROR SHOW: */
		ImGui::Text("Show error list when build finishes with an error: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_autoerror", &settings->General.AutoOpenErrorWindow);

		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

		/* RECOVERY: */
		ImGui::Text("Save recovery file every 10mins: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_recovery", &settings->General.Recovery);

		/* CHECK FOR UPDATES: */
		ImGui::Text("Check for updates on startup: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_checkupdates", &settings->General.CheckUpdates);

		ImGui::PopStyleVar();
		ImGui::PopItemFlag();

		/* TRACK FILE CHANGES: */
		ImGui::Text("Recompile shader on file change: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_trackfilechange", &settings->General.RecompileOnFileChange);

		/* REOPEN: */
		ImGui::Text("Reopen shaders after openning a project: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_reopen", &settings->General.ReopenShaders);

		/* USE EXTERNAL EDITOR: */
		ImGui::Text("Open shaders in an external editor: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_useexternaleditor", &settings->General.UseExternalEditor);

		/* SHADER PASS DOUBLE CLICK: */
		ImGui::Text("Double click on shader pass opens the shaders: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_opensdblclk", &settings->General.OpenShadersOnDblClk);

		/* PROPERTIES DOUBLE CLICK: */
		ImGui::Text("Open the double clicked item in properties: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_propsdblclk", &settings->General.ItemPropsOnDblCLk);

		/* SELECT DOUBLE CLICK: */
		ImGui::Text("Select the double clicked item: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_selectdblclk", &settings->General.SelectItemOnDblClk);

		/* STARTUP TEMPLATE: */
		ImGui::Text("Default template: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		if (ImGui::BeginCombo("##optg_template", settings->General.StartUpTemplate.c_str())) {
			for (const auto & entry : ghc::filesystem::directory_iterator("./templates")) {
				std::string file = entry.path().filename().native();
				if (file[0] != '.' && ImGui::Selectable(file.c_str(), file == settings->General.StartUpTemplate))
					settings->General.StartUpTemplate = file;
			}
			ImGui::EndCombo();
		}
		ImGui::PopItemWidth();

		/* EXTENSIONS: */
		ImGui::Text("HLSL extensions: ");
		ImGui::SameLine();
		ImGui::Indent(150 * settings->DPIScale);
		static char hlslExtEntry[64] = { 0 };
		if (ImGui::ListBoxHeader("##optg_hlslexts",ImVec2(100 * settings->DPIScale, 100 * settings->DPIScale))) {
			for (auto& ext : settings->General.HLSLExtensions)
				if (ImGui::Selectable(ext.c_str()))
					strcpy(hlslExtEntry, ext.c_str());
			ImGui::ListBoxFooter();
		}
		ImGui::PushItemWidth(100 * settings->DPIScale);
		ImGui::InputText("##optg_glslext_inp",hlslExtEntry,64);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("ADD##optg_btnaddext")) {
			int exists = -1;
			std::string hlslExtEntryStr(hlslExtEntry);
			for (int i = 0; i < settings->General.HLSLExtensions.size(); i++)
				if (settings->General.HLSLExtensions[i] == hlslExtEntryStr) {
					exists = i;
					break;
				}
			if (exists == -1 && hlslExtEntryStr.size() >= 1)
				settings->General.HLSLExtensions.push_back(hlslExtEntryStr);
			else
				settings->General.HLSLExtensions.erase(settings->General.HLSLExtensions.begin() + exists);
		}
		ImGui::SameLine();
		if (ImGui::Button("REMOVE##optg_btnremext")) {
			std::string glslExtEntryStr(hlslExtEntry);
			for (int i = 0; i < settings->General.HLSLExtensions.size(); i++)
				if (settings->General.HLSLExtensions[i] == glslExtEntryStr) {
					settings->General.HLSLExtensions.erase(settings->General.HLSLExtensions.begin() + i);
					break;
				}
		}
		ImGui::Unindent(150 * settings->DPIScale);


		/* WORKSPACE STUFF */
		ImGui::NewLine();
		ImGui::Separator();
		ImGui::NewLine();

		/* FONT: */
		ImGui::Text("Font: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(REFRESH_BUTTON_SPACE);
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::InputText("##optw_font", settings->General.Font, 256);
		ImGui::PopItemFlag();
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("...", ImVec2(-1, 0))) {
			std::string file;
			bool success = UIHelper::GetOpenFileDialog(file, "ttf;otf");
			if (success) {
				file = ghc::filesystem::relative(file).generic_string();
				strcpy(settings->General.Font, file.c_str());
			}
		}


		/* FONT SIZE: */
		ImGui::Text("Font size: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		if (ImGui::InputInt("##optw_fontsize", &settings->General.FontSize, 1, 5))
			settings->General.FontSize = std::max<int>(std::min<int>(settings->General.FontSize, 72), 9);
		ImGui::PopItemWidth();

		/* DPI AWARE: */
		ImGui::Text("DPI aware: ");
		ImGui::SameLine();
		if (ImGui::Checkbox("##optw_autoscale", &settings->General.AutoScale)) {
			if (settings->General.AutoScale) {
				// get dpi
				float dpi;
				int wndDisplayIndex = SDL_GetWindowDisplayIndex(m_ui->GetSDLWindow());
				SDL_GetDisplayDPI(wndDisplayIndex, NULL, &dpi, NULL);
				dpi /= 96.0f;
				
				settings->TempScale = dpi;
			} else
				settings->TempScale = 1;
		}

		/* UI SCALE (IF NOT USING DPI AWARENESS) */
		if (!settings->General.AutoScale) {
			ImGui::Text("UI scale: ");
			ImGui::SameLine();
			ImGui::SliderFloat("##optw_uiscale", &settings->TempScale, 0.1f, 3.0f);
		} else {
			ImGui::SameLine();
			ImGui::TextDisabled("   (scale=%.4f)", settings->TempScale);
		}

		/* LOGGER STUFF */
		ImGui::NewLine();
		ImGui::Separator();
		ImGui::NewLine();

		/* LOG: */
		ImGui::Text("Log: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_logs", &settings->General.Log);

		if (!settings->General.Log) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		/* PIPE LOGS: */
		ImGui::Text("Write log messages to console window: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optg_terminallogs", &settings->General.PipeLogsToTerminal);

		if (!settings->General.Log) {
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
		}
	}
	void OptionsUI::m_renderEditor()
	{
		Settings* settings = &Settings::Instance();


		/* SMART PREDICTIONS: */
		ImGui::Text("Smart predictions: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_smart_pred", &settings->Editor.SmartPredictions);

		/* SHOW WHITESPACE: */
		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

		ImGui::Text("Show whitespace: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_show_whitespace", &settings->Editor.ShowWhitespace);

		ImGui::PopStyleVar();
		ImGui::PopItemFlag();

		/* FONT: */
		ImGui::Text("Font: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(REFRESH_BUTTON_SPACE);
		ImGui::InputText("##opte_font", settings->Editor.Font, 256);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("...", ImVec2(-1, 0))) {
			std::string file;
			bool success = UIHelper::GetOpenFileDialog(file, "ttf;otf");
			if (success)
				strcpy(settings->Editor.Font, file.c_str());
		}


		/* FONT SIZE: */
		ImGui::Text("Font size: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		if (ImGui::InputInt("##opte_fontsize", &settings->Editor.FontSize, 1, 5))
			settings->Editor.FontSize = std::max<int>(std::min<int>(settings->Editor.FontSize, 72), 9);
		ImGui::PopItemWidth();

		/* AUTO BRACE COMPLETION: */
		ImGui::Text("Brace completion: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_autobrace", &settings->Editor.AutoBraceCompletion);

		/* LINE NUMBERS: */
		ImGui::Text("Line numbers: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_ln_numbers", &settings->Editor.LineNumbers);

		/* STATUS BAR: */
		ImGui::Text("Status bar: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_statusbar", &settings->Editor.StatusBar);

		/* HORIZONTAL SCROLL BAR: */
		ImGui::Text("Enable horizontal scroll bar: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_horizscroll", &settings->Editor.HorizontalScroll);

		/* HIGHLIGHT CURRENT LINE: */
		ImGui::Text("Highlight current line: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_highlight_cur_ln", &settings->Editor.HiglightCurrentLine);

		/* SMART INDENTING: */
		ImGui::Text("Smart indenting: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_smartindent", &settings->Editor.SmartIndent);

		/* INSERT SPACES: */
		ImGui::Text("Insert spaces on tab press: ");
		ImGui::SameLine();
		ImGui::Checkbox("##opte_tabspace", &settings->Editor.InsertSpaces);

		/* TAB SIZE: */
		ImGui::Text("Tab size: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		if (ImGui::InputInt("##opte_tabsize", &settings->Editor.TabSize, 1, 2))
			settings->Editor.TabSize = std::max<int>(std::min<int>(settings->Editor.TabSize, 12), 1);
		ImGui::PopItemWidth();
	}
	void OptionsUI::m_renderShortcuts()
	{
		std::vector<std::string> names = KeyboardShortcuts::Instance().GetNameList();

		ImGui::Columns(2);

		for (int i = 0; i < names.size(); i++) {
			ImGui::Text(names[i].c_str());
			ImGui::NextColumn();

			if (m_selectedShortcut == i) {
				std::string txt = m_getShortcutString();
				if (txt.size() == 0)
					txt = "-- press keys --";

				ImGui::Text(txt.c_str());
				ImGui::SameLine();
				if (ImGui::Button("ASSIGN")) {
					bool updated = KeyboardShortcuts::Instance().Set(names[i], m_newShortcut.Key1, m_newShortcut.Key2, m_newShortcut.Alt, m_newShortcut.Ctrl, m_newShortcut.Shift);
					if (!updated)
						KeyboardShortcuts::Instance().Remove(names[i]);
					m_selectedShortcut = -1;
				}
				ImGui::SameLine();
				if (ImGui::Button("CANCEL"))
					m_selectedShortcut = -1;
			}
			else {
				if (ImGui::Button(KeyboardShortcuts::Instance().GetString(names[i]).c_str(), ImVec2(-1, 0))) {
					if (ImGui::IsKeyDown(SDL_SCANCODE_LCTRL) || ImGui::IsKeyDown(SDL_SCANCODE_RCTRL))
						KeyboardShortcuts::Instance().Remove(names[i]);
					else {
						m_selectedShortcut = i;
						m_newShortcut.Ctrl = m_newShortcut.Alt = m_newShortcut.Shift = false;
						m_newShortcut.Key1 = m_newShortcut.Key2 = -1;
					}
				}
			}

			ImGui::NextColumn();

			ImGui::Separator();
		}

		ImGui::Columns(1);
	}
	void OptionsUI::m_renderPreview()
	{
		Settings* settings = &Settings::Instance();

		ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

		/* FXAA: */
		ImGui::Text("FXAA: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optp_fxaa", &settings->Preview.FXAA);

		ImGui::PopStyleVar();
		ImGui::PopItemFlag();

		/* SWITCH LEFT AND RIGHT: */
		ImGui::Text("Switch what left and right clicks do: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optp_switchlrclick", &settings->Preview.SwitchLeftRightClick);

		/* STATUS BAR: */
		ImGui::Text("Status bar: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optp_status_bar", &settings->Preview.StatusBar);

		/* SHOW BOUNDING BOX: */
		ImGui::Text("Show bounding box: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optp_bbox", &settings->Preview.BoundingBox);

		/* SHOW GIZMO: */
		ImGui::Text("Show gizmo/3d manipulators: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optp_gizmo_pick", &settings->Preview.Gizmo);

		if (!settings->Preview.Gizmo) {
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		/* SHOW GIZMO ROTATION UI: */
		ImGui::Text("Show gizmo rotation UI: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optp_gizmo_rota", &settings->Preview.GizmoRotationUI);

		/* GIZMO TRANSLATION SNAP: */
		ImGui::Text("Gizmo translation snap: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		ImGui::InputInt("##optp_transsnap", &settings->Preview.GizmoSnapTranslation, 1, 10);
		ImGui::PopItemWidth();

		/* GIZMO SCALE SNAP: */
		ImGui::Text("Gizmo scale snap: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		ImGui::InputInt("##optp_scalesnap", &settings->Preview.GizmoSnapScale, 1, 10);
		ImGui::PopItemWidth();

		/* GIZMO ROTATION SNAP: */
		ImGui::Text("Gizmo rotation snap: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		ImGui::InputInt("##optp_rotasnap", &settings->Preview.GizmoSnapRotation, 1, 10);
		ImGui::PopItemWidth();

		if (!settings->Preview.Gizmo) {
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
		}

		/* PROP OPEN PICKED: */
		ImGui::Text("Open picked item in property window: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optp_prop_pick", &settings->Preview.PropertyPick);

		/* FPS LIMIT: */
		ImGui::Text("FPS limit: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		ImGui::InputInt("##optp_fpslimit", &settings->Preview.FPSLimit, 1, 10);
		ImGui::PopItemWidth();

		/* APPLY THE FPS LIMIT TO WHOLE APP: */
		ImGui::Text("Apply the FPS limit to the whole application: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optp_fps_wholeapp", &settings->Preview.ApplyFPSLimitToApp);

		if (settings->Preview.ApplyFPSLimitToApp) {
			settings->Preview.LostFocusLimitFPS = false;
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		/* LIMIT TO 60FPS WHEN APP IS NOT FOCUSED: */
		ImGui::Text("Limit application to 60FPS when it is not focused: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optp_fps_notfocus", &settings->Preview.LostFocusLimitFPS);

		if (settings->Preview.ApplyFPSLimitToApp) {
			settings->Preview.LostFocusLimitFPS = false;
			ImGui::PopStyleVar();
			ImGui::PopItemFlag();
		}

	}
	void OptionsUI::m_renderProject()
	{
		Settings* settings = &Settings::Instance();

		/* FPS CAMERA: */
		ImGui::Text("First person camera: ");
		ImGui::SameLine();
		ImGui::Checkbox("##optpr_fpcamera", &settings->Project.FPCamera);

		/* CLEAR COLOR: */
		ImGui::Text("Preview window clear color: ");
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		ImGui::ColorEdit4("##optpr_clrclr", glm::value_ptr(settings->Project.ClearColor));
		ImGui::PopItemWidth();
	}
}