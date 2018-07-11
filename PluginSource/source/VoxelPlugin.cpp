#include "VoxelPlugin.hpp"

#include "PlatformBase.h"
#include "RenderAPI.h"

//static float g_time;
static VoxelManager *s_VoxelManager = nullptr;

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
static GLuint s_count;

//
extern "C"
{	
// 
//	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetTimeFromUnity(float t) { g_time = t; }
//
	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API InitializeVoxelPlugin(int voxelSize, int chunkRange, int chunkSize, float maxHeight)
	{
		error(glGetError());

		LogToUnity("Creating Voxel Manager");
		s_VoxelManager = new VoxelManager();
		s_VoxelManager->Init(voxelSize, chunkRange, chunkSize, maxHeight);				
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GenerateChunksInRange()
	{
		s_VoxelManager->GenerateChunksInRange();
	}

	int UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetActiveChunkCount()
	{
		return s_VoxelManager->GetChunkCount();
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API  BindChunks(int count, void * vboArr, void * eboArr)
	{
		GLuint *VBOs = (GLuint *)(size_t*)vboArr;
		GLuint *EBOs = (GLuint *)(size_t*)eboArr;

		s_vboArr = VBOs;
		s_eboArr = EBOs;
		s_count = (GLuint)count;		
	}
	

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetChunkMeshSizes(int *vertCount, int *triCount)
	{
		s_VoxelManager->GetChunkMeshSizes(vertCount, triCount);
	}

	void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API OnShutdown()
	{
		if (s_VoxelManager != nullptr)
		{
			delete s_VoxelManager;
			s_VoxelManager = nullptr;
		}
	}

	static void UNITY_INTERFACE_API OnChunkInitEvent(int eventID)
	{
		// Unknown / unsupported graphics device type? Do nothing
		s_VoxelManager->BindChunks(s_count, s_vboArr, s_eboArr);
	}


	// --------------------------------------------------------------------------
	// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.

	extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API OnInitVoxelsEvent()
	{
		return OnChunkInitEvent;
	}
}