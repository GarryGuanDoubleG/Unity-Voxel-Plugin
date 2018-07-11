#include "VoxelPlugin.hpp"
#include <set>
#include <mutex>

const float QEF_ERROR = 1e-6f;
const int QEF_SWEEPS = 4;

void FindEdgeCrossing(Octree *node, const unordered_map<glm::vec3, EdgeInfo> &hermite_map)
{
	int v_edges[4][12];

	int v_index = 0;
	int e_index = 0;

	//DMC lookup tables
	for (int e = 0; e < 16; e++)
	{
		int code = edge_table[node->m_corners][e];

		if(code == -2)
		{
			v_edges[v_index++][e_index] = -1;
			break;
		}
		if(code == -1)
		{
			v_edges[v_index++][e_index] = -1;
			e_index = 0;
			continue;
		}

		v_edges[v_index][e_index++] = code;
	}

	node->m_vertices = new VoxelVertex[v_index];
	node->m_vertex_count = v_index;


	if(node->m_vertex_count > 0)
		node->m_flag |= OCTREE_ACTIVE | OCTREE_LEAF;

	for (int i = 0; i < v_index; i++)
	{
		int edgeCount = 0;
		int k = 0;
		glm::vec3 pos;
		glm::vec3 averagePos;
		glm::vec3 averageNormal(0);
		QEFSolver qef;

		int ei[12] = { 0 };
		while (v_edges[i][k] != -1)
		{
			ei[v_edges[i][k]] = 1;
			glm::vec3 a = node->m_minPos + corner_deltas_f[edge_pairs[v_edges[i][k]][0]] * (float)node->m_size;
			glm::vec3 b = node->m_minPos + corner_deltas_f[edge_pairs[v_edges[i][k]][1]] * (float)node->m_size;

			glm::vec3 edgePos = (a + b) * .5f;

			const auto iter = hermite_map.find(edgePos);
			if(iter != hermite_map.end())
			{
				EdgeInfo edge = iter->second;

				averagePos += edge.pos;
				averageNormal += edge.normal;
				qef.add(edge.pos.x, edge.pos.y, edge.pos.z, edge.normal.x, edge.normal.y, edge.normal.z);
				edgeCount++;
			}
			else
			{
				//slog("couldnt find edge");
				float dsadas = 1.0f;
			}
			k++;
		}

		assert(edgeCount != 0);

		averagePos /= (float)edgeCount;
		averageNormal /= (float)edgeCount;
		averageNormal = glm::normalize(averageNormal);

		Vec3 p;
		qef.solve(p, 1e-6, 4, 1e-6);

		node->m_vertices[i].index = -1; //set during vertex buffer gen
		node->m_vertices[i].parent = 0;
		node->m_vertices[i].error = 0;
		node->m_vertices[i].normal = averageNormal;
		node->m_vertices[i].euler = 1;
		node->m_vertices[i].in_cell = node->m_child_index;
		node->m_vertices[i].flags |= VoxelVertexFlags::COLLAPSIBLE | VoxelVertexFlags::FACEPROP2;
		node->m_vertices[i].textureID = 0;
		node->m_vertices[i].position = glm::vec3(p.x, p.y, p.z);
		//node->m_vertices[i].position = averagePos;


		memcpy(node->m_vertices[i].eis, ei, sizeof(int) * 12);
		memcpy(&node->m_vertices[i].qef, &qef, sizeof(qef));
	}
}

Octree * BottomUpTreeGen(const unordered_map<glm::vec3, Octree *> &map, const glm::vec3 &chunkPos)
{
	if(map.size() < 1) return nullptr;

	vector<Octree *> tree;

	for (auto kv : map)
		tree.push_back(kv.second);

	while (tree.size() > 1)
	{
		vector<Octree *> parents;

		for (vector<Octree*>::iterator it = tree.begin(); it < tree.end(); ++it)
		{
			Octree *currNode = *it;
			bool foundParent = false;
			int parentSize = currNode->m_size << 1; //twice the size

			glm::ivec3 currPos = currNode->m_minPos;
			glm::ivec3 floorChunkPos = chunkPos;
			glm::vec3 parentPos = currPos - ((currPos - floorChunkPos) % parentSize);

			int x = currPos.x >= parentPos.x && currPos.x < parentPos.x + currNode->m_size ? 0 : 1;
			int y = currPos.y >= parentPos.y && currPos.y < parentPos.y + currNode->m_size ? 0 : 1;
			int z = currPos.z >= parentPos.z && currPos.z < parentPos.z + currNode->m_size ? 0 : 1;

			int index = GETINDEXXYZ(x, y, z);

			for (vector<Octree *>::iterator it = parents.begin(); it != parents.end(); it++)
			{
				Octree * parent = *it;
				if(parent->m_minPos == parentPos)
				{
					foundParent = true;
					parent->m_children[index] = currNode;
					parent->m_childMask |= 1 << index;
					break;
				}
			}
			if(!foundParent)
			{
				Octree *parent = new Octree();
				parent->InitNode(parentPos, parentSize, 0);
				parent->m_flag |= OCTREE_ACTIVE | OCTREE_INNER;
				parent->m_childMask |= 1 << index;
				parent->m_children[index] = currNode;

				parents.push_back(parent);
			}
		}

		tree = parents;
	}

	return tree[0];
}

