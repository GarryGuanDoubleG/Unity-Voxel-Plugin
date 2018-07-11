#include "VoxelPlugin.hpp"


VoxelVertex::VoxelVertex() : parent(0), index(-1), surface_index(-1), flags(0), position(0), normal(0), error(0), euler(0), in_cell(0)
{
	memset(eis, 0, sizeof(int) * 12);
}

VoxelVertex::~VoxelVertex()
{
}
