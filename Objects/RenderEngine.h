#pragma once
#include "PipelineManager.h"
#include "ProjectParser.h"
#include "MessageStack.h"
#include "../Engine/Timer.h"

#include <unordered_map>
#include <functional>

#include <glm/glm.hpp>
#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/glew.h>
#include <GL/gl.h>

namespace ed
{
	class ObjectManager;

	class RenderEngine
	{
	public:
		RenderEngine(PipelineManager* pipeline, ObjectManager* objects, ProjectParser* project, MessageStack* messages);
		~RenderEngine();

		void Render(int width, int height);
		void Recompile(const char* name);
		void Pick(float sx, float sy, bool multiPick, std::function<void(PipelineItem*)> func = nullptr);

		void FlushCache();
		void AddPickedItem(PipelineItem* pipe, bool multiPick = false);

		inline GLuint GetTexture() { return m_rtColor; }
		glm::ivec2 GetLastRenderSize() { return m_lastSize; }

	public:
		struct ItemVariableValue
		{
			ItemVariableValue(ed::ShaderVariable* var) { 
				Variable = var;
				OldValue = var->Data;
				NewValue = new ShaderVariable(var->GetType(), var->Name, var->System);
				NewValue->Function = var->Function;
				Item = nullptr;
			}
			PipelineItem* Item;
			ed::ShaderVariable* Variable;
			char* OldValue;
			ed::ShaderVariable* NewValue;
		};

		inline std::vector<ItemVariableValue>& GetItemVariableValues() { return m_itemValues; }
		inline void AddItemVariableValue(const ItemVariableValue& item) { m_itemValues.push_back(item); }
		inline void RemoveItemVariableValue(PipelineItem* item, ShaderVariable* var) {
			for (int i = 0; i < m_itemValues.size(); i++)
				if (m_itemValues[i].Item == item && m_itemValues[i].Variable == var) {
					m_itemValues.erase(m_itemValues.begin() + i);
					return;
				}
		}
		inline void RemoveItemVariableValues(PipelineItem* item) {
			for (int i = 0; i < m_itemValues.size(); i++)
				if (m_itemValues[i].Item == item) {
					m_itemValues.erase(m_itemValues.begin() + i);
					i--;
				}
		}

	private:
		PipelineManager* m_pipeline;
		ObjectManager* m_objects;
		ProjectParser* m_project;
		MessageStack* m_msgs;

		/* picking */
		bool m_pickAwaiting;
		float m_pickDist;
		std::function<void(PipelineItem*)> m_pickHandle;
		glm::vec3 m_pickOrigin; // view space
		glm::vec3 m_pickDir;
		std::vector<PipelineItem*> m_pick;
		bool m_wasMultiPick;
		void m_pickItem(PipelineItem* item, bool multiPick);

		glm::ivec2 m_lastSize;
		GLuint m_rtColor, m_rtDepth;
		bool m_fbosNeedUpdate;
		
		std::vector<PipelineItem*> m_items;
		std::vector<GLuint> m_shaders;
		std::map<pipe::ShaderPass*, std::vector<GLuint>> m_fbos;
		std::map<pipe::ShaderPass*, GLuint> m_fboCount;

		void m_updatePassFBO(ed::pipe::ShaderPass* pass);

		std::vector<ItemVariableValue> m_itemValues; // list of all values to apply once we start rendering 

		eng::Timer m_cacheTimer;
		void m_cache();
	};
}