Octree * BottomUpTreeGen(const vector<Octree*> &nodes, const glm::vec3 &chunkPos)
{
	vector<Octree*> tree = nodes;
	while (tree.size() > 1)
	{
		vector<Octree *> parents;

		for (vector<Octree*>::iterator it = tree.begin(); it < tree.end(); ++it)
		{
			Octree *currNode = *it;
			bool foundParent = false;
			int parentSize = currNode->m_size << 1; //twice the size

			glm::ivec3 currPos = currNode->m_minPos;
			glm::ivec3 floorChunkPos = chunkPos;
			glm::vec3 parentPos = currPos - ((currPos - floorChunkPos) % parentSize);

			int x = currPos.x >= parentPos.x && currPos.x < parentPos.x + currNode->m_size ? 0 : 1;
			int y = currPos.y >= parentPos.y && currPos.y < parentPos.y + currNode->m_size ? 0 : 1;
			int z = currPos.z >= parentPos.z && currPos.z < parentPos.z + currNode->m_size ? 0 : 1;

			int index = GETINDEXXYZ(x, y, z);

			for (vector<Octree *>::iterator it = parents.begin(); it != parents.end(); it++)
			{
				Octree * parent = *it;
				if (parent->m_minPos == parentPos)
				{
					foundParent = true;
					parent->m_children[index] = currNode;
					parent->m_childMask |= 1 << index;
					break;
				}
			}
			if (!foundParent)
			{
				Octree *parent = new Octree();
				parent->InitNode(parentPos, parentSize, 0);
				parent->m_flag |= OCTREE_ACTIVE | OCTREE_INNER;
				parent->m_childMask |= 1 << index;
				parent->m_children[index] = currNode;

				parents.push_back(parent);
			}
		}

		tree = parents;
	}

	return tree[0];
}

Octree::Octree() : m_flag (0), m_vertices(nullptr), m_corners(0), m_vertex_count(0)
{
}

Octree::~Octree()
{
	for (int i = 0; i < 8; i++)
	{
		if (m_children[i] != nullptr)
		{
			delete m_children[i];			
		}

		if (m_vertices != nullptr)
			delete m_vertices;
	}
}

void Octree::InitNode(glm::vec3 minPos, int size, int corners)
{
	m_minPos = minPos;
	m_size = size;
	m_corners = corners;
	m_index = 0;
	m_childMask = 0;
	memset(m_children, 0, sizeof(m_children));

	m_flag = OCTREE_INUSE;
}

void Octree::DestroyNode()
{
//	if(m_flag & OCTREE_INNER)
//	{
//		for (int i = 0; i < 8; i++)
//		{
////			Octree *child = (m_children + sizeof(Octree) * i);
//			child->DestroyNode();
//		}
//	}

	m_flag = m_flag >> 8;
}

void Octree::GenerateVertexBuffer(std::vector<VoxelVertex>& v_out)
{
	if(~m_flag & OCTREE_LEAF)
	{
		for (int i = 0; i < 8; i++)
		{
			if(m_children[i])
				m_children[i]->GenerateVertexBuffer(v_out);
		}
	}

	if(!m_vertices || m_vertex_count == 0)
		return;

	for (int i = 0; i < m_vertex_count; i++)
	{
		m_vertices[i].index = v_out.size();
		VoxelVertex v;
		Vec3 p;

		m_vertices[i].qef.solve(p, 1e-6, 4, 1e-6);
		//m_vertices[i].position = glm::vec3(p.x, p.y, p.z);
		v.position = glm::vec3(p.x, p.y, p.z);
		v.normal = m_vertices[i].normal;
		//v.color = World::GetColor(v.position);
		v_out.push_back(v);
	}
}

