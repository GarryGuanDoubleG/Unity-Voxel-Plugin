#pragma once

enum VoxelVertexFlags
{
	NONE = 0,
	COLLAPSIBLE = 1,
	FACEPROP2 = 2
};

class VoxelVertex
{
public:
	QEFSolver qef;
	VoxelVertex* parent;
	glm::vec3 position;
	glm::vec3 normal;
	int textureID;
	int flip;

	unsigned int index;
	int surface_index;
	unsigned char flags;
	float error;
	int euler;
	int eis[12];
	unsigned char in_cell;

public:
	VoxelVertex();
	~VoxelVertex();

	inline bool IsCollapsible() { return (flags & VoxelVertexFlags::COLLAPSIBLE) != 0; }
	inline bool IsManifold() { return euler == 1 && (flags & VoxelVertexFlags::FACEPROP2) != 0; }
};

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 normal;
};