#include "VoxelPlugin.hpp"
#include <string>

VoxelManager::VoxelManager()
{

}

VoxelManager::~VoxelManager()
{
	Close();
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
	int totalChunks = x_range * y_range * z_range;

	m_chunkMap.reserve(totalChunks);
	m_chunkMap.clear();

	m_chunks = new Chunk[totalChunks];

	Density::SetVoxelSize(m_voxelSize);
	Density::SetMaxVoxelHeight(maxHeight);
	Density::Initialize();
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

void VoxelManager::Close()
{
	m_chunkMap.clear();
	m_chunkMap.empty();
}

void VoxelManager::GenerateChunksInRange()
{	
	int x_range = 2 * m_renderRange + 1;
	int z_range = x_range;
	int y_range = 3;
	int totalChunks = x_range * y_range * z_range;

	glm::vec3 playerPos = glm::vec3(0);

	const glm::ivec3 playerChunkIndex = glm::ivec3(0, 0, 0);
	m_playerPos = playerChunkIndex;

	for (int x = -m_renderRange; x <= m_renderRange; x++)
	{
		//omp_set_num_threads(4);
		for (int z = -m_renderRange; z <= m_renderRange; z++)
		{
			//#pragma omp parallel for
			for (int y = -1; y <= 1; y++)
			{
				int chunkIndex = (y_range * x_range) * (x + m_renderRange) + y_range * (z + m_renderRange) + (y + 1);
				m_chunks[chunkIndex].Init(playerChunkIndex + glm::ivec3(x, y, z), glm::vec3(m_chunkSize), m_voxelSize);
			}
		}
	}

	for (int i = 0; i < totalChunks; i++)
	{
		Chunk * chunk = &m_chunks[i];

		m_chunkMap[chunk->m_chunkIndex] = chunk;
		m_renderList.push_back(chunk);
	}

	for (auto &value : m_chunkMap) {
		if (!value.second) continue;

		Chunk *chunk = value.second;
		AssignChunkNeighbors(value.second);
		chunk->GenerateSeam();
		if (chunk->GetVertexCount() == 0 || chunk->GetIndicesCount() == 0)
			continue;
		m_activeChunks++;
	}
}

int VoxelManager::GetChunkCount()
{
	return m_activeChunks;
}

void VoxelManager::BindChunks(int count, GLuint* vboArr, GLuint * eboArr)
{
	int i = 0;
	for (auto &value : m_chunkMap) {
		if (!value.second) continue;
		if (i >= count) break;

		Chunk *chunk = value.second;
		if (chunk->GetVertexCount() == 0 || chunk->GetIndicesCount() == 0)
			continue;

		string msg = "BindChunk: i " + to_string(i);
		LogToUnity(msg.c_str());

		chunk->BindMesh((GLuint)(size_t)vboArr[i], (GLuint)(size_t)eboArr[i]);
		
		i++;
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

void VoxelManager::Update()
{
	//const glm::ivec3 playerChunkIndex = glm::vec3(1, 0, 1) *g_game->GetPlayerPosition() / (float)m_voxelSize / (float)m_chunkSize;
	//if (m_playerPos != playerChunkIndex)
	//{
	//	m_playerPos = playerChunkIndex;
	//	//#pragma omp concurrent
	//	GenerateChunksInRange();
	//}

	//if (m_newChunks.size() != 0)
	//{
	//	for (auto &chunk : m_newChunks) 
	//	{
	//		chunk->BindMesh();
	//		m_renderList.push_back(chunk);
	//	}
	//	m_newChunks.clear();
	//}
}

void VoxelManager::Render()
{
	for (auto &chunk : m_renderList )
	{
		//sanity check
		if (chunk) 
			chunk->Render();
	}
}