void Octree::ProcessCell(std::vector<GLuint>& indexes, float threshold)
{
	if(m_flag & OCTREE_LEAF)
		return;

	for (int i = 0; i < 8; i++)
	{
		if(m_children[i])
			m_children[i]->ProcessCell(indexes, threshold);
	}

	Octree* face_nodes[2];
	Octree* edge_nodes[4];
	for (int i = 0; i < 12; i++)
	{
		face_nodes[0] = m_children[edge_pairs[i][0]];
		face_nodes[1] = m_children[edge_pairs[i][1]];

		ProcessFace(face_nodes, edge_pairs[i][2], indexes, threshold);
	}

	for (int i = 0; i < 6; i++)
	{
		edge_nodes[0] = m_children[cell_proc_edge_mask[i][0]];
		edge_nodes[1] = m_children[cell_proc_edge_mask[i][1]];
		edge_nodes[2] = m_children[cell_proc_edge_mask[i][2]];
		edge_nodes[3] = m_children[cell_proc_edge_mask[i][3]];

		ProcessEdge(edge_nodes, cell_proc_edge_mask[i][4], indexes, threshold);
	}
}

void Octree::ProcessFace(Octree ** nodes, int direction, std::vector<GLuint>& indexes, float threshold)
{
	if(!nodes[0] || !nodes[1])
		return;

	if(~nodes[0]->m_flag & OCTREE_LEAF || ~nodes[1]->m_flag & OCTREE_LEAF)
	{
		Octree* face_nodes[2];
		Octree* edge_nodes[4];
		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 2; j++)
			{
				if(nodes[j]->m_flag & OCTREE_LEAF)
					face_nodes[j] = nodes[j];
				else
					face_nodes[j] = nodes[j]->m_children[face_proc_face_mask[direction][i][j]];
			}

			ProcessFace(face_nodes, face_proc_face_mask[direction][i][2], indexes, threshold);
		}

		const int orders[2][4] =
		{
			{ 0, 0, 1, 1 },
			{ 0, 1, 0, 1 },
		};

		for (int i = 0; i < 4; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				if(nodes[orders[face_proc_edge_mask[direction][i][0]][j]]->m_flag & OCTREE_LEAF)
					edge_nodes[j] = nodes[orders[face_proc_edge_mask[direction][i][0]][j]];
				else
					edge_nodes[j] = nodes[orders[face_proc_edge_mask[direction][i][0]][j]]->m_children[face_proc_edge_mask[direction][i][1 + j]];
			}

			ProcessEdge(edge_nodes, face_proc_edge_mask[direction][i][5], indexes, threshold);
		}
	}
}

void Octree::ProcessEdge(Octree ** nodes, int direction, std::vector<GLuint>& indexes, float threshold)
{
	if(!nodes[0] || !nodes[1] || !nodes[2] || !nodes[3])
		return;

	if(nodes[0]->m_flag & OCTREE_LEAF && nodes[1]->m_flag & OCTREE_LEAF && nodes[2]->m_flag & OCTREE_LEAF && nodes[3]->m_flag & OCTREE_LEAF)
		ProcessIndexes(nodes, direction, indexes, threshold);
	else
	{
		Octree* edge_nodes[4];
		for (int i = 0; i < 2; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				if(nodes[j]->m_flag & OCTREE_LEAF)
					edge_nodes[j] = nodes[j];
				else
					edge_nodes[j] = nodes[j]->m_children[edge_proc_edge_mask[direction][i][j]];
			}

			ProcessEdge(edge_nodes, edge_proc_edge_mask[direction][i][4], indexes, threshold);
		}
	}
}

