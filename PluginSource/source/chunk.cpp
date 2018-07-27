#include "VoxelPlugin.hpp"

#define CHUNK_ACTIVE 1
#define GENERATED_SEAM 2
#define CHUNK_RENDERING 4
#define CHUNK_BINDED 8

static const glm::vec3 AXIS_OFFSET[3] =
{
	glm::vec3(1.f, 0.f, 0.f),
	glm::vec3(0.f, 1.f, 0.f),
	glm::vec3(0.f, 0.f, 1.f)
};

Chunk::Chunk() : m_flag(0), m_vbo(0), m_ebo(0)
{

}

Chunk::~Chunk()
{
	LogToUnity("Deleting Chunk");
	ClearBufferedData();
}

bool Chunk::Init(glm::ivec3 chunkIndices, glm::vec3 chunkSize, Density::DensityType type, int voxelSize)
{
	m_chunkIndex = chunkIndices;
	m_chunkSize = chunkSize;
	m_voxelSize = voxelSize;
	m_invVoxelSize = 1.0f / (float)m_voxelSize;

	m_terrainType = type;
	
	m_position = chunkIndices * voxelSize;
	m_position *= chunkSize;

	memset(m_neighbors, NULL, sizeof(m_neighbors));

	vector<float> densityField;

	bool active = GenerateMaterialIndices();

	if (!active)
	{
		m_flag = ~CHUNK_ACTIVE;
		return false;
	}

	FindActiveVoxels();
	if (m_nodeMap.size() == 0)
	{
		m_flag = ~CHUNK_ACTIVE;
		return false;
	}

	if (m_terrainType == Density::Terrain)
		GenerateHermiteHeightMap2D();
	else
		GenerateHermiteField();

	GenerateMesh();

	if(m_root == nullptr) return false;

	return true;
}

void Chunk::ClearBufferedData()
{
	if (m_vbo) glDeleteBuffers(1, &m_vbo);
	if (m_ebo) glDeleteBuffers(1, &m_ebo);

}

bool Chunk::SetBuffers(GLuint vbo, GLuint ebo)
{
	m_vbo = vbo;
	m_ebo = ebo;
}

bool Chunk::HasGeneratedSeam()
{
	return m_flag & GENERATED_SEAM;
}
bool Chunk::IsRendering()
{
	return m_flag & CHUNK_RENDERING;
}

bool Chunk::IsActive()
{
	return m_flag & CHUNK_ACTIVE;
}

void Chunk::SetIsRendering(bool flag)
{
	if (flag)
		m_flag |= CHUNK_RENDERING;
	else
		m_flag &= ~CHUNK_RENDERING;
}

glm::vec3 Chunk::GetPosition()
{
	return m_position;
}

void Chunk::AssignNeighbor(Chunk *chunk, Side side)
{
	m_neighbors[side] = chunk;
}

void Chunk::FindActiveVoxels()
{
	vector<glm::vec3> positions;
	vector<int> cornersArray;

	glm::vec3 gridSize = m_chunkSize + glm::vec3(1.0f);
	for (int x = 0; x < m_chunkSize.x; x++)
	{
		for (int y = 0; y < m_chunkSize.y; y++)
		{
			for (int z = 0; z < m_chunkSize.z; z++)
			{
				int corners = 0;

				for (int i = 0; i < 8; i++)
				{
					glm::ivec3 cornerIndex = glm::vec3(x, y, z) + CHILD_MIN_OFFSETS[i];
					corners |= (m_materialIndices[GETINDEXCHUNK(gridSize, cornerIndex.x, cornerIndex.y, cornerIndex.z)] << i);
				}
				if (corners == 0 || corners == 255) continue;

				positions.push_back(m_position + glm::vec3(x, y, z) * (float)m_voxelSize);
				cornersArray.push_back(corners);
			}
		}
	}

	Octree *nodes = new Octree[positions.size()];
	for (int i = 0; i < positions.size(); i++)
	{
		nodes[i].InitNode(positions[i], m_voxelSize, cornersArray[i]);
		m_nodeMap.insert(std::pair<glm::vec3, Octree*>(positions[i], &nodes[i]));
	}
}

void Chunk::GetNodesInRange(const Chunk * chunk, const glm::ivec3 minrange, const glm::ivec3 maxrange, vector<Octree*> &outputNodes)
{
	for (int x = minrange.x; x <= maxrange.x; x++)
	{
		for (int y = minrange.y; y <= maxrange.y; y++)
		{
			for (int z = minrange.z; z <= maxrange.z; z++)
			{
				const glm::vec3 key = chunk->m_position + glm::vec3(x, y, z) * (float)chunk->m_voxelSize;
				const auto & iterator = chunk->m_nodeMap.find(key);
				if (iterator != chunk->m_nodeMap.end())
				{
					outputNodes.push_back(iterator->second);
				}
			}
		}
	}
}

