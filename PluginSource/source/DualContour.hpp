#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <string.h>
#include <unordered_map>
#include <stddef.h>
#include <GL\glew.h>
#include <SDL.h>
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtc/matrix_transform.hpp> // glm::translate, glm::rotate, glm::scale, glm::perspective
#include <glm/gtc/constants.hpp> // glm::pi
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm\gtx\euler_angles.hpp>
#include <omp.h>

#include <FastNoiseSIMD.h>
#include <FastNoise.h>

using namespace std;
#include "tables.hpp"
#include "SVD.h"
#include "QEFSolver.h"
#include "VoxelVertex.hpp"
#include "density.hpp"
#include "octree.hpp"
#include "chunk.hpp"
#include "voxelManager.hpp"
//
//#include "Unity/IUnityGraphics.h"
//#include "RenderAPI.h"
//#include "PlatformBase.h"
