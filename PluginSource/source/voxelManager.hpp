#pragma once

class VoxelManager
{
	int m_voxelSize, 
		m_renderRange,
		m_chunkSize;

	int m_activeChunks,
		m_totalChunks;

	glm::ivec3 m_playerChunkIndex;	

	unordered_map<glm::ivec3, Chunk*> m_chunkMap;
	unordered_map<glm::ivec3, bool> m_chunkInitMap;
	std::vector<Chunk*> m_newChunks;
	std::vector<Chunk*> m_chunks;

public:
	VoxelManager();

	~VoxelManager();

	void Init(int voxelSize, int range, float startRange, int chunkSize, float maxHeight);

	void AssignChunkNeighbors(Chunk *chunk);

	void GenerateChunksInRange(int count, glm::vec3 * indices);
	
	void GenerateSeamJob(int size, vector<Chunk*> chunks);

	int GetChunkCount();

	int GetNewChunkCount();

	void GetNewChunkTriangles(const int count, int * triCount);

	//void GetNewChunkMeshData(const int count, int * vertCount, int * triCount);

	void GetNewChunkIndices(const int count, glm::vec3 *indices);

	void BindChunk(const glm::vec3 & indices, const GLuint vbo, const GLuint ebo);

	void BindChunks(int count, glm::vec3 * indices, GLuint * vboArr, GLuint * eboArr);

	void GetChunkMeshSizes(int *vertSizes, int *indiceSizes);

	void GetActiveChunkPositions(int count, glm::vec3 * positions);

	void CheckChunkJobs();

	void CheckSeamJobs();

	void Update(glm::vec3 playerPos);
};