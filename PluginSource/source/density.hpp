#pragma once

#define MATERIAL_SOLID 1
#define MATERIAL_AIR 0

class Density
{
	static float voxelSize;
	static float invVoxelSize;
	static float maxHeight;
	static float noiseScale;

	static FastNoise terrainFN;

	static FastNoiseSIMD *terrainFNSIMD;
	static FastNoiseSIMD *caveFNSIMD;

public:
	static float densityGenTime;

	enum DensityType {Terrain, Cave};
public:
	static FastNoiseSIMD * GetTerrainType();
	static void SetVoxelSize(const float & voxelSize);

	static void SetMaxVoxelHeight(const float & height);

	static void Initialize();

	static glm::vec3 FindIntersection(Density::DensityType type, const glm::vec3 &p0, const glm::vec3 &p1);
	static glm::vec3 FindIntersection2D(Density::DensityType type, const glm::vec3 & p0, const glm::vec3 & p1);
	static glm::vec3 CalculateNormals(Density::DensityType type, const glm::vec3 &pos);

	static glm::vec3 CalculateNormals2D(Density::DensityType type, const glm::vec3 & pos);

	static float GetSphere(const glm::vec3& worldPosition, const glm::vec3& origin, float radius);
	static float GetTerrainDensity(const glm::vec3& worldPosition, float noise3D = 0, float noise2D = 0);
	static float GetTerrain(const glm::vec3 & worldPosition);
	
	static float GetDensity(DensityType type, const glm::vec3 &worldPosition);
	static float GetNoise2D(DensityType type, const glm::vec3 &worldPosition);
	static float *GetDensitySet(DensityType type, const vector<glm::vec3> &positionSet);

	static void FreeSet(float * set);

	static void GenerateTerrainIndices(const glm::vec3 &chunkPos, const glm::vec3 &chunkSize, vector<int> &materialIndices);
	static void GenerateCaveIndices(const glm::vec3 & chunkPos, const glm::vec3 & chunkSize, vector<int>& materialIndices);
	static void GenerateMaterialIndices(DensityType type, const glm::vec3 & chunkPos, const glm::vec3 & chunkSize, vector<int>& materialIndices);

	static void GenerateHeightMap(const glm::vec3 & chunkPos, const glm::vec3 & chunkSize, vector<float>& heightmap);
	static bool GenerateMaterialIndices(const glm::vec3 & chunkPos, const glm::vec3 & chunkSize, vector<int>& materialIndices, vector<float> &heightMap);
	static float GetCaveNoise(glm::vec3 worldPosition);

};
