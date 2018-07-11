#include "VoxelPlugin.hpp"

#define CHUNK_ACTIVE 1


static const glm::vec3 AXIS_OFFSET[3] =
{
	glm::vec3(1.f, 0.f, 0.f),
	glm::vec3(0.f, 1.f, 0.f),
	glm::vec3(0.f, 0.f, 1.f)
};



Chunk::Chunk() : m_flag(0), m_init(false), m_vao(0), m_vbo(0), m_ebo(0)
{
}

Chunk::~Chunk()
{
	ClearBufferedData();
}

bool Chunk::Init(glm::ivec3 chunkIndices, glm::vec3 chunkSize, int voxelSize)
{
	m_chunkIndex = chunkIndices;
	m_chunkSize = chunkSize;
	m_voxelSize = voxelSize;
	m_invVoxelSize = 1.0f / (float)m_voxelSize;

	m_position = chunkIndices * voxelSize;
	m_position *= chunkSize;

	m_init = true;
	memset(m_neighbors, NULL, sizeof(m_neighbors));

	vector<float> heightMap;
	Density::GenerateHeightMap(m_position, m_chunkSize, heightMap);
	
	bool active = Density::GenerateMaterialIndices(m_position, m_chunkSize, m_materialIndices, heightMap);

	if (!active)
	{
		m_flag = ~CHUNK_ACTIVE;
		return false;
	}

	FindActiveVoxels();

	GenerateHermiteHeightMap2D(heightMap);
	GenerateMesh(heightMap);

	if(m_root == nullptr) return false;

	return true;
}

bool Chunk::ClearBufferedData()
{
	//if (m_vao) glDeleteVertexArrays(1, &m_vao);
	//if (m_vbo) glDeleteBuffers(1, &m_vbo);
	//if (m_ebo) glDeleteBuffers(1, &m_ebo);

	m_vao = m_vbo = m_ebo = 0;
	m_vertices.empty();
	m_triIndices.empty();
	m_nodeMap.empty();
	m_hermiteMap.empty();

	if (m_root) 
		delete m_root;
	m_root = nullptr;
	
	return true;
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

void Chunk::GenerateMesh()
{
	//find active nodes & their edge crossings
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
					corners |= (m_materialIndices[GETINDEXCHUNK(glm::ivec3(m_chunkSize + glm::vec3(1.0f)), cornerIndex.x, cornerIndex.y, cornerIndex.z)] << i);
				}

				//no material change. Isosurface not in voxel
				if(corners == 0 || corners == 255) continue;

				Octree *node = new Octree;

				glm::vec3 leafPos = m_position + glm::vec3(x, y, z) * (float)m_voxelSize;
				node->InitNode(leafPos, m_voxelSize, corners);
				FindEdgeCrossing(node, m_hermiteMap);

				if(node->m_flag & OCTREE_ACTIVE)
					m_nodeMap.insert(std::pair<glm::vec3, Octree*>(leafPos, node));
				else
					delete node;
			}
		}
	}

	m_root = BottomUpTreeGen(m_nodeMap, m_position);

	if(m_root == nullptr)
	{
		m_flag = ~CHUNK_ACTIVE;
		return;
	}

	m_flag |= CHUNK_ACTIVE;
	
	m_root->ClusterCellBase(2000000.f);
	m_root->GenerateVertexBuffer(m_vertices);
	m_root->ProcessCell(m_triIndices, 2000000.f);
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

	root->GenerateVertexBuffer(m_vertices);
	root->ProcessCell(m_triIndices, 1000.f);
}

void Chunk::GenerateMesh(vector<float> &heightmap)
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
	m_root->ProcessCell(m_triIndices, 2000000.f);
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
					
					if(GETINDEXCHUNK(glm::ivec3(m_chunkSize + glm::vec3(1.0f)), index2.x, index2.y, index2.z) > m_materialIndices.size())
						continue;

					int m1 = m_materialIndices[GETINDEXCHUNK(glm::ivec3(m_chunkSize + glm::vec3(1.0f)), x, y, z)];
					int m2 = m_materialIndices[GETINDEXCHUNK(glm::ivec3(m_chunkSize + glm::vec3(1.0f)), index2.x, index2.y, index2.z)];

					//no material change, no edge
					if(m1 == m2) 
						continue;

					glm::vec3 p1 = m_position + glm::vec3(x, y, z) * (float)m_voxelSize;
					glm::vec3 p2 = m_position + (glm::vec3(x, y, z) + AXIS_OFFSET[axis]) * (float)m_voxelSize;

					//get hermite data
					glm::vec3 p = Density::FindIntersection(m_densityType, p1, p2);
					glm::vec3 n = Density::CalculateNormals(m_densityType, p);

					if (((p1 + p2) *.5f) == glm::vec3(-9600, -1792, -9984))
					{
						float afd = 1.0f;
					}

					//store in map
					EdgeInfo edge(p, n);
					m_hermiteMap[(p1 + p2) *.5f] = edge;
				}
			}
		}
	}

}

