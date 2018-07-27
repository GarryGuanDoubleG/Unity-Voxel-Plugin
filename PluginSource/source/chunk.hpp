#pragma once

enum Side : GLuint
{
	TOP = 0,
	BOTTOM,
	LEFT,
	RIGHT,
	BACK,
	FRONT,
	FRONT_RIGHT
};

const glm::ivec3 g_neighbors[6] =
{
	glm::ivec3(-1,0,0),
	glm::ivec3(1,0,0),
	glm::ivec3(0,-1,0),
	glm::ivec3(0, 1,0),
	glm::ivec3(0,0,-1),
	glm::ivec3(0,0, 1)
};
//for selection func
typedef std::function<bool(const glm::ivec3&, const glm::ivec3&)> FilterNodesFunc;


class Chunk
{
	Octree * m_root;

	int m_flag;

	int m_voxelSize;
	float m_invVoxelSize; // inverse voxel size (1 / voxelSize)
	glm::vec3 m_chunkSize;
	glm::vec3 m_position;

	Density::DensityType m_terrainType;

	//proc mesh
	GLuint m_vao, m_vbo, m_ebo;
public:
	Chunk *m_neighbors[7];
	glm::ivec3 m_chunkIndex;	

	unordered_map<glm::vec3, EdgeInfo> m_hermiteMap;
	vector<int> m_materialIndices;
	unordered_map<glm::vec3, Octree*> m_nodeMap;

	vector<VoxelVertex> m_vertices;
	vector<GLuint> m_triIndices;
	vector<GLboolean> m_flipVerts;
public:
	Chunk();
	~Chunk();

	bool Init(glm::ivec3 chunkIndices, glm::vec3 chunkSize, Density::DensityType type, int voxelSize);
	
	void ClearBufferedData();

	bool SetBuffers(GLuint vbo, GLuint ebo);

	bool HasGeneratedSeam();

	bool IsRendering();

	bool IsActive();

	void SetIsRendering(bool flag);

	glm::vec3 GetPosition();

	void AssignNeighbor(Chunk * chunk, Side side);

	void FindActiveVoxels();
	
	void GetNodesInRange(const Chunk * chunk, const glm::ivec3 minrange, const glm::ivec3 maxrange, vector<Octree*> &outputNodes);
	vector<Octree*> FindSeamNodes();
	void GenerateSeam();	
	void GenerateMesh();

	bool GenerateMaterialIndices();

	void GenerateHermiteField();
	
	void GenerateHermiteHeightMap2D();

	void BindMesh();
	
	bool IsActive();
	int GetVertexCount();
	int GetIndicesCount();
};