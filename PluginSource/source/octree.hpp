#pragma once

#define OCTREE_ACTIVE 1
#define OCTREE_INUSE 2
#define OCTREE_LEAF 4
#define OCTREE_INNER 8
#define OCTREE_PSUEDO 16

#define GETINDEXXYZ(x,y,z) ((4 * x) + (2 * y) + z)
#define GETINDEXXZ(x,z) ((2 * x) + z)
#define GETINDEXCHUNK(chunkSize, a, b, c) ((chunkSize.x * chunkSize.y * a) + (chunkSize.y * b) + c)
//Finds the index of a chunk using x z coordinates. Mainly for heightmap approach
#define GETINDEXCHUNKXZ(chunkSize, a, b) ((chunkSize.x * a) + b)
const glm::vec3 CHILD_MIN_OFFSETS[] =
{
	// needs to match the vertunordered_map from Dual Contouring impl
	glm::vec3(0, 0, 0),
	glm::vec3(0, 0, 1),
	glm::vec3(0, 1, 0),
	glm::vec3(0, 1, 1),
	glm::vec3(1, 0, 0),
	glm::vec3(1, 0, 1),
	glm::vec3(1, 1, 0),
	glm::vec3(1, 1, 1),
};

struct EdgeInfo
{
	EdgeInfo() {};
	EdgeInfo(glm::vec3 p, glm::vec3 n) :pos(p), normal(n) {};
	glm::vec3 pos;
	glm::vec3 normal;
};


class Octree;


void FindEdgeCrossing(Octree *node, const unordered_map<glm::vec3, EdgeInfo> &hermite_map);

Octree * BottomUpTreeGen(const unordered_map<glm::vec3, Octree *> &unordered_map, const glm::vec3 &chunkPos);
Octree * BottomUpTreeGen(const vector<Octree*> &nodes, const glm::vec3 &chunkPos);

class Octree
{
public:
	int m_index;
	unsigned char m_child_index;
	unsigned char m_corners;
	unsigned char m_vertex_count;

	Octree * m_children[8];
	VoxelVertex *m_vertices;

	char m_flag;
	char m_childMask;

	int m_size;
	glm::vec3 m_minPos;
public:
	Octree();
	~Octree();
	void InitNode(glm::vec3 minPos, int size, int corners);
	void DestroyNode();

	
	void GenerateVertexBuffer(std::vector<VoxelVertex>& v_out);

	void ProcessCell(std::vector<GLuint>& indexes, const float threshold);
	void ProcessFace(Octree ** nodes, int direction, std::vector<GLuint>& indexes, float threshold);
	void ProcessEdge(Octree ** nodes, int direction, std::vector<GLuint>& indexes, float threshold);
	void ProcessIndexes(Octree ** nodes, int direction, std::vector<GLuint>& indexes, float threshold);
	void ClusterCellBase(float error);
	void ClusterCell(float error);
	void ClusterFace(Octree ** nodes, int direction, int & surface_index, std::vector<VoxelVertex*>& collected_vertices);
	void ClusterEdge(Octree ** nodes, int direction, int & surface_index, std::vector<VoxelVertex*>& collected_vertices);
	void ClusterIndexes(Octree ** nodes, int direction, int & max_surface_index, std::vector<VoxelVertex*>& collected_vertices);
	void AssignSurface(std::vector<VoxelVertex*>& vertices, int from, int to);
};