void Octree::ProcessIndexes(Octree ** nodes, int direction, std::vector<GLuint>& indexes, float threshold)
{
	unsigned int min_size = 100000000;
	unsigned int indices[4] = { -1,-1,-1,-1 };
	bool flip = false;
	bool sign_changed = false;

	for (int i = 0; i < 4; i++)
	{
		int edge = process_edge_mask[direction][i];
		int c1 = edge_pairs[edge][0];
		int c2 = edge_pairs[edge][1];

		int m1 = (nodes[i]->m_corners >> c1) & 1;
		int m2 = (nodes[i]->m_corners >> c2) & 1;

		if(nodes[i]->m_size < min_size)
		{
			min_size = nodes[i]->m_size;
			flip = m2 == 1;
			sign_changed = ((!m1 && m2) || (m1 && !m2));
		}

		int index = 0;
		bool skip = false;
		for (int k = 0; k < 16; k++)
		{
			int e = edge_table[nodes[i]->m_corners][k];
			if(e == -1)
			{
				index++;
				continue;
			}
			if(e == -2)
			{
				skip = true;
				break;
			}
			if(e == edge)
				break;
		}

		if(skip)
			continue;
		if(index >= nodes[i]->m_vertex_count)
			return;

		VoxelVertex* v = &nodes[i]->m_vertices[index];
		VoxelVertex* highest = v;
		while (highest->parent)
		{
			if((highest->parent->error <= threshold && highest->parent->IsManifold()))
			{
				highest = highest->parent;
				v = highest;
			}
			else
				highest = highest->parent;
		}
		indices[i] = v->index;
	}

	if(sign_changed)
	{
		if(flip)
		{
			//later flip the normal too
			if(indices[0] != -1 && indices[1] != -1 && indices[2] != -1 && indices[0] != indices[1] && indices[1] != indices[3])
			{
				indexes.push_back(indices[0]);
				indexes.push_back(indices[1]);
				indexes.push_back(indices[3]);
			}

			if(indices[0] != -1 && indices[2] != -1 && indices[3] != -1 && indices[0] != indices[2] && indices[2] != indices[3])
			{
				indexes.push_back(indices[0]);
				indexes.push_back(indices[3]);
				indexes.push_back(indices[2]);
			}
		}
		else
		{
			if(indices[0] != -1 && indices[3] != -1 && indices[1] != -1 && indices[0] != indices[1] && indices[1] != indices[3])
			{
				indexes.push_back(indices[0]);
				indexes.push_back(indices[3]);
				indexes.push_back(indices[1]);
			}

			if(indices[0] != -1 && indices[2] != -1 && indices[3] != -1 && indices[0] != indices[2] && indices[2] != indices[3])
			{
				indexes.push_back(indices[0]);
				indexes.push_back(indices[2]);
				indexes.push_back(indices[3]);
			}
		}
	}
}

void Octree::ClusterCellBase(float error)
{
	if(m_flag & OCTREE_LEAF)
		return;

	for (int i = 0; i < 8; i++)
	{
		if(!m_children[i])
			continue;
		m_children[i]->ClusterCell(error);
	}
}

