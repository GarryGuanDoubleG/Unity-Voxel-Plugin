#include "VoxelPlugin.hpp"
#include "enkiTS\TaskScheduler.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
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
	g_newChunks.clear();
	g_newChunkTriCount.clear();
	g_newChunkVertCount.clear();
	g_genChunkTasks.clear();
	g_genSeamTasks.clear();
	g_TScheduler.~TaskScheduler();
}


void VoxelManager::Init(int voxelSize, int range, float startRange, int chunkSize, float maxHeight)
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

	int startChunkCount = (startRange * 2 + 1);//x, y, z
	startChunkCount = startChunkCount * startChunkCount * startChunkCount;
	glm::vec3 * indices = new glm::vec3[startChunkCount];
	int i = 0;
	for (float x = -startRange; x <= startRange; x++)
	{
		for (float z = -startRange; z <= startRange; z++)
		{
			for (float y = -startRange; y <= startRange; y++)
			{
				indices[i++] = glm::ivec3(x, y, z);
			}
		}
	}

	//generate new chunks and wait

	GenerateChunksInRange(startChunkCount, indices);
	delete indices;
	g_TScheduler.WaitforAll();
	CheckChunkJobs();	
	g_TScheduler.WaitforAll();
	CheckSeamJobs();

	std::vector<glm::vec3> renderIndices;
	renderIndices.reserve(m_renderRange * m_renderRange * m_renderRange);
	int count = 0;
	//queue up the rest of the chunks within render range
	for (int x = -m_renderRange; x <= m_renderRange; x++)
	{
		for (int z = -m_renderRange; z <= m_renderRange; z++)
		{
			for (int y = -m_renderRange; y <= m_renderRange; y++)
			{
				glm::vec3 index(x, y, z);
				if (!m_chunkInitMap[index])
				{
					renderIndices.push_back(index);
					count++;
				}
			}
		}
	}

	GenerateChunksInRange(count, &renderIndices[0]);
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

	vector<glm::ivec3> taskIndices(count);	
	int taskSize = 0;	
	for (int i = 0; i < count; i++)
	{
		glm::ivec3 chunkWorldIndices = glm::ivec3(indices[i]);
		if (m_chunkInitMap[chunkWorldIndices] == false)
		{			
			taskIndices[i] = chunkWorldIndices;
			m_chunkInitMap[chunkWorldIndices] = true;
			taskSize++;
		}
	}
	Chunk *chunks = new Chunk[taskSize];
	for (int i = 0; i < taskSize; i++)	
		m_chunkMap[taskIndices[i]] = &chunks[i];

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

void VoxelManager::GetNewChunkTriangles(const int count, int *triArr)
{
	for (int i = 0; i < count; i++)
	{
		//vertCount[i] = g_newChunkVertCount[i];
		triArr[i] = g_newChunkTriCount[i];
	}
}

void VoxelManager::GetNewChunkIndices(const int count, glm::vec3 * indices)
{
	int i = 0;
	for (const auto chunk : g_newChunks)
	{
		if (i >= count) break;

		indices[i] = chunk->m_chunkIndex;
		i++;
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
		if (chunk && chunk->GetVertexCount() > 0)
		{
			chunk->SetBuffers((GLuint)(size_t)vboArr[i], (GLuint)(size_t)eboArr[i]);
			chunk->BindMesh();			
		}
	}

	g_newChunks.clear();
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

void VoxelManager::CheckChunkJobs()
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
}

void VoxelManager::CheckSeamJobs()
{
	for (int i = 0; i < g_genSeamTasks.size(); i++)
	{
		if (g_genSeamTasks[i].GetIsComplete())
		{
			GenerateSeamTask *task = &g_genSeamTasks[i];

			if (g_newChunks.size() == 0)
				g_newChunks.reserve(task->m_SetSize);
			for (int j = 0; j < task->m_SetSize; j++)
			{
				Chunk *chunk = task->chunks[j];
				g_newChunks.push_back(chunk);
				g_newChunkVertCount.push_back(chunk->GetVertexCount());
				g_newChunkTriCount.push_back(chunk->GetIndicesCount());
			}

			g_genSeamTasks.erase(g_genSeamTasks.begin() + i);
			std::cout << "ChunkGen Complete! " << std::endl;			

			if (g_newChunks.size() > 0)
				ThrowEventToUnity(CHUNK_GEN_FINISHED);
		}
	}
}

void VoxelManager::Update(glm::vec3 playerPos)
{
	glm::ivec3 newPlayerChunkIndex = glm::ivec3(playerPos) / m_chunkSize / m_voxelSize;

	CheckChunkJobs();
	CheckSeamJobs();
}