vector<Octree*> Chunk::FindSeamNodes()
{
	vector<Octree*> nodes;

	//get 7 neighbors
	if (m_neighbors[TOP])
	{
		GetNodesInRange(this, glm::ivec3(0, m_chunkSize.y - 1, 0), glm::ivec3(m_chunkSize), nodes);
		GetNodesInRange(m_neighbors[TOP], glm::ivec3(0), glm::ivec3(m_chunkSize.x, 0, m_chunkSize.z), nodes);
	}

	if (m_neighbors[RIGHT])
	{
		GetNodesInRange(this, glm::ivec3(m_chunkSize.x - 1, 0, 0), glm::ivec3(m_chunkSize), nodes);
		GetNodesInRange(m_neighbors[RIGHT], glm::ivec3(0), glm::ivec3(0, m_chunkSize.y, m_chunkSize.z), nodes);
	}

	if (m_neighbors[FRONT])
	{
		GetNodesInRange(this, glm::ivec3(0, 0, m_chunkSize.z - 1), glm::ivec3(m_chunkSize), nodes);
		GetNodesInRange(m_neighbors[FRONT], glm::ivec3(0), glm::ivec3(m_chunkSize.x, m_chunkSize.y, 0), nodes);
	}

	if (m_neighbors[FRONT_RIGHT])
	{		
		GetNodesInRange(m_neighbors[FRONT_RIGHT], glm::ivec3(0), glm::ivec3(0, m_chunkSize.y, 0), nodes);
	}
	
	if (m_neighbors[TOP] && m_neighbors[TOP]->m_neighbors[RIGHT])
	{
		GetNodesInRange(m_neighbors[TOP]->m_neighbors[RIGHT], glm::ivec3(0), glm::ivec3(0, 0, m_chunkSize.z), nodes);
	}

	if (m_neighbors[TOP] && m_neighbors[TOP]->m_neighbors[FRONT])
	{
		GetNodesInRange(m_neighbors[TOP]->m_neighbors[FRONT], glm::ivec3(0), glm::ivec3(m_chunkSize.x, 0, 0), nodes);
	}

	if (m_neighbors[TOP] && m_neighbors[TOP]->m_neighbors[FRONT_RIGHT])
	{
		GetNodesInRange(m_neighbors[TOP]->m_neighbors[FRONT_RIGHT], glm::ivec3(0), glm::ivec3(0, m_chunkSize.y, 0), nodes);
	}

	return nodes;
}

void Chunk::GenerateSeam()
{
	vector<Octree *> nodes = FindSeamNodes();
	if (nodes.size() == 0) return;

	Octree* root = BottomUpTreeGen(nodes, m_position);

	int currSize = m_vertices.size();
	root->GenerateVertexBuffer(m_vertices);	
	for (int i = currSize; i < m_vertices.size(); i++)
		m_vertices[i].position -= m_position;

	root->ProcessCell(m_triIndices, 1000.f);
}

void Chunk::GenerateMesh()
{
	glm::vec3 gridSize = m_chunkSize + glm::vec3(1.0f);
	float chunkMaxBound = m_position.y + m_chunkSize.y * m_voxelSize;

	for (auto &values : m_nodeMap)
	{
		Octree *node = values.second;
		FindEdgeCrossing(node, m_hermiteMap);
		if (~node->m_flag & OCTREE_ACTIVE)
			m_nodeMap.erase(values.first);
	}

	m_root = BottomUpTreeGen(m_nodeMap, m_position);

	if (m_root == nullptr)
	{
		m_flag = ~CHUNK_ACTIVE;
		return;
	}
	
	m_flag |= CHUNK_ACTIVE;


	//m_root->ClusterCellBase(2000000.f);
	m_root->GenerateVertexBuffer(m_vertices);
	for (int i = 0; i < m_vertices.size(); i++)
		m_vertices[i].position -= m_position;
	m_root->ProcessCell(m_triIndices, 2000000.f);
}

bool  Chunk::GenerateMaterialIndices()
{
	vector<float> densityField;
	bool active = true;
	switch (m_terrainType)
	{
	case Density::Terrain:
		Density::GenerateHeightMap(m_position, m_chunkSize, densityField);
		active = Density::GenerateMaterialIndices(m_position, m_chunkSize, m_materialIndices, densityField);
		break;
	case Density::Cave:
		Density::GenerateCaveIndices(m_position, m_chunkSize, m_materialIndices);
		break;
	default:
		break;
	}

	return active;
}

