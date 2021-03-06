#include "PreviewUI.h"
#include "PropertyUI.h"
#include "PipelineUI.h"
#include "../Objects/Logger.h"
#include "../Objects/Settings.h"
#include "../Objects/DefaultState.h"
#include "../Objects/SystemVariableManager.h"
#include "../Objects/KeyboardShortcuts.h"
#include "../Engine/GeometryFactory.h"
#include "../Engine/GLUtils.h"

#include <chrono>
#include <thread>
#include <imgui/imgui_internal.h>

#define STATUSBAR_HEIGHT 25 * Settings::Instance().DPIScale
#define BUTTON_SIZE 17 * Settings::Instance().DPIScale
#define FPS_UPDATE_RATE 0.3f
#define BOUNDING_BOX_PADDING 0.01f


const char* BOX_VS_CODE = R"(
#version 330

layout (location = 0) in vec3 iPos;

uniform mat4 uMatWVP;

void main() {
	gl_Position = uMatWVP * vec4(iPos, 1.0f);
}
)";

const char* BOX_PS_CODE = R"(
#version 330

uniform vec4 uColor;
out vec4 fragColor;

void main()
{
	fragColor = uColor;
}
)";

namespace ed
{
	void PreviewUI::m_setupShortcuts()
	{
		KeyboardShortcuts::Instance().SetCallback("Gizmo.Position", [=]() {
			m_pickMode = 0;
			m_gizmo.SetMode(m_pickMode);
		});
		KeyboardShortcuts::Instance().SetCallback("Gizmo.Scale", [=]() {
			m_pickMode = 1;
			m_gizmo.SetMode(m_pickMode);
		});
		KeyboardShortcuts::Instance().SetCallback("Gizmo.Rotation", [=]() {
			m_pickMode = 2;
			m_gizmo.SetMode(m_pickMode);
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.Delete", [=]() {
			if (m_picks.size() != 0 && m_hasFocus) {
				PropertyUI* props = (PropertyUI*)m_ui->Get(ViewID::Properties);
				for (int i = 0; i < m_picks.size(); i++) {
					if (props->CurrentItemName() == m_picks[i]->Name)
						props->Open(nullptr);
					m_data->Pipeline.Remove(m_picks[i]->Name);
				}
				m_picks.clear();
			}
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.Unselect", [=]() {
			m_picks.clear();
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.Duplicate", [=]() {
			Duplicate();
		});
		KeyboardShortcuts::Instance().SetCallback("Preview.SelectAll", [=]() {
			// clear the list
			Pick(nullptr);

			// select all geometry and mesh items
			std::vector<PipelineItem*>& pass = m_data->Pipeline.GetList();
			for (int i = 0; i < pass.size(); i++) {
				ed::pipe::ShaderPass* pdata = (ed::pipe::ShaderPass*)pass[i]->Data;
				for (int j = 0; j < pdata->Items.size(); j++) {
					if (pdata->Items[j]->Type == PipelineItem::ItemType::Geometry ||
						pdata->Items[j]->Type == PipelineItem::ItemType::Model)
						Pick(pdata->Items[j], true);
				}
			}
		});
	}
	void PreviewUI::OnEvent(const SDL_Event& e)
	{
		if (e.type == SDL_MOUSEBUTTONDOWN)
			m_mouseContact = ImVec2(e.button.x, e.button.y);
		else if (e.type == SDL_MOUSEMOTION && Settings::Instance().Preview.Gizmo && m_picks.size() != 0) {
			glm::vec2 s = SystemVariableManager::Instance().GetMousePosition();
			s.x *= m_lastSize.x;
			s.y *= m_lastSize.y;
			m_gizmo.HandleMouseMove(s.x, s.y, m_lastSize.x, m_lastSize.y);
		}
		else if (e.type == SDL_MOUSEBUTTONUP) {
			SDL_CaptureMouse(SDL_FALSE);
			m_startWrap = false;
			if (Settings::Instance().Preview.Gizmo)
				m_gizmo.UnselectAxis();
		}
	}
	void PreviewUI::Pick(PipelineItem* item, bool add)
	{
		// reset variables
		m_prevScale = m_tempScale = glm::vec3(1, 1, 1);
		m_prevRota = m_tempRota = glm::vec3(0, 0, 0);

		// check if it already exists
		bool skipAdd = false;
		for (int i = 0; i < m_picks.size(); i++)
			if (m_picks[i] == item) {
				if (!add) {
					m_picks.clear();
					m_picks.push_back(item);
				}
				skipAdd = true;
				break;
			}

		// add item
		if (!skipAdd) {
			if (item == nullptr)
				m_picks.clear();
			else if (add)
				m_picks.push_back(item);
			else {
				m_picks.clear();
				m_picks.push_back(item);
			}
		}

		// calculate position
		m_prevTrans = glm::vec3(0, 0, 0);
		for (int i = 0; i < m_picks.size(); i++) {
			glm::vec3 pos(0,0,0);
			if (m_picks[i]->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* geo = (pipe::GeometryItem*)m_picks[i]->Data;
				pos = geo->Position;
			}
			else if (m_picks[i]->Type == PipelineItem::ItemType::Model) {
				pipe::Model* obj = (pipe::Model*)m_picks[i]->Data;
				pos = obj->Position;
			}
			m_prevTrans.x += pos.x;
			m_prevTrans.y += pos.y;
			m_prevTrans.z += pos.z;
		}
		if (m_picks.size() != 0) {
			m_prevTrans.x /= m_picks.size();
			m_prevTrans.y /= m_picks.size();
			m_prevTrans.z /= m_picks.size();
		}
		m_tempTrans = m_prevTrans;


		if (m_picks.size() != 0) {
			if (Settings::Instance().Preview.BoundingBox)
				m_buildBoundingBox();

			m_gizmo.SetTransform(&m_tempTrans, &m_tempScale, &m_tempRota);
		}
	}
	void PreviewUI::Update(float delta)
	{
		if (!m_data->Messages.CanRenderPreview()) {
			ImGui::TextColored(IMGUI_ERROR_COLOR, "Can not display preview - there are some errors you should fix.");
			return;
		}

		ed::Settings& settings = Settings::Instance();

		bool capWholeApp = settings.Preview.ApplyFPSLimitToApp;
		bool statusbar = settings.Preview.StatusBar;
		float fpsLimit = settings.Preview.FPSLimit;
		if (fpsLimit != m_fpsLimit) {
			m_elapsedTime = 0;
			m_fpsLimit = fpsLimit;
		}

		ImVec2 imageSize = ImVec2(ImGui::GetWindowContentRegionWidth(), abs(ImGui::GetWindowContentRegionMax().y - ImGui::GetWindowContentRegionMin().y - STATUSBAR_HEIGHT * statusbar));
		ed::RenderEngine* renderer = &m_data->Renderer;

		m_fpsUpdateTime += delta;
		m_elapsedTime += delta;
		if (capWholeApp || m_fpsLimit <= 0 || m_elapsedTime >= 1.0f / m_fpsLimit) {
			renderer->Render(imageSize.x, imageSize.y);

			float fps = m_fpsTimer.Restart();
			if (m_fpsUpdateTime > FPS_UPDATE_RATE) {
				m_fpsDelta = fps;
				m_fpsUpdateTime -= FPS_UPDATE_RATE;
			}

			m_elapsedTime -= 1 / m_fpsLimit;
		}

		if (capWholeApp && 1000 / delta > m_fpsLimit)
			std::this_thread::sleep_for(std::chrono::milliseconds(1000 / (int)m_fpsLimit - (int)(1000 * delta)));


		GLuint rtView = renderer->GetTexture();
		
		// display the image on the imgui window
		ImGui::Image((void*)rtView, imageSize, ImVec2(0,1), ImVec2(1,0));

		m_hasFocus = ImGui::IsWindowFocused();
		
		// render the gizmo if necessary
		if (m_picks.size() != 0 && settings.Preview.Gizmo) {
			// recreate render texture if size is changed
			if (m_lastSize.x != imageSize.x || m_lastSize.y != imageSize.y) {
				m_lastSize = glm::ivec2(imageSize.x, imageSize.y);

				gl::FreeSimpleFramebuffer(m_overlayFBO, m_overlayColor, m_overlayDepth);
				m_overlayFBO = gl::CreateSimpleFramebuffer(imageSize.x, imageSize.y, m_overlayColor, m_overlayDepth);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, m_overlayFBO);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // TODO: is this needed?
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			m_gizmo.SetProjectionMatrix(SystemVariableManager::Instance().GetProjectionMatrix());
			m_gizmo.SetViewMatrix(SystemVariableManager::Instance().GetCamera()->GetMatrix());
			m_gizmo.Render();

			if (settings.Preview.BoundingBox)
				m_renderBoundingBox();

			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMin().y);
			ImGui::Image((void*)m_overlayColor, imageSize, ImVec2(0, 1), ImVec2(1, 0));
		}

