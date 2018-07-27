#include "VoxelPlugin.hpp"
#include "enkiTS\TaskScheduler.h"
#include <string>

using namespace enki;
static enki::TaskScheduler g_TScheduler;

//generate chunks of volume chunkRange^3
struct GenerateChunkTaskSet : ITaskSet
{
	Chunk *chunks;
	float voxelSize;
	glm::vec3 chunkSize;
	vector<glm::ivec3> chunkIndices;
	int chunkRange;

	GenerateChunkTaskSet(const uint32_t size_, Chunk *chunks, const vector<glm::ivec3> &chunkIndices, const float voxelSize, const glm::vec3 &chunkSize)
	{
		m_SetSize = size_;
		this->chunks = chunks;
		this->voxelSize = voxelSize;
		this->chunkSize = chunkSize;
		this->chunkIndices = chunkIndices;
	}

	void ExecuteRange(TaskSetPartition range, uint32_t threadnum)
	{
		for (int i = range.start; i < range.end; i++)
		{
			Density::DensityType type = chunkIndices[i].y < 0 ? Density::Cave : Density::Terrain;
			//Density::DensityType type = Density::Terrain;
			chunks[i].Init(chunkIndices[i], chunkSize, type, voxelSize);
		}
	}
};

struct GenerateSeamTask : ITaskSet
{
	std::vector<Chunk *> chunks;

	GenerateSeamTask(const uint32_t size_, std::vector<Chunk *> chunks)
	{
		m_SetSize = size_;
		this->chunks = chunks;
	}

	void ExecuteRange(TaskSetPartition range, uint32_t threadnum)
	{
		for (int i = range.start; i < range.end; i++)
			chunks[i]->GenerateSeam();
	}
};

static vector<GenerateChunkTaskSet> g_genChunkTasks;
static vector<GenerateSeamTask> g_genSeamTasks;

static vector<Chunk*> g_newChunks;
static vector<int> g_newChunkVertCount;
static vector<int> g_newChunkTriCount;

VoxelManager::VoxelManager()
{

}

VoxelManager::~VoxelManager()
{
	LogToUnity("Deleting Chunks");
	for (auto chunk : m_chunks)
		delete chunk;

	m_chunkMap.empty();
}

void VoxelManager::Init(int voxelSize, int range, int chunkSize, float maxHeight)
{
	m_voxelSize = voxelSize;
	m_renderRange = range;
	m_chunkSize = chunkSize;
	m_activeChunks = 0;
	int x_range = 2 * m_renderRange + 1;
	int z_range = x_range;
	int y_range = 3;
	m_totalChunks = x_range * y_range * z_range;

	m_chunkMap.reserve(m_totalChunks);
	m_chunks.reserve(m_totalChunks);
	Chunk * chunks = new Chunk[m_totalChunks];
	for (int i = 0; i < m_totalChunks; i++)
		m_chunks[i] = &chunks[i];

	Density::SetVoxelSize(m_voxelSize);
	Density::SetMaxVoxelHeight(maxHeight);
	Density::Initialize();

	g_TScheduler.Initialize();
}

void VoxelManager::AssignChunkNeighbors(Chunk *chunk)
{
	if (!chunk) return;
	const glm::ivec3 chunkIndex = chunk->m_chunkIndex;

	chunk->AssignNeighbor(m_chunkMap[chunkIndex + glm::ivec3(0, 1, 0)], TOP);
	chunk->AssignNeighbor(m_chunkMap[chunkIndex + glm::ivec3(0, -1, 0)], BOTTOM);
	chunk->AssignNeighbor(m_chunkMap[chunkIndex + glm::ivec3(-1, 0, 0)], LEFT);
	chunk->AssignNeighbor(m_chunkMap[chunkIndex + glm::ivec3(1, 0, 0)], RIGHT);
	chunk->AssignNeighbor(m_chunkMap[chunkIndex + glm::ivec3(0, 0, -1)], BACK);
	chunk->AssignNeighbor(m_chunkMap[chunkIndex + glm::ivec3(0, 0, 1)], FRONT);
	chunk->AssignNeighbor(m_chunkMap[chunkIndex + glm::ivec3(1, 0, 1)], FRONT_RIGHT);
}

void VoxelManager::GenerateChunksInRange(int count, glm::vec3 *indices)
{	
	if (count == 0)
	{
		LogToUnity("Cannot generate 0 chunks");
		return;
	}

	vector<glm::ivec3> taskIndices;
	taskIndices.reserve(count);

	int taskSize = 0;

	Chunk *chunks = new Chunk[count];
	for (int i = 0; i < count; i++)
	{
		glm::ivec3 chunkWorldIndices = glm::ivec3(indices[i]);
		m_chunkMap[chunkWorldIndices] = &chunks[i];		

		if (m_chunkInitMap[chunkWorldIndices] == false)
		{
			taskIndices[i] = chunkWorldIndices;
			m_chunkInitMap[chunkWorldIndices] = true;
			taskSize++;
		}
	}

	if (taskSize == 0) return;
	
	GenerateChunkTaskSet genTask(taskSize, chunks, taskIndices, m_voxelSize, glm::vec3(m_chunkSize));
	g_genChunkTasks.push_back(genTask);

	g_TScheduler.AddTaskSetToPipe(&g_genChunkTasks[g_genChunkTasks.size() - 1]);
}