void Octree::ClusterCell(float error)
{
	if(m_flag & OCTREE_LEAF)
		return;

	for (int i = 0; i < 8; i++)
	{
		if(~m_childMask & 1 << i || m_children[i]->m_flag & OCTREE_LEAF)
			continue;
		m_children[i]->ClusterCell(error);
	}

	int surface_index = 0;
	std::vector<VoxelVertex*> collected_vertices;
	std::vector<VoxelVertex> new_vertices;

	/*
	* Find all the surfaces inside the m_children that cross the 6 Euclidean edges and the vertices that connect to them
	*/
	Octree* face_nodes[2];
	Octree* edge_nodes[4];
	for (int i = 0; i < 12; i++)
	{
		int c1 = edge_pairs[i][0];
		int c2 = edge_pairs[i][1];

		face_nodes[0] = m_children[c1];
		face_nodes[1] = m_children[c2];

		ClusterFace(face_nodes, edge_pairs[i][2], surface_index, collected_vertices);
	}


	for (int i = 0; i < 6; i++)
	{
		edge_nodes[0] = m_children[cell_proc_edge_mask[i][0]];
		edge_nodes[1] = m_children[cell_proc_edge_mask[i][1]];
		edge_nodes[2] = m_children[cell_proc_edge_mask[i][2]];
		edge_nodes[3] = m_children[cell_proc_edge_mask[i][3]];

		ClusterEdge(edge_nodes, cell_proc_edge_mask[i][4], surface_index, collected_vertices);
	}

	int highest_index = surface_index;
	if(highest_index == -1)
		highest_index = 0;

	for (int i = 0; i < 8; i++)
	{
		if(~m_childMask & 1 << i)
			continue;
		for (int k = 0; k < m_children[i]->m_vertex_count; k++)
		{
			VoxelVertex* v = &m_children[i]->m_vertices[k];
			if(!v)
				continue;
			if(v->surface_index == -1)
			{
				v->surface_index = highest_index++;
				collected_vertices.push_back(v);
			}
		}
	}

	if(collected_vertices.size() > 0)
	{
		std::set<int> surface_set;
		for (auto& v : collected_vertices)
		{
			surface_set.insert(v->surface_index);
		}
		if(surface_set.size() == 0)
			return;

		if(this->m_vertices)
			delete[] this->m_vertices;

		this->m_vertices = new VoxelVertex[surface_set.size()];
		this->m_vertex_count = surface_set.size();

		for (int i = 0; i <= highest_index; i++)
		{
			QEFSolver qef;
			glm::vec3 normal(0, 0, 0);
			glm::vec3 positions(0, 0, 0);
			int count = 0;
			int edges[12] = { 0 };
			int euler = 0;
			int e = 0;

			/* manifold criterion */
			for (auto& v : collected_vertices)
			{
				if(v->surface_index == i)
				{
					//if(v->position == glm::vec3(0.0f)) 
					//	continue;

					for (int k = 0; k < 3; k++)
					{
						int edge = external_edges[v->in_cell][k];
						edges[edge] += v->eis[edge];
					}
					for (int k = 0; k < 9; k++)
					{
						int edge = internal_edges[v->in_cell][k];
 						e += v->eis[edge];
					}

					euler += v->euler;
					qef.add(v->qef.getData());
					normal += v->normal;
					positions += v->position;
					count++;
				}
			}

			if(count == 0)
				continue;

			bool face_prop2 = true;
			for (int f = 0; f < 6 && face_prop2; f++)
			{
				int intersections = 0;
				for (int ei = 0; ei < 4; ei++)
				{
					intersections += edges[faces[f][ei]];
				}
				if(!(intersections == 0 || intersections == 2))
					face_prop2 = false;
			}

			VoxelVertex new_vertex;
			positions /= (float)count;
			normal /= (float)count;
			normal = glm::normalize(normal);
			new_vertex.normal = normal;
			//new_vertex.normal = World::GetNormal(positions, 1);
			new_vertex.euler = euler - e / 4;
			new_vertex.in_cell = this->m_child_index;
			memcpy(&new_vertex.qef, &qef, sizeof(qef));
			memcpy(&new_vertex.eis, &edges, sizeof(edges));

			Vec3 p_out;

			qef.solve(p_out, 1e-6f, 4, 1e-6f);
			//new_vertex.position = glm::vec3(p_out.x, p_out.y, p_out.z);
			new_vertex.error = qef.getError();
			if(new_vertex.error <= error)
				new_vertex.flags |= VoxelVertexFlags::COLLAPSIBLE;
			if(face_prop2)
				new_vertex.flags |= VoxelVertexFlags::FACEPROP2;
			new_vertex.flags |= 8;

			new_vertices.push_back(new_vertex);

			for (auto& v : collected_vertices)
			{
				if(v->surface_index == i)
				{
					if(v != &new_vertex)
						v->parent = &this->m_vertices[new_vertices.size() - 1];
					else
						v->parent = 0;
				}
			}
		}
	}
	else
		return;

	for (auto& v : collected_vertices)
	{
		v->surface_index = -1;
	}

	memcpy(this->m_vertices, &new_vertices[0], sizeof(VoxelVertex) * new_vertices.size());
}

void Octree::ClusterFace(Octree ** nodes, int direction, int & surface_index, std::vector<VoxelVertex*>& collected_vertices)
{
	if(!nodes[0] || !nodes[1])
		return;

	if(~nodes[0]->m_flag & OCTREE_LEAF || ~nodes[1]->m_flag & OCTREE_LEAF)
	{
		for (int i = 0; i < 4; i++)
		{
			Octree* face_nodes[2] = { 0, 0 };
			for (int j = 0; j < 2; j++)
			{
				if(!nodes[j])
					continue;
				if(nodes[j]->m_flag & OCTREE_LEAF)
					face_nodes[j] = nodes[j];
				else
					face_nodes[j] = nodes[j]->m_children[face_proc_face_mask[direction][i][j]];
			}

			ClusterFace(face_nodes, face_proc_face_mask[direction][i][2], surface_index, collected_vertices);
		}
	}

	const int orders[2][4] =
	{
		{ 0, 0, 1, 1 },
		{ 0, 1, 0, 1 },
	};

	for (int i = 0; i < 4; i++)
	{
		Octree* edge_nodes[4] = { 0, 0, 0, 0 };
		for (int j = 0; j < 4; j++)
		{
			if(!nodes[orders[face_proc_edge_mask[direction][i][0]][j]])
				continue;
			if(nodes[orders[face_proc_edge_mask[direction][i][0]][j]]->m_flag & OCTREE_LEAF)
				edge_nodes[j] = nodes[orders[face_proc_edge_mask[direction][i][0]][j]];
			else
				edge_nodes[j] = nodes[orders[face_proc_edge_mask[direction][i][0]][j]]->m_children[face_proc_edge_mask[direction][i][1 + j]];
		}

		ClusterEdge(edge_nodes, face_proc_edge_mask[direction][i][5], surface_index, collected_vertices);
	}
}

