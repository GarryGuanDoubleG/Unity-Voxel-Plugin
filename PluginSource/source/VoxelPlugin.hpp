#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <assert.h>
#include <math.h>
#include <unordered_map>
#include <stddef.h>
#include "GLEW/glew.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "glm/glm.hpp"
#include "glm/gtx/hash.hpp"
#include "glm/gtc/matrix_transform.hpp" // glm::translate, glm::rotate, glm::scale, glm::perspective
#include "glm/gtc/constants.hpp" // glm::pi
#include "glm/gtc/type_ptr.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm\gtx\euler_angles.hpp"
#include "omp.h"

#include "FastNoiseSIMD/FastNoiseSIMD.h"
#include "FastNoiseSIMD/FastNoise.h"

using namespace std;
#include "tables.hpp"
#include "SVD.h"
#include "QEFSolver.h"
#include "VoxelVertex.hpp"
#include "density.hpp"
#include "octree.hpp"
#include "chunk.hpp"
#include "voxelManager.hpp"

#include "Unity/IUnityInterface.h"
#include "Unity/IUnityGraphics.h"
#include "RenderAPI.h"
#include "PlatformBase.h"


//voxel events
#define CHUNK_GEN_FINISHED  1

extern void error(GLenum e);
extern "C" void LogToUnity(const string &msg);
extern "C" void ThrowEventToUnity(uint8_t eventVal);