		// update wasd key state
		SystemVariableManager::Instance().SetKeysWASD(ImGui::IsKeyDown(SDL_SCANCODE_W), ImGui::IsKeyDown(SDL_SCANCODE_A), ImGui::IsKeyDown(SDL_SCANCODE_S), ImGui::IsKeyDown(SDL_SCANCODE_D));

		// update system variable mouse position value
		SystemVariableManager::Instance().SetMousePosition((ImGui::GetMousePos().x - ImGui::GetCursorScreenPos().x - ImGui::GetScrollX()) / imageSize.x,
			1 - (imageSize.y + (ImGui::GetMousePos().y - ImGui::GetCursorScreenPos().y - ImGui::GetScrollY())) / imageSize.y);

		// apply transformations to picked items
		if (settings.Preview.Gizmo && m_picks.size() != 0) {
			if (m_tempTrans != m_prevTrans || m_tempScale != m_prevScale || m_tempRota != m_prevRota) {
				for (int i = 0; i < m_picks.size(); i++) {
					// TODO: tidy this up
					glm::vec3 t = m_tempTrans - m_prevTrans;
					glm::vec3 s = m_tempScale - m_prevScale;
					glm::vec3 r = m_tempRota - m_prevRota;
					glm::vec3* ot = nullptr, * os = nullptr, * orot = nullptr;

					if (m_picks[i]->Type == PipelineItem::ItemType::Geometry) {
						pipe::GeometryItem* geo = (pipe::GeometryItem*)m_picks[i]->Data;
						ot = &geo->Position;
						os = &geo->Scale;
						orot = &geo->Rotation;
					}
					else if (m_picks[i]->Type == PipelineItem::ItemType::Model) {
						pipe::Model* obj = (pipe::Model*)m_picks[i]->Data;
						ot = &obj->Position;
						os = &obj->Scale;
						orot = &obj->Rotation;
					}

					if (ot != nullptr) {
						ot->x += t.x;
						ot->y += t.y;
						ot->z += t.z;
					}
					if (os != nullptr) {
						os->x += s.x;
						os->y += s.y;
						os->z += s.z;
					}
					if (orot != nullptr) {
						orot->x += r.x;
						orot->y += r.y;
						orot->z += r.z;
					}

				}

				m_prevTrans = m_tempTrans;
				m_prevRota = m_tempRota;
				m_prevScale = m_tempScale;

				m_buildBoundingBox();
			}
			else if (m_picks.size() == 1) {
				if (m_picks[0]->Type == PipelineItem::ItemType::Geometry) {
					pipe::GeometryItem* obj = (pipe::GeometryItem*)m_picks[0]->Data;
					if (obj->Position != m_tempTrans) {
						m_prevTrans = m_tempTrans = obj->Position;
						m_buildBoundingBox();
					}
					else if (obj->Scale != m_tempScale) {
						m_prevScale = m_tempScale = obj->Scale;
						m_buildBoundingBox();
					}
					else if (obj->Rotation != m_tempRota) {
						m_prevRota = m_tempRota = obj->Rotation;
						m_buildBoundingBox();
					}
				}
				else if (m_picks[0]->Type == PipelineItem::ItemType::Model) {
					pipe::Model* obj = (pipe::Model*)m_picks[0]->Data;
					if (obj->Position != m_tempTrans) {
						m_prevTrans = m_tempTrans = obj->Position;
						m_buildBoundingBox();
					}
					else if (obj->Scale != m_tempScale) {
						m_prevScale = m_tempScale = obj->Scale;
						m_buildBoundingBox();
					}
					else if (obj->Rotation != m_tempRota) {
						m_prevRota = m_tempRota = obj->Rotation;
						m_buildBoundingBox();
					}
				}
			}
		}