void Octree::ClusterEdge(Octree ** nodes, int direction, int & surface_index, std::vector<VoxelVertex*>& collected_vertices)
{
	if((!nodes[0] || nodes[0]->m_flag & OCTREE_LEAF) && (!nodes[1] || nodes[1]->m_flag & OCTREE_LEAF) && (!nodes[2] || nodes[2]->m_flag & OCTREE_LEAF) && (!nodes[3] || nodes[3]->m_flag & OCTREE_LEAF))
		ClusterIndexes(nodes, direction, surface_index, collected_vertices);
	else
	{
		for (int i = 0; i < 2; i++)
		{
			Octree* edge_nodes[4] = { 0, 0, 0, 0 };
			for (int j = 0; j < 4; j++)
			{
				if(!nodes[j])
					continue;
				if(nodes[j]->m_flag & OCTREE_LEAF)
					edge_nodes[j] = nodes[j];
				else
					edge_nodes[j] = nodes[j]->m_children[edge_proc_edge_mask[direction][i][j]];
			}

			ClusterEdge(edge_nodes, edge_proc_edge_mask[direction][i][4], surface_index, collected_vertices);
		}
	}
}

void Octree::ClusterIndexes(Octree ** nodes, int direction, int & max_surface_index, std::vector<VoxelVertex*>& collected_vertices)
{
	if(!nodes[0] && !nodes[1] && !nodes[2] && !nodes[3])
		return;

	VoxelVertex* vertices[4] = { 0, 0, 0, 0 };
	int v_count = 0;
	int node_count = 0;

	for (int i = 0; i < 4; i++)
	{
		if(!nodes[i])
			continue;

		int corners = nodes[i]->m_corners;
		int edge = process_edge_mask[direction][i];
		int c1 = edge_pairs[edge][0];
		int c2 = edge_pairs[edge][1];

		int m1 = (corners >> c1) & 1;
		int m2 = (corners >> c2) & 1;

		int index = 0;
		bool skip = false;
		for (int k = 0; k < 16; k++)
		{
			int e = edge_table[corners][k];
			if(e == -1)
			{
				index++;
				continue;
			}
			if(e == -2)
			{
				if(!((m1 == 0 && m2 != 0) || (m1 != 0 && m2 == 0)))
					skip = true;
				break;
			}
			if(e == edge)
				break;
		}

		if(!skip && index < nodes[i]->m_vertex_count)
		{
			vertices[i] = &nodes[i]->m_vertices[index];
			while (vertices[i]->parent)
				vertices[i] = vertices[i]->parent;
			v_count++;
		}
	}

	if(!v_count)
		return;

	int surface_index = -1;

	for (int i = 0; i < 4; i++)
	{
		VoxelVertex* v = vertices[i];
		if(!v)
			continue;
		if(v->surface_index != -1)
		{
			if(surface_index != -1 && surface_index != v->surface_index)
				AssignSurface(collected_vertices, v->surface_index, surface_index);
			else if(surface_index == -1)
				surface_index = v->surface_index;
		}
	}

	if(surface_index == -1)
		surface_index = max_surface_index++;
	for (int i = 0; i < 4; i++)
	{
		VoxelVertex* v = vertices[i];
		if(!v)
			continue;
		if(v->surface_index == -1)
			collected_vertices.push_back(v);
		v->surface_index = surface_index;
	}
}

void Octree::AssignSurface(std::vector<VoxelVertex*>& vertices, int from, int to)
{
	for (auto& v : vertices)
	{
		if(v && v->surface_index == from)
			v->surface_index = to;
	}
}