void VoxelManager::GenerateSeamJob(int size, vector<Chunk *> chunks)
{
	GenerateSeamTask newTask(size, chunks);
	g_genSeamTasks.push_back(newTask);

	g_TScheduler.AddTaskSetToPipe(&g_genSeamTasks[g_genSeamTasks.size() - 1]);
}

int VoxelManager::GetChunkCount()
{
	return m_activeChunks;
}

int VoxelManager::GetNewChunkCount()
{
	return g_newChunks.size();
}

void VoxelManager::GetNewChunkMeshData(const int count, int * vertCount, int *triCount)
{
	for (int i = 0; i < count; i++)
	{
		vertCount[i] = g_newChunkVertCount[i];
		triCount[i] = g_newChunkTriCount[i];
	}
}

void VoxelManager::BindChunk(const glm::vec3 &indices, const GLuint vbo, const GLuint ebo)
{
	Chunk * chunk = m_chunkMap[indices];
	if (chunk != nullptr && chunk->IsActive())
	{
		chunk->SetBuffers(vbo, ebo);
		chunk->BindMesh();
	}
}
void VoxelManager::BindChunks(int count, glm::vec3 *indices, GLuint* vboArr, GLuint * eboArr)
{
	for (int i = 0; i < count; i++)
	{
		Chunk *chunk = m_chunkMap[indices[i]];
		if (chunk && chunk->GetVertexCount > 0)
			chunk->BindMesh((GLuint)(size_t)vboArr[i], (GLuint)(size_t)eboArr[i]);
	}
}

void VoxelManager::GetChunkMeshSizes(int * vertSizes, int * indiceSizes)
{	
	int i = 0;
	for (auto &value : m_chunkMap) 
	{
		if (!value.second) continue;
		if (i >= m_activeChunks) break;

		Chunk *chunk = value.second;
		if (chunk->GetVertexCount() == 0 || chunk->GetIndicesCount() == 0) 
			continue;

		LogToUnity(to_string(i) + " chunk vert size: " + to_string(chunk->GetVertexCount()));
		vertSizes[i] = chunk->GetVertexCount();
		indiceSizes[i] = chunk->GetIndicesCount();

		i++;
	}
}

void VoxelManager::GetActiveChunkPositions(int count, glm::vec3 *positions)
{
	int i = 0;
	for (auto &value : m_chunkMap)
	{
		if (!value.second) continue;
		if (i >= count) break;

		Chunk *chunk = value.second;
		if (chunk->GetVertexCount() == 0 || chunk->GetIndicesCount() == 0)
			continue;

		positions[i] = chunk->GetPosition();
		i++;
	}
}

void VoxelManager::Update(glm::vec3 playerPos)
{
	for (int i = 0; i < g_genChunkTasks.size(); i++)
	{
		if (g_genChunkTasks[i].GetIsComplete())
		{
			GenerateChunkTaskSet * task = &g_genChunkTasks[i];
			vector<Chunk*> seamChunks(task->m_SetSize);

			for (int j = 0; j < task->m_SetSize; j++)
			{
				Chunk * chunk = &task->chunks[j];
				seamChunks[j] = chunk;
				m_chunkMap[chunk->m_chunkIndex] = chunk;
			}
			for (int j = 0; j < task->m_SetSize; j++)
			{
				Chunk * chunk = &task->chunks[j];
				AssignChunkNeighbors(chunk);
				//get a list of old chunks to regenerate seams
				for (const auto neighbor : chunk->m_neighbors)
				{
					if (neighbor != nullptr && neighbor->HasGeneratedSeam() && neighbor->IsActive())
					{
						AssignChunkNeighbors(neighbor);
						seamChunks.push_back(neighbor);
					}
				}
			}

			GenerateSeamJob(seamChunks.size(), seamChunks);
			g_genChunkTasks.erase(g_genChunkTasks.begin() + i);
		}
	}

	for (int i = 0; i < g_genSeamTasks.size(); i++)
	{
		if (g_genSeamTasks[i].GetIsComplete())
		{
			GenerateSeamTask *task = &g_genSeamTasks[i];

			for (int j = 0; j < task->m_SetSize; j++)
			{				
				Chunk *chunk = task->chunks[j];
				g_newChunks.push_back(chunk);
				g_newChunkVertCount.push_back(chunk->GetVertexCount());
				g_newChunkTriCount.push_back(chunk->GetIndicesCount());
			}
			
			g_genSeamTasks.erase(g_genSeamTasks.begin() + i);
			std::cout << "ChunkGen Complete! " << std::endl;
		}

		ThrowEventToUnity(CHUNK_GEN_FINISHED);
	}
}
