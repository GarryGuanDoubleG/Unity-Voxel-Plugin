#pragma once

class VoxelManager
{
	int m_voxelSize, 
		m_renderRange,
		m_chunkSize;

	int m_activeChunks;

	glm::ivec3 m_playerPos;

	unordered_map<glm::ivec3, Chunk*> m_chunkMap;
	std::vector<Chunk*> m_newChunks;
	std::vector<Chunk*> m_renderList;

	Chunk *m_chunks;

public:
	VoxelManager();
	~VoxelManager();

	void Init(int voxelSize, int range, int chunkSize, float maxHeight);	
	void AssignChunkNeighbors(Chunk *chunk);
	void Close();
	void GenerateChunksInRange();
	int GetChunkCount();
	void BindChunks(int count,  GLuint* vboArr, GLuint* eboArr);
	void GetChunkMeshSizes(int *vertSizes, int *indiceSizes);
	void Update();
	void Render();
};