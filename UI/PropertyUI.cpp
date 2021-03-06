#include "PropertyUI.h"
#include "UIHelper.h"
#include "CodeEditorUI.h"
#include "../Objects/Logger.h"
#include "../Objects/Names.h"
#include "../Objects/HLSL2GLSL.h"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#define BUTTON_SPACE_LEFT -40 * Settings::Instance().DPIScale
#define HARRAYSIZE(a) (sizeof(a)/sizeof(*a))

namespace ed
{
	void PropertyUI::OnEvent(const SDL_Event& e)
	{}
	void PropertyUI::Update(float delta)
	{
		if (m_current != nullptr || m_currentRT != nullptr) {
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
			if (m_current != nullptr)
				ImGui::Text(m_current->Name);
			else if (m_currentRT != nullptr)
				ImGui::Text(m_currentRT->Name.c_str());
			else
				ImGui::Text("nullptr");
			ImGui::PopStyleColor();

			ImGui::Columns(2, "##content_columns");
			ImGui::SetColumnWidth(0, ImGui::GetWindowSize().x * 0.3f);

			ImGui::Separator();
			
			if (m_currentRT == nullptr) {
				ImGui::Text("Name:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-40);
				ImGui::InputText("##pui_itemname", m_itemName, PIPELINE_ITEM_NAME_LENGTH);
				ImGui::PopItemWidth();
				ImGui::SameLine();
				if (ImGui::Button("Ok##pui_itemname", ImVec2(-1, 0))) {
					if (m_data->Pipeline.Has(m_itemName))
						ImGui::OpenPopup("ERROR##pui_itemname_taken");
					else if (strlen(m_itemName) <= 2)
						ImGui::OpenPopup("ERROR##pui_itemname_short");
					else if (m_current != nullptr) {
						m_data->Messages.RenameGroup(m_current->Name, m_itemName);
						((CodeEditorUI*)m_ui->Get(ViewID::Code))->RenameShaderPass(m_current->Name, m_itemName);
						memcpy(m_current->Name, m_itemName, PIPELINE_ITEM_NAME_LENGTH);
					}
				}
				ImGui::NextColumn();
				ImGui::Separator();
			}

			ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5f, 0.5f));
			if (ImGui::BeginPopupModal("ERROR##pui_itemname_taken", 0, ImGuiWindowFlags_NoResize)) {
				ImGui::Text("There is already an item with name \"%s\"", m_itemName);
				if (ImGui::Button("Ok")) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
			if (ImGui::BeginPopupModal("ERROR##pui_itemname_short", 0, ImGuiWindowFlags_NoResize)) {
				ImGui::Text("The name must be at least 3 characters long.");
				if (ImGui::Button("Ok")) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
			ImGui::PopStyleVar();

			if (m_current != nullptr) {
				if (m_current->Type == ed::PipelineItem::ItemType::ShaderPass) {
					ed::pipe::ShaderPass* item = reinterpret_cast<ed::pipe::ShaderPass*>(m_current->Data);

					/* Render Texture */
					ImGui::Text("RT:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					std::vector<std::string> rts = m_data->Objects.GetObjects();
					bool windowAlreadyBound = false;
					for (int i = 0; i < MAX_RENDER_TEXTURES; i++) {
						if (item->RenderTextures[i] == 0)
							continue;
						
						if (!windowAlreadyBound && item->RenderTextures[i] == m_data->Renderer.GetTexture()) {
							windowAlreadyBound = true;
							continue;
						}

						for (int j = 0; j < rts.size(); j++) {
							if (!m_data->Objects.IsRenderTexture(rts[j]) || (m_data->Objects.IsRenderTexture(rts[j]) && item->RenderTextures[i] == m_data->Objects.GetTexture(rts[j]))) {
								rts.erase(rts.begin() + j);
								j--;
							}
						}
					}
					for (item->RTCount = 0; item->RTCount < MAX_RENDER_TEXTURES; item->RTCount++) {
						int i = item->RTCount;
						GLuint rtID = item->RenderTextures[i];
						std::string name = rtID == 0 ? "NULL" : (rtID == m_data->Renderer.GetTexture() ? "Window" : m_data->Objects.GetRenderTexture(rtID)->Name);
						
						if (ImGui::BeginCombo(("##pui_rt_combo" + std::to_string(i)).c_str(), name.c_str())) {
							// null element
							if (i != 0 && rtID != 0) {
								if (ImGui::Selectable("NULL", rtID == 0)) {
									item->RenderTextures[i] = 0;
									for (int j = i + 1; j < MAX_RENDER_TEXTURES; j++)
										item->RenderTextures[j] = 0;
								}
							}

							// window element
							if (!windowAlreadyBound)
								if (ImGui::Selectable("Window", rtID == m_data->Renderer.GetTexture()))
									item->RenderTextures[i] = m_data->Renderer.GetTexture();

							// users RTs
							for (int j = 0; j < rts.size(); j++)
								if (m_data->Objects.IsRenderTexture(rts[j])) {
									GLuint texID = m_data->Objects.GetTexture(rts[j]);
									if (ImGui::Selectable(rts[j].c_str(), rtID == texID))
										item->RenderTextures[i] = texID;
								}

							ImGui::EndCombo();
						}

						if (item->RenderTextures[i] == 0)
							break;
					}
					ImGui::PopItemWidth();
					ImGui::NextColumn();

					ImGui::Separator();

					/* vertex shader path */
					ImGui::Text("VS Path:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::InputText("##pui_vspath", item->VSPath, MAX_PATH);
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_vsbtn", ImVec2(-1, 0))) {
						std::string file;
						bool success = UIHelper::GetOpenFileDialog(file);
						if (success) {
							file = m_data->Parser.GetRelativePath(file);
							strcpy(item->VSPath, file.c_str());

							if (m_data->Parser.FileExists(file))
								m_data->Messages.ClearGroup(m_current->Name);
							else
								m_data->Messages.Add(ed::MessageStack::Type::Error, m_current->Name, "Vertex shader file doesnt exist");
						}
					}
					ImGui::NextColumn();

					ImGui::Separator();

					/* vertex shader entry */
					ImGui::Text("VS Entry:");
					ImGui::NextColumn();

					if (HLSL2GLSL::IsHLSL(item->VSPath)) {
						ImGui::PushItemWidth(-1);
						ImGui::InputText("##pui_vsentry", item->VSEntry, 32);
						ImGui::PopItemWidth();
					} else
						ImGui::Text("main");
					ImGui::NextColumn();

					ImGui::Separator();

					/* pixel shader path */
					ImGui::Text("PS Path:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::InputText("##pui_pspath", item->PSPath, MAX_PATH);
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_psbtn", ImVec2(-1, 0))) {
						std::string file;
						bool success = UIHelper::GetOpenFileDialog(file);
						if (success) {
							file = m_data->Parser.GetRelativePath(file);
							strcpy(item->PSPath, file.c_str());

							if (m_data->Parser.FileExists(file))
								m_data->Messages.ClearGroup(m_current->Name);
							else
								m_data->Messages.Add(ed::MessageStack::Type::Error, m_current->Name, "Pixel shader file doesnt exist");
						}
					}
					ImGui::NextColumn();

					ImGui::Separator();

					/* pixel shader entry */
					ImGui::Text("PS Entry:");
					ImGui::NextColumn();

					if (HLSL2GLSL::IsHLSL(item->PSPath)) {
						ImGui::PushItemWidth(-1);
						ImGui::InputText("##pui_psentry", item->PSEntry, 32);
						ImGui::PopItemWidth();
					} else
						ImGui::Text("main");
					ImGui::NextColumn();

					ImGui::Separator();

					// gs used
					ImGui::Text("GS:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::Checkbox("##pui_gsuse", &item->GSUsed);
					ImGui::NextColumn();
					ImGui::Separator();

					if (!item->GSUsed) ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);

					// gs path
					ImGui::Text("GS path:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(BUTTON_SPACE_LEFT);
					ImGui::InputText("##pui_gspath", item->GSPath, MAX_PATH);
					ImGui::PopItemWidth();
					ImGui::SameLine();
					if (ImGui::Button("...##pui_gsbtn", ImVec2(-1, 0))) {
						std::string file;
						bool success = UIHelper::GetOpenFileDialog(file);
						if (success) {
							file = m_data->Parser.GetRelativePath(file);
							strcpy(item->GSPath, file.c_str());

							if (m_data->Parser.FileExists(file))
								m_data->Messages.ClearGroup(m_current->Name);
							else
								m_data->Messages.Add(ed::MessageStack::Type::Error, m_current->Name, "Geometry shader file doesnt exist");
						}
					}
					ImGui::NextColumn();
					ImGui::Separator();

					// gs entry
					ImGui::Text("GS entry:");
					ImGui::NextColumn();
					if (HLSL2GLSL::IsHLSL(item->GSPath)) {
						ImGui::PushItemWidth(-1);
						ImGui::InputText("##pui_gsentry", item->GSEntry, 32);
						ImGui::PopItemWidth();
					} else
						ImGui::Text("main");
					ImGui::NextColumn();

					if (!item->GSUsed) ImGui::PopItemFlag();
				}
				else if (m_current->Type == ed::PipelineItem::ItemType::Geometry) {
					ed::pipe::GeometryItem* item = reinterpret_cast<ed::pipe::GeometryItem*>(m_current->Data);

					/* position */
					ImGui::Text("Position:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					ImGui::DragFloat3("##pui_geopos", glm::value_ptr(item->Position), 0.01f);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* scale */
					ImGui::Text("Scale:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					ImGui::DragFloat3("##pui_geoscale", glm::value_ptr(item->Scale), 0.01f);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* rotation */
					ImGui::Text("Rotation:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					glm::vec3 rotaDeg(glm::degrees(item->Rotation.x), glm::degrees(item->Rotation.y), glm::degrees(item->Rotation.z));
					ImGui::DragFloat3("##pui_georota", glm::value_ptr(rotaDeg), 0.01f);
					if (rotaDeg.x > 360)
						rotaDeg.x = 0;
					if (rotaDeg.x < 0)
						rotaDeg.x = 360;
					if (rotaDeg.y > 360)
						rotaDeg.y = 0;
					if (rotaDeg.y < 0)
						rotaDeg.y = 360;
					if (rotaDeg.z > 360)
						rotaDeg.z = 0;
					if (rotaDeg.z < 0)
						rotaDeg.z = 360;
					item->Rotation = glm::vec3(glm::radians(rotaDeg.x), glm::radians(rotaDeg.y), glm::radians(rotaDeg.z));
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();
					
					/* topology type */
					ImGui::Text("Topology:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					ImGui::Combo("##pui_geotopology", reinterpret_cast<int*>(&item->Topology), TOPOLOGY_ITEM_NAMES, HARRAYSIZE(TOPOLOGY_ITEM_NAMES));
					ImGui::PopItemWidth();
				}
				else if (m_current->Type == PipelineItem::ItemType::RenderState) {
					pipe::RenderState* data = (pipe::RenderState*)m_current->Data;

					// enable/disable wireframe rendering
					bool isWireframe = data->PolygonMode == GL_LINE;
					ImGui::Text("Wireframe:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::Checkbox("##cui_wireframe", (bool*)(&isWireframe));
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();
					data->PolygonMode = isWireframe ? GL_LINE : GL_FILL;

					// cull
					ImGui::Text("Cull:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::Checkbox("##cui_cullm", &data->CullFace);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// cull mode
					ImGui::Text("Cull mode:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateCullModeCombo("##cui_culltype", data->CullFaceType);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// front face == counter clockwise order
					bool isCCW = data->FrontFace == GL_CCW;
					ImGui::Text("Counter clockwise:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::Checkbox("##cui_ccw", &isCCW);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();
					data->FrontFace = isCCW ? GL_CCW : GL_CW;




					// depth enable
					ImGui::Text("Depth test:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::Checkbox("##cui_depth", &data->DepthTest);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (!data->DepthTest)
					{
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
					}

					// depth clip
					ImGui::Text("Depth clip:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::Checkbox("##cui_depthclip", &data->DepthClamp);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// depth mask
					ImGui::Text("Depth mask:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::Checkbox("##cui_depthmask", &data->DepthMask);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// depth function
					ImGui::Text("Depth function:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateComparisonFunctionCombo("##cui_depthop", data->DepthFunction);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// depth bias
					ImGui::Text("Depth bias:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::DragFloat("##cui_depthbias", &data->DepthBias);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (!data->DepthTest)
					{
						ImGui::PopItemFlag();
						ImGui::PopStyleVar();
					}




					// blending
					ImGui::Text("Blending:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::Checkbox("##cui_blend", &data->Blend);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (!data->Blend)
					{
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
					}

					// alpha to coverage
					ImGui::Text("Alpha to coverage:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::Checkbox("##cui_alphacov", &data->AlphaToCoverage);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// source blend factor
					ImGui::Text("Source blend factor:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateBlendCombo("##cui_srcblend", data->BlendSourceFactorRGB);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// operator
					ImGui::Text("Blend operator:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateBlendOperatorCombo("##cui_blendop", data->BlendFunctionColor);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// destination blend factor
					ImGui::Text("Destination blend factor:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateBlendCombo("##cui_destblend", data->BlendDestinationFactorRGB);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// source alpha blend factor
					ImGui::Text("Source alpha blend factor:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateBlendCombo("##cui_srcalphablend", data->BlendSourceFactorAlpha);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// operator
					ImGui::Text("Alpha blend operator:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateBlendOperatorCombo("##cui_blendopalpha", data->BlendFunctionAlpha);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// destination alpha blend factor
					ImGui::Text("Destination alpha blend factor:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateBlendCombo("##cui_destalphablend", data->BlendDestinationFactorAlpha);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// blend factor
					ImGui::Text("Custom blend factor:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::ColorEdit4("##cui_blendfactor", glm::value_ptr(data->BlendFactor));
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (!data->Blend)
					{
						ImGui::PopItemFlag();
						ImGui::PopStyleVar();
					}




					// stencil enable
					ImGui::Text("Stencil test:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::Checkbox("##cui_stencil", &data->StencilTest);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (!data->StencilTest)
					{
						ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
						ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
					}

					// stencil mask
					ImGui::Text("Stencil mask:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::InputInt("##cui_stencilmask", (int*)& data->StencilMask);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (data->StencilMask < 0)
						data->StencilMask = 0;
					if (data->StencilMask > 255)
						data->StencilMask = 255;

					// stencil reference
					ImGui::Text("Stencil reference:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					ImGui::InputInt("##cui_sref", (int*)& data->StencilReference); // TODO: imgui uint input??
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					if (data->StencilReference < 0)
						data->StencilReference = 0;
					if (data->StencilReference > 255)
						data->StencilReference = 255;

					// front face function
					ImGui::Text("Stencil front face function:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateComparisonFunctionCombo("##cui_ffunc", data->StencilFrontFaceFunction);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// front face pass operation
					ImGui::Text("Stencil front face pass:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateStencilOperationCombo("##cui_fpass", data->StencilFrontFaceOpPass);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// front face stencil fail operation
					ImGui::Text("Stencil front face fail:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateStencilOperationCombo("##cui_ffail", data->StencilFrontFaceOpStencilFail);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// front face depth fail operation
					ImGui::Text("Depth front face fail:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateStencilOperationCombo("##cui_fdfail", data->StencilFrontFaceOpDepthFail);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// back face function
					ImGui::Text("Stencil back face function:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateComparisonFunctionCombo("##cui_bfunc", data->StencilBackFaceFunction);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// back face pass operation
					ImGui::Text("Stencil back face pass:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateStencilOperationCombo("##cui_bpass", data->StencilBackFaceOpPass);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// back face stencil fail operation
					ImGui::Text("Stencil back face fail:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateStencilOperationCombo("##cui_bfail", data->StencilBackFaceOpStencilFail);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					// back face depth fail operation
					ImGui::Text("Depth back face fail:");
					ImGui::NextColumn();
					ImGui::PushItemWidth(-1);
					UIHelper::CreateStencilOperationCombo("##cui_bdfail", data->StencilBackFaceOpDepthFail);
					ImGui::PopItemWidth();
					ImGui::NextColumn();

					if (!data->StencilTest)
					{
						ImGui::PopItemFlag();
						ImGui::PopStyleVar();
					}
				}
				else if (m_current->Type == ed::PipelineItem::ItemType::Model) {
					ed::pipe::Model* item = reinterpret_cast<ed::pipe::Model*>(m_current->Data);

					/* position */
					ImGui::Text("Position:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					ImGui::DragFloat3("##pui_geopos", glm::value_ptr(item->Position), 0.01f);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* scale */
					ImGui::Text("Scale:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					ImGui::DragFloat3("##pui_geoscale", glm::value_ptr(item->Scale), 0.01f);
					ImGui::PopItemWidth();
					ImGui::NextColumn();
					ImGui::Separator();

					/* rotation */
					ImGui::Text("Rotation:");
					ImGui::NextColumn();

					ImGui::PushItemWidth(-1);
					glm::vec3 rotaDeg(glm::degrees(item->Rotation.x), glm::degrees(item->Rotation.y), glm::degrees(item->Rotation.z));
					ImGui::DragFloat3("##pui_georota", glm::value_ptr(rotaDeg), 0.01f);
					item->Rotation = glm::vec3(glm::radians(rotaDeg.x), glm::radians(rotaDeg.y), glm::radians(rotaDeg.z));
					ImGui::PopItemWidth();
					ImGui::NextColumn();
				}
			} else if (m_currentRT != nullptr) {
				/* FIXED SIZE */
				ImGui::Text("Fixed size:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				if (ImGui::DragInt2("##prop_rt_fsize", glm::value_ptr(m_currentRT->FixedSize), 1)) {
					m_currentRT->RatioSize = glm::vec2(-1, -1);
					if (m_currentRT->FixedSize.x <= 0)
						m_currentRT->FixedSize.x = 10;
					if (m_currentRT->FixedSize.y <= 0)
						m_currentRT->FixedSize.y = 10;

					m_data->Objects.ResizeRenderTexture(std::string(m_itemName), m_currentRT->FixedSize);
				}
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* RATIO SIZE */
				ImGui::Text("Ratio size:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				if (ImGui::DragFloat2("##prop_rt_rsize", glm::value_ptr(m_currentRT->RatioSize), 0.01f)) {
					m_currentRT->FixedSize = glm::ivec2(-1, -1);
					if (m_currentRT->RatioSize.x <= 0)
						m_currentRT->RatioSize.x = 0.01f;
					if (m_currentRT->RatioSize.y <= 0)
						m_currentRT->RatioSize.y = 0.01f;

					glm::ivec2 newSize(m_currentRT->RatioSize.x * m_data->Renderer.GetLastRenderSize().x,
						m_currentRT->RatioSize.y * m_data->Renderer.GetLastRenderSize().y);

					m_data->Objects.ResizeRenderTexture(std::string(m_itemName), newSize);
				}
				ImGui::PopItemWidth();
				ImGui::NextColumn();
				ImGui::Separator();

				/* CLEAR COLOR */
				ImGui::Text("Clear:");
				ImGui::NextColumn();

				ImGui::PushItemWidth(-1);
				ImGui::ColorEdit4("##prop_rt_color", glm::value_ptr(m_currentRT->ClearColor));
				ImGui::PopItemWidth();
			}
			
			ImGui::NextColumn();
			ImGui::Separator();
			ImGui::Columns(1);
		} else {
			ImGui::TextWrapped("Right click on an item -> Properties");
		}
	}
	void PropertyUI::Open(ed::PipelineItem * item)
	{
		if (item != nullptr) memcpy(m_itemName, item->Name, PIPELINE_ITEM_NAME_LENGTH);

		Logger::Get().Log("Openning a pipeline item in the PropertyUI");

		m_currentRT = nullptr;
		m_current = item;
	}
	void PropertyUI::Open(const std::string& name, ed::RenderTextureObject * obj)
	{
		Logger::Get().Log("Openning a render texutre in the PropertyUI");

		memset(m_itemName, 0, PIPELINE_ITEM_NAME_LENGTH);
		memcpy(m_itemName, name.c_str(), name.size());

		m_current = nullptr;
		m_currentRT = obj;
	}
}