		// mouse controls for preview window
		if (ImGui::IsItemHovered()) {
			bool fp = settings.Project.FPCamera;

			// zoom in/out if needed
			if (!fp)
				((ed::ArcBallCamera*)SystemVariableManager::Instance().GetCamera())->Move(-ImGui::GetIO().MouseWheel);

			// handle left click - selection
			if (((ImGui::IsMouseClicked(0) && !settings.Preview.SwitchLeftRightClick) ||
				(ImGui::IsMouseClicked(1) && settings.Preview.SwitchLeftRightClick)) &&
				settings.Preview.Gizmo)
			{
				// screen space position
				glm::vec2 s = SystemVariableManager::Instance().GetMousePosition();
				s.x *= imageSize.x;
				s.y *= imageSize.y;

				bool shiftPickBegan = ImGui::GetIO().KeyShift;

				if ((m_picks.size() != 0 && m_gizmo.Click(s.x, s.y, m_lastSize.x, m_lastSize.y) == -1) || m_picks.size() == 0) {
					renderer->Pick(s.x, s.y, shiftPickBegan, [&](PipelineItem* item) {
						if (settings.Preview.PropertyPick)
							((PropertyUI*)m_ui->Get(ViewID::Properties))->Open(item);

						bool shift = ImGui::GetIO().KeyShift;

						Pick(item, shift);
					});
				}
			}

			// handle right mouse dragging - camera
			if (((ImGui::IsMouseDown(0) && settings.Preview.SwitchLeftRightClick) ||
				(ImGui::IsMouseDown(1) && !settings.Preview.SwitchLeftRightClick)))
			{
				m_startWrap = true;
				SDL_CaptureMouse(SDL_TRUE);

				int ptX, ptY;
				SDL_GetMouseState(&ptX, &ptY);

				// get the delta from the last position
				int dX = ptX - m_mouseContact.x;
				int dY = ptY - m_mouseContact.y;

				// save the last position
				m_mouseContact = ImVec2(ptX, ptY);

				// rotate the camera according to the delta
				if (!fp) {
					ed::ArcBallCamera* cam = ((ed::ArcBallCamera*)SystemVariableManager::Instance().GetCamera());
					cam->Yaw(dX);
					cam->Pitch(dY);
				} else {
					ed::FirstPersonCamera* cam = ((ed::FirstPersonCamera*)SystemVariableManager::Instance().GetCamera());
					cam->Yaw(dX * 0.05f);
					cam->Pitch(dY * 0.05f);
				}
			}
			
			// handle left mouse dragging - moving objects if selected
			else if (((ImGui::IsMouseDown(0) && !settings.Preview.SwitchLeftRightClick) ||
				(ImGui::IsMouseDown(1) && settings.Preview.SwitchLeftRightClick)) &&
				settings.Preview.Gizmo)
			{
				// screen space position
				glm::vec2 s = SystemVariableManager::Instance().GetMousePosition();
				s.x *= imageSize.x;
				s.y *= imageSize.y;

				m_gizmo.Move(s.x, s.y, ImGui::GetIO().KeyShift);
			}

			// WASD key press - first person camera
			if (fp) {
				ed::FirstPersonCamera* cam = ((ed::FirstPersonCamera*)SystemVariableManager::Instance().GetCamera());
				cam->MoveUpDown((ImGui::IsKeyDown(SDL_SCANCODE_S) - ImGui::IsKeyDown(SDL_SCANCODE_W)) / 70.0f);
				cam->MoveLeftRight((ImGui::IsKeyDown(SDL_SCANCODE_D) - ImGui::IsKeyDown(SDL_SCANCODE_A)) / 70.0f);
			}
		}

