#include "VoxelPlugin.hpp"

#include "PlatformBase.h"
#include "RenderAPI.h"

//static float g_time;
static VoxelManager *s_VoxelManager = nullptr;

enum RenderEvents
{
	ChunkGenerated = 0,
	BindChunks = 1
};

//log call back
extern "C"
{
	typedef void(*logMSG_t) (const char *msg);
	static logMSG_t logMsg = nullptr;

	void UNITY_INTERFACE_EXPORT SetLogCallBack(logMSG_t logMsgCallback)
	{
		logMsg = logMsgCallback;
	}

	void LogToUnity(const string &msg)
	{
		if(logMsg != nullptr)
			(*logMsg)(msg.c_str());
	}
}

//voxel event callback
extern "C"
{
	typedef void(*VoxelEvent) (uint8_t eventVal);
	static VoxelEvent eventHandler = nullptr;

	void UNITY_INTERFACE_EXPORT SetVoxelEventHandler(VoxelEvent eventcallBack)
	{
		eventHandler = eventcallBack;
	}

	void ThrowEventToUnity(uint8_t eventVal)
	{
		if (eventHandler != nullptr)
			(*eventHandler)(eventVal);
	}
}

void error(GLenum e) 
{
	switch (e)
	{
	case GL_INVALID_ENUM:
		LogToUnity("Error: GL_INVALID_ENUM");
		break;
	case GL_INVALID_VALUE:
		LogToUnity("Error: GL_INVALID_VALUE");
		break;
	case GL_INVALID_OPERATION:
		LogToUnity("Error: GL_INVALID_OPERATION");
		break;
	case GL_INVALID_FRAMEBUFFER_OPERATION:
		LogToUnity("Error: GL_INVALID_FRAMEBUFFER_OPERATION");
		break;
	case GL_OUT_OF_MEMORY:
		LogToUnity("Error: GL_OUT_OF_MEMORY");
		break;
	case GL_NO_ERROR:
		LogToUnity("No error reported");
		break;
	default:
		LogToUnity("Unknown error");
		break;
	}
}


static GLuint * s_vboArr;
static GLuint * s_eboArr;
static glm::vec3 * s_chunkIndicesToBind;
static GLuint s_count;

//
extern "C"
{
	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitializeVoxelPlugin(int voxelSize, int chunkRange, float startRange, int chunkSize, float maxHeight)
	{
		error(glGetError());

		LogToUnity("Creating Voxel Manager");
		s_VoxelManager = new VoxelManager();
		s_VoxelManager->Init(voxelSize, chunkRange, startRange, chunkSize, maxHeight);
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GenerateChunksInRange(int count, glm::vec3 * indices)
	{
		s_VoxelManager->GenerateChunksInRange(count, indices);
	}

	int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetActiveChunkCount()
	{
		return s_VoxelManager->GetChunkCount();
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API  SetChunkBuffers(int count,  glm::vec3 *chunksToBind, void * vboArr, void * eboArr)
	{
		GLuint *VBOs = (GLuint *)(size_t*)vboArr;
		GLuint *EBOs = (GLuint *)(size_t*)eboArr;

		s_chunkIndicesToBind = chunksToBind;
		s_vboArr = VBOs;
		s_eboArr = EBOs;
		s_count = (GLuint)count;
	}
	
	int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetNewChunkCount()
	{
		return s_VoxelManager->GetNewChunkCount();
	}

	//This might not be needed
	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetNewChunkData(int count, int *vertCount, int *triCount)
	{
		s_VoxelManager->GetNewChunkMeshData(count, vertCount, triCount);
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetNewChunkIndices(int count, glm::vec3 * indices)
	{
		s_VoxelManager->GetNewChunkIndices(count, indices);
	}

	//void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetChunkMeshSizes(int *vertCount, int *triCount)
	//{
	//	s_VoxelManager->GetChunkMeshSizes(vertCount, triCount);
	//}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetActiveChunkPositions(int count, glm::vec3 *positions)
	{
		s_VoxelManager->GetActiveChunkPositions(count, positions);
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API OnShutdown()
	{
		if (s_VoxelManager != nullptr)
		{
			LogToUnity("Shutdown");
			delete s_VoxelManager;
			s_VoxelManager = nullptr;
		}
	}

	static void UNITY_INTERFACE_API OnChunkInitEvent(int eventID)
	{
		// Unknown / unsupported graphics device type? Do nothing
		//s_VoxelManager->BindChunks(s_count, s_chunkIndicesToBind, s_vboArr, s_eboArr);
	}

	static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
	{
		switch (eventID)
		{
		case RenderEvents::BindChunks:
			s_VoxelManager->BindChunks(s_count, s_chunkIndicesToBind, s_vboArr, s_eboArr);
			break;
		default:
			break;
		}
	}

	// --------------------------------------------------------------------------
	// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.
	extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
	{
		return OnRenderEvent;
	}
}