void Chunk::GenerateHermiteHeightMap2D(vector<float> &heightmap)
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
			glm::vec3 p = Density::FindIntersection2D(Density::Terrain, p1, p2);
			glm::vec3 n = Density::CalculateNormals2D(Density::Terrain, p);

			//store in map
			EdgeInfo edge(p, n);
			glm::vec3 edgePos = (p1 + p2) * .5f;
			m_hermiteMap[(p1 + p2) *.5f] = edge;

			edgeCount++;
		}
	}
}

void * Chunk::BeginModifyVertexBuffer(int &size)
{
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
	glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * m_vertices.size(), nullptr, GL_DYNAMIC_DRAW);
	void *vboDataPtr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

	return vboDataPtr;
}

void Chunk::EndModifyVertexBuffer()
{
	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glUnmapBuffer(GL_ARRAY_BUFFER);
}

void * Chunk::BeginModifyIndexBuffer(int &size)
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	size = 0;
	glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER, GL_BUFFER_SIZE, &size);
	void *eboDataPtr = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);

	return eboDataPtr;
}

void Chunk::EndModifyIndexBuffer()
{
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
}

void Chunk::BindMesh(GLuint vbo, GLuint ebo)
{
	//ClearBufferedData();

	m_vbo = vbo;
	m_ebo = ebo;

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

	string msg = "Vertices binding size " + to_string(vertices.size());
	LogToUnity(msg.c_str());

	msg = "Triangle size " + to_string(m_triIndices.size());
	LogToUnity(msg.c_str());

	msg = "VBO: " + to_string(m_vbo) + ", EBO: " + to_string(m_ebo);
	LogToUnity(msg.c_str());

	glGenVertexArrays(1, &m_vao);
	glBindVertexArray(m_vao);

	glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * vertices.size(), &vertices[0]);
	//glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * vertices.size(), &vertices[0], GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, sizeof(GLuint) * m_triIndices.size(), &m_triIndices[0]);
	//glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * m_triIndices.size(), &m_triIndices[0], GL_DYNAMIC_DRAW);

	//glEnableVertexAttribArray(0);
	//glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, pos));
	////now normals
	//glEnableVertexAttribArray(1);
	//glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (GLvoid*)offsetof(Vertex, normal));

	//GLint bufferSize = 0;
	//void *vboDataPtr = BeginModifyVertexBuffer(bufferSize);

	//LogToUnity("BufferSize: " + to_string(bufferSize));
	//LogToUnity("VBO: " + to_string(m_vbo) + ", EBO: " + to_string(m_ebo));
	//if (!vboDataPtr)
	//{
	//	LogToUnity("VBO IS NULLL!");
	//	return;
	//}

	//int vertexStride = bufferSize / m_vertices.size();
	//char *bufferPtr = (char *)vboDataPtr;
	//Vertex *vertexPtr = (Vertex*)bufferPtr;

	//for (int i = 0; i < m_vertices.size(); i++)
	//{
	//	const VoxelVertex &src = m_vertices[i];
	//	Vertex &dst = *(Vertex*)bufferPtr;

	//	dst.pos.x = src.position.x;
	//	dst.pos.y = src.position.y;
	//	dst.pos.z = src.position.z;
	//	dst.normal.x = src.normal.x;
	//	dst.normal.y = src.normal.y;
	//	dst.normal.z = src.normal.z;
	//	bufferPtr += vertexStride;
	//}

	//string msg = "VBO buffer size: " + to_string(bufferSize);
	//LogToUnity(msg);
	//EndModifyVertexBuffer();

	//int indexSize;
	//void *indexBufferPtr = BeginModifyIndexBuffer(indexSize);
	//if (!indexBufferPtr)
	//{
	//	LogToUnity("EBO IS NULLL!");
	//	return;
	//}
	//msg = "EBO buffer size: " + to_string(indexSize);
	//LogToUnity(msg);

	//auto indexPtr = (GLuint *)(indexBufferPtr);
	//for (int i = 0; i < m_triIndices.size(); i++)
	//	indexPtr[i] = m_triIndices[i];

	//EndModifyIndexBuffer();
}

bool Chunk::IsActive()
{
	return m_flag & CHUNK_ACTIVE;
}

int Chunk::GetVertexCount()
{
	return m_vertices.size();
}

int Chunk::GetIndicesCount()
{
	return m_triIndices.size();
}

void Chunk::Render()
{
	if(~m_flag & CHUNK_ACTIVE) return;

	glBindVertexArray(m_vao);
	glDrawElements(GL_TRIANGLES, m_triIndices.size(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}