		// mouse wrapping
		if (m_startWrap) {
			int ptX, ptY;
			SDL_GetMouseState(&ptX, &ptY);

			// screen space position
			glm::vec2 s = SystemVariableManager::Instance().GetMousePosition();

			bool wrappedMouse = false;
			const float mPercent = 0.00f;
			if (s.x < mPercent) {
				ptX += imageSize.x * (1 - mPercent);
				wrappedMouse = true;
			}
			else if (s.x > 1 - mPercent) {
				ptX -= imageSize.x * (1 - mPercent);
				wrappedMouse = true;
			}
			else if (s.y > 1 - mPercent) {
				ptY += imageSize.y * (1 - mPercent);
				wrappedMouse = true;
			}
			else if (s.y < mPercent) {
				ptY -= imageSize.y * (1 - mPercent);
				wrappedMouse = true;
			}

			if (wrappedMouse) {
				m_mouseContact = ImVec2(ptX, ptY);
				SDL_WarpMouseInWindow(m_ui->GetSDLWindow(), ptX, ptY);
			}
		}

		// status bar
		if (statusbar)
			m_renderStatusbar();
	}
	void PreviewUI::Duplicate()
	{
		if (m_picks.size() == 0)
			return;

		// store pointers to these objects as we will select them after duplication
		std::vector<PipelineItem*> duplicated;

		// duplicate each item
		for (int i = 0; i < m_picks.size(); i++) {
			ed::PipelineItem* item = m_picks[i];

			// first find a name that is not used
			std::string name = std::string(item->Name);
		
			// remove numbers at the end of the string
			size_t lastOfLetter = std::string::npos;
			for (size_t j = name.size()-1; j > 0; j--)
				if (!std::isdigit(name[j])) {
					lastOfLetter = j + 1;
					break;
				}
			if (lastOfLetter != std::string::npos)
				name = name.substr(0, lastOfLetter);

			// add number to the string and check if it already exists
			for (size_t j = 2; /*WE WILL BRAKE FROM INSIDE ONCE WE FIND THE NAME*/;j++) {
				std::string newName = name + std::to_string(j);
				bool has = m_data->Pipeline.Has(newName.c_str());

				if (!has) {
					name = newName;
					break;
				}
			}

			// get item owner
			char* owner = m_data->Pipeline.GetItemOwner(item->Name);

			// once we found a name, duplicate the properties:
			// duplicate geometry object:
			if (item->Type == PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* data = new pipe::GeometryItem();
				pipe::GeometryItem* origData = (pipe::GeometryItem*)item->Data;

				data->Position = origData->Position;
				data->Rotation = origData->Rotation;
				data->Scale = origData->Scale;
				data->Size = origData->Size;
				data->Topology = origData->Topology;
				data->Type = origData->Type;

				if (data->Type == pipe::GeometryItem::GeometryType::Cube)
					data->VAO = eng::GeometryFactory::CreateCube(data->VBO, data->Size.x, data->Size.y, data->Size.z);
				else if (data->Type == pipe::GeometryItem::Circle) {
					data->VAO = eng::GeometryFactory::CreateCircle(data->VBO, data->Size.x, data->Size.y);
					data->Topology = GL_TRIANGLE_STRIP;
				}
				else if (data->Type == pipe::GeometryItem::Plane)
					data->VAO = eng::GeometryFactory::CreatePlane(data->VBO, data->Size.x, data->Size.y);
				else if (data->Type == pipe::GeometryItem::Rectangle)
					data->VAO = eng::GeometryFactory::CreatePlane(data->VBO, 1, 1);
				else if (data->Type == pipe::GeometryItem::Sphere)
					data->VAO = eng::GeometryFactory::CreateSphere(data->VBO, data->Size.x);
				else if (data->Type == pipe::GeometryItem::Triangle)
					data->VAO = eng::GeometryFactory::CreateTriangle(data->VBO,  data->Size.x);

				m_data->Pipeline.AddItem(owner, name.c_str(), item->Type, data);
			}

			// duplicate Model:
			else if (item->Type == PipelineItem::ItemType::Model) {
				pipe::Model* data = new pipe::Model();
				pipe::Model* origData = (pipe::Model*)item->Data;

				strcpy(data->Filename, origData->Filename);
				strcpy(data->GroupName, origData->GroupName);
				data->OnlyGroup = origData->OnlyGroup;
				data->Scale = origData->Scale;
				data->Position = origData->Position;
				data->Rotation = origData->Rotation;


				if (strlen(data->Filename) > 0) {
					std::string objMem = m_data->Parser.LoadProjectFile(data->Filename);
					eng::Model* mdl = m_data->Parser.LoadModel(data->Filename);

					bool loaded = mdl != nullptr;
					if (loaded)
						data->Data = mdl;
					else m_data->Messages.Add(ed::MessageStack::Type::Error, owner, "Failed to create .obj model " + std::string(item->Name));
				}

				m_data->Pipeline.AddItem(owner, name.c_str(), item->Type, data);
			}


			duplicated.push_back(m_data->Pipeline.Get(name.c_str()));
		}

		// select the newly created items
		m_picks = duplicated;

		// open in properties if needed
		if (Settings::Instance().Preview.PropertyPick && m_picks.size() > 0)
				((PropertyUI*)m_ui->Get(ViewID::Properties))->Open(m_picks[m_picks.size()-1]);
	}

	void PreviewUI::m_renderStatusbar()
	{
		float FPS = 1.0f / m_fpsDelta;
		ImGui::Separator();
		ImGui::Text("FPS: %.2f", FPS);
		ImGui::SameLine();
		ImGui::Text("|");
		ImGui::SameLine();

		if (m_pickMode == 0) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		if (ImGui::Button("P##pickModePos", ImVec2(BUTTON_SIZE, BUTTON_SIZE)) && m_pickMode != 0) {
			m_pickMode = 0;
			m_gizmo.SetMode(m_pickMode);
		}
		else if (m_pickMode == 0) ImGui::PopStyleColor();
		ImGui::SameLine();

		if (m_pickMode == 1) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		if (ImGui::Button("S##pickModeScl", ImVec2(BUTTON_SIZE, BUTTON_SIZE)) && m_pickMode != 1) {
			m_pickMode = 1;
			m_gizmo.SetMode(m_pickMode);
		}
		else if (m_pickMode == 1) ImGui::PopStyleColor();
		ImGui::SameLine();

		if (m_pickMode == 2) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		if (ImGui::Button("R##pickModeRot", ImVec2(BUTTON_SIZE, BUTTON_SIZE)) && m_pickMode != 2) {
			m_pickMode = 2;
			m_gizmo.SetMode(m_pickMode);
		}
		else if (m_pickMode == 2) ImGui::PopStyleColor();
		ImGui::SameLine();

		if (m_picks.size() != 0) {
			ImGui::Text("|");
			ImGui::SameLine();
			ImGui::Text("Picked: ");
			
			for (int i = 0; i < m_picks.size(); i++) {
				ImGui::SameLine();

				if (i != m_picks.size() - 1)
					ImGui::Text("%s,", m_picks[i]->Name);
				else
					ImGui::Text("%s", m_picks[i]->Name);
			}
		}
	
	}

	void PreviewUI::m_setupBoundingBox()
	{
		Logger::Get().Log("Setting up bounding box...");

		GLint success = 0;
		char infoLog[512];

		// create vertex shader
		unsigned int boxVS = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(boxVS, 1, &BOX_VS_CODE, nullptr);
		glCompileShader(boxVS);
		glGetShaderiv(boxVS, GL_COMPILE_STATUS, &success);
		if(!success) {
			glGetShaderInfoLog(boxVS, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to compile a bounding box vertex shader", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create pixel shader
		unsigned int boxPS = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(boxPS, 1, &BOX_PS_CODE, nullptr);
		glCompileShader(boxPS);
		glGetShaderiv(boxPS, GL_COMPILE_STATUS, &success);
		if(!success) {
			glGetShaderInfoLog(boxPS, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to compile a bounding box pixel shader", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		// create a shader program for gizmo
		m_boxShader = glCreateProgram();
		glAttachShader(m_boxShader, boxVS);
		glAttachShader(m_boxShader, boxPS);
		glLinkProgram(m_boxShader);
		glGetProgramiv(m_boxShader, GL_LINK_STATUS, &success);
		if(!success) {
			glGetProgramInfoLog(m_boxShader, 512, NULL, infoLog);
			ed::Logger::Get().Log("Failed to create a bounding box shader program", true);
			ed::Logger::Get().Log(infoLog, true);
		}

		glDeleteShader(boxVS);
		glDeleteShader(boxPS);

		m_uMatWVPLoc = glGetUniformLocation(m_boxShader, "uMatWVP");
		m_uColorLoc = glGetUniformLocation(m_boxShader, "uColor");
	}
	void PreviewUI::m_buildBoundingBox()
	{
		glm::vec3 minPos(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()),
			maxPos(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());

		// find min and max pos
		for (int i = 0; i < m_picks.size(); i++) {
			ed::PipelineItem* item = m_picks[i];

			bool rotatePoints = true;
			glm::vec3 minPosItem(0,0,0), maxPosItem(0,0,0);
			glm::vec3 rota(0,0,0);
			glm::vec3 pos(0, 0, 0);

			if (item->Type == ed::PipelineItem::ItemType::Geometry) {
				pipe::GeometryItem* data = (pipe::GeometryItem*)item->Data;
				glm::vec3 size(data->Size.x * data->Scale.x, data->Size.y * data->Scale.y, data->Size.z * data->Scale.z);
				
				if (data->Type == pipe::GeometryItem::Sphere) {
					size = glm::vec3(size.x * 2, size.x * 2, size.x * 2);
					rotatePoints = false;
				}
				else if (data->Type == pipe::GeometryItem::Circle)
					size = glm::vec3(size.x * 2, size.y * 2, 0.0001f);
				else if (data->Type == pipe::GeometryItem::Triangle) {
					float rightOffs = data->Size.x / tan(glm::radians(30.0f));
					size = glm::vec3(rightOffs * 2 * data->Scale.x, data->Size.x * 2 * data->Scale.y, 0.0001f);
				}
				else if (data->Type == pipe::GeometryItem::Plane)
					size.z = 0.0001f;

				minPosItem = glm::vec3(- size.x / 2, - size.y / 2, - size.z / 2);
				maxPosItem = glm::vec3(+ size.x / 2, + size.y / 2, + size.z / 2);

				rota = data->Rotation;
				pos = data->Position;
			}
			else if (item->Type == ed::PipelineItem::ItemType::Model) {
				pipe::Model* model = (pipe::Model*)item->Data;

				minPosItem = model->Data->GetMinBound() * model->Scale; // TODO: add positions so that it works for multiple objects
				maxPosItem = model->Data->GetMaxBound() * model->Scale;

				rota = model->Rotation;
				pos = model->Position;
			}

			// 8 points
			float pointsX[8] = { minPosItem.x, minPosItem.x, minPosItem.x, minPosItem.x, maxPosItem.x, maxPosItem.x, maxPosItem.x, maxPosItem.x };
			float pointsY[8] = { minPosItem.y, minPosItem.y, maxPosItem.y, maxPosItem.y, minPosItem.y, minPosItem.y, maxPosItem.y, maxPosItem.y };
			float pointsZ[8] = { minPosItem.z, maxPosItem.z, minPosItem.z, maxPosItem.z, minPosItem.z, maxPosItem.z, minPosItem.z, maxPosItem.z };
			
			// apply rotation and translation to those 8 points and check for min and max pos
			for (int j = 0; j < 8; j++) {
				glm::vec4 point(pointsX[j], pointsY[j], pointsZ[j], 1);

				if (rotatePoints)
					point = glm::yawPitchRoll(rota.y, rota.x, rota.z) * point;

				point = glm::translate(glm::mat4(1), pos) * point;

				minPos.x = std::min<float>(point.x, minPos.x);
				minPos.y = std::min<float>(point.y, minPos.y);
				minPos.z = std::min<float>(point.z, minPos.z);
				maxPos.x = std::max<float>(point.x, maxPos.x);
				maxPos.y = std::max<float>(point.y, maxPos.y);
				maxPos.z = std::max<float>(point.z, maxPos.z);
			}
		}

		minPos.x -= BOUNDING_BOX_PADDING; minPos.y -= BOUNDING_BOX_PADDING; minPos.z -= BOUNDING_BOX_PADDING;
		maxPos.x += BOUNDING_BOX_PADDING; maxPos.y += BOUNDING_BOX_PADDING; maxPos.z += BOUNDING_BOX_PADDING;

		// lines
		std::vector<glm::vec3> verts = {
			// back face
			{ glm::vec3(minPos.x, minPos.y, minPos.z) },
			{ glm::vec3(maxPos.x, minPos.y, minPos.z) },
			{ glm::vec3(maxPos.x, minPos.y, minPos.z) },
			{ glm::vec3(maxPos.x, maxPos.y, minPos.z) },
			{ glm::vec3(minPos.x, minPos.y, minPos.z) },
			{ glm::vec3(minPos.x, maxPos.y, minPos.z) },
			{ glm::vec3(minPos.x, maxPos.y, minPos.z) },
			{ glm::vec3(maxPos.x, maxPos.y, minPos.z) },

			// front face
			{ glm::vec3(minPos.x, minPos.y, maxPos.z) },
			{ glm::vec3(maxPos.x, minPos.y, maxPos.z) },
			{ glm::vec3(maxPos.x, minPos.y, maxPos.z) },
			{ glm::vec3(maxPos.x, maxPos.y, maxPos.z) },
			{ glm::vec3(minPos.x, minPos.y, maxPos.z) },
			{ glm::vec3(minPos.x, maxPos.y, maxPos.z) },
			{ glm::vec3(minPos.x, maxPos.y, maxPos.z) },
			{ glm::vec3(maxPos.x, maxPos.y, maxPos.z) },

			// sides
			{ glm::vec3(minPos.x, minPos.y, minPos.z) },
			{ glm::vec3(minPos.x, minPos.y, maxPos.z) },
			{ glm::vec3(maxPos.x, minPos.y, minPos.z) },
			{ glm::vec3(maxPos.x, minPos.y, maxPos.z) },
			{ glm::vec3(minPos.x, maxPos.y, minPos.z) },
			{ glm::vec3(minPos.x, maxPos.y, maxPos.z) },
			{ glm::vec3(maxPos.x, maxPos.y, minPos.z) },
			{ glm::vec3(maxPos.x, maxPos.y, maxPos.z) },
		};

		if (m_boxVBO != 0)
			glDeleteBuffers(1, &m_boxVBO);
		if (m_boxVAO != 0)
			glDeleteVertexArrays(1, &m_boxVAO);

		// create vbo
		glGenBuffers(1, &m_boxVBO);
		glBindBuffer(GL_ARRAY_BUFFER, m_boxVBO);
		glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(glm::vec3), verts.data(), GL_STATIC_DRAW);
		
		// create vao
		glGenVertexArrays(1, &m_boxVAO);
		glBindVertexArray(m_boxVAO);
		
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
		glEnableVertexAttribArray(0);

		// unbind
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);


		// calculate color for the bounding box
		glm::vec4 clearClr = Settings::Instance().Project.ClearColor;
		float avgClear = ((float)clearClr.r + clearClr.g + clearClr.b) / 3.0f;
		ImVec4 wndBg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
		float avgWndBg = (wndBg.x + wndBg.y + wndBg.z) / 3;
		float clearAlpha = clearClr.a / 255.0f;
		float color = avgClear * clearAlpha + avgWndBg * (1 - clearAlpha);
		if (color >= 0.5f)
			m_boxColor = glm::vec4(0, 0, 0, 1);
		else
			m_boxColor = glm::vec4(1, 1, 1, 1);
	}
	void PreviewUI::m_renderBoundingBox()
	{
		glUseProgram(m_boxShader);

		glm::mat4 matWorld(1);
		glm::mat4 matProj = SystemVariableManager::Instance().GetProjectionMatrix() * SystemVariableManager::Instance().GetViewMatrix();

		glUniformMatrix4fv(m_uMatWVPLoc, 1, GL_FALSE, glm::value_ptr(matProj * matWorld));
		glUniform4fv(m_uColorLoc, 1, glm::value_ptr(m_boxColor));

		glBindVertexArray(m_boxVAO);
		glDrawArrays(GL_LINES, 0, 24);
	}
}