void Chunk::GenerateHermiteField()
{
	//loop through each voxel and calculate edge crossings
	for (int x = 0; x <= m_chunkSize.x; x++)
	{
		for (int y = 0; y <= m_chunkSize.y; y++)
		{
			for (int z = 0; z <= m_chunkSize.z; z++)
			{
				for (int axis = 0; axis < 3; axis++)
				{
					glm::ivec3 index2 = glm::vec3(x, y, z) + AXIS_OFFSET[axis];

					if (GETINDEXCHUNK(glm::ivec3(m_chunkSize + glm::vec3(1.0f)), index2.x, index2.y, index2.z) > m_materialIndices.size())
						continue;

					int m1 = m_materialIndices[GETINDEXCHUNK(glm::ivec3(m_chunkSize + glm::vec3(1.0f)), x, y, z)];
					int m2 = m_materialIndices[GETINDEXCHUNK(glm::ivec3(m_chunkSize + glm::vec3(1.0f)), index2.x, index2.y, index2.z)];

					//no material change, no edge
					if (m1 == m2)
						continue;

					glm::vec3 p1 = m_position + glm::vec3(x, y, z) * (float)m_voxelSize;
					glm::vec3 p2 = m_position + (glm::vec3(x, y, z) + AXIS_OFFSET[axis]) * (float)m_voxelSize;

					//get hermite data
					glm::vec3 p = Density::FindIntersection(m_terrainType, p1, p2);
					glm::vec3 n = Density::CalculateNormals(m_terrainType, p);

					//store in map
					EdgeInfo edge(p, n);
					m_hermiteMap[(p1 + p2) *.5f] = edge;
				}
			}
		}
	}
}

void Chunk::GenerateHermiteHeightMap2D()
{
	float chunkMaxBound = m_position.y + m_chunkSize.y * m_voxelSize;
	glm::ivec3 gridSize = m_chunkSize + glm::vec3(1.0f);

	for (auto &values : m_nodeMap)
	{
		Octree *node = values.second;
		int corners = node->m_corners;

		//find intersection for all 12 edges of the voxel
		int edgeCount = 0;
		const int MAX_CROSSINGS = 6;
		for (int i = 0; i < 12 && edgeCount < MAX_CROSSINGS; i++)
		{
			const int c1 = edgevmap[i][0];
			const int c2 = edgevmap[i][1];
					
			const int m1 = (corners >> c1) & 1;
			const int m2 = (corners >> c2) & 1;

			//no material change, no edge
			if (m1 == m2)
				continue;

			glm::vec3 p1 = node->m_minPos + CHILD_MIN_OFFSETS[c1] * (float)m_voxelSize;
			glm::vec3 p2 = node->m_minPos + CHILD_MIN_OFFSETS[c2] * (float)m_voxelSize;

			//get hermite data
			glm::vec3 p = Density::FindIntersection2D(m_terrainType, p1, p2);
			glm::vec3 n = Density::CalculateNormals2D(m_terrainType, p);

			//store in map
			EdgeInfo edge(p, n);
			glm::vec3 edgePos = (p1 + p2) * .5f;
			m_hermiteMap[(p1 + p2) *.5f] = edge;

			edgeCount++;
		}
	}
}

void Chunk::BindMesh()
{
	ClearBufferedData();
	//sanity check
	if (m_vbo == 0 || m_ebo == 0)
	{
		LogToUnity("Must set mesh VBO and EBO before binding");
		return;
	}
	if((m_vertices.size() == 0) || m_triIndices.size() == 0)
	{
		m_flag = ~CHUNK_ACTIVE;
		return;
	}

	vector<Vertex> vertices;
	vertices.reserve(m_vertices.size());
	
	for (int i = 0; i < m_vertices.size(); i++)
	{
		Vertex vert;
		vert.pos = m_vertices[i].position;
		vert.normal = m_vertices[i].normal;
		vertices.push_back(vert);
	}

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * vertices.size(), &vertices[0]);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(GLuint) * m_triIndices.size(), &m_triIndices[0]);
}

bool Chunk::IsActive()
{
	return m_flag & CHUNK_ACTIVE;
}

void Chunk::SetIsRendering(bool flag)
{
}

int Chunk::GetVertexCount()
{
	return m_vertices.size();
}

int Chunk::GetIndicesCount()
{
	return m_triIndices.size();
}
