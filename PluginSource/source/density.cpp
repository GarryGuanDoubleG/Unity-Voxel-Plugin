#include "VoxelPlugin.hpp"
float Density::voxelSize = 32.0f;
float Density::invVoxelSize = 1.0f;
float Density::maxHeight = 32.0f; //max number of voxels high
float Density::noiseScale;
float Density::densityGenTime;

FastNoise Density::terrainFN;
FastNoiseSIMD *Density::terrainFNSIMD;
FastNoiseSIMD *Density::caveFNSIMD;

omp_lock_t g_thread_lock;

void Density::SetVoxelSize(const float & voxelSize)
{
	//size in world space
	Density::voxelSize = voxelSize;
	Density::invVoxelSize = 1.0f / voxelSize;
}

void Density::SetMaxVoxelHeight(const float & height)
{
	//height is in voxel space
	Density::maxHeight = height;
}

void Density::Initialize()
{
	omp_init_lock(&g_thread_lock);
	noiseScale = 0.3f;

	terrainFN.SetFractalOctaves(10);
	terrainFN.SetFrequency(0.01f);
	terrainFN.SetFractalLacunarity(2.0f);
	terrainFN.SetFractalType(terrainFN.FBM);
	terrainFN.SetFractalGain(0.5);
	terrainFN.SetNoiseType(terrainFN.SimplexFractal);

	terrainFNSIMD = FastNoiseSIMD::NewFastNoiseSIMD();
	terrainFNSIMD->SetFractalOctaves(8);
	terrainFNSIMD->SetFrequency(0.04f);
	terrainFNSIMD->SetFractalLacunarity(2.0f);
	terrainFNSIMD->SetFractalType(terrainFNSIMD->FBM);
	terrainFNSIMD->SetFractalGain(0.5);
	terrainFNSIMD->SetNoiseType(terrainFNSIMD->SimplexFractal);

	caveFNSIMD = FastNoiseSIMD::NewFastNoiseSIMD();
	caveFNSIMD->SetFrequency(0.03);
	caveFNSIMD->SetSeed(12345);
	caveFNSIMD->SetNoiseType(caveFNSIMD->Cellular);
	caveFNSIMD->SetCellularReturnType(caveFNSIMD->Distance2Cave);
	caveFNSIMD->SetCellularDistanceFunction(caveFNSIMD->Euclidean);
	caveFNSIMD->SetCellularJitter(0.4f);
	caveFNSIMD->SetPerturbAmp(caveFNSIMD->GradientFractal);
	caveFNSIMD->SetPerturbFrequency(0.6f);
}

glm::vec3 Density::FindIntersection(Density::DensityType type, const glm::vec3 &p0, const glm::vec3 &p1)
{
	float minValue = 100000.f;
	float t = 0.f;
	float currentT = 0.f;
	const int steps = 8;
	const float increment = 1.f / (float)steps;

	vector<glm::vec3> positions(steps + 1);
	for (int i = 0; i <= steps; i++)
	{
		positions[i] = p0 + ((p1 - p0) * currentT);
		currentT += increment;
	}

	float *noiseSet = Density::GetDensitySet(type, positions);
	for (int i = 0; i <= steps; i++)
	{
		float density = glm::abs(noiseSet[i]);
		if(density < minValue)
		{
			minValue = density;
			t = increment * i;
		}
	}

	Density::FreeSet(noiseSet);

	return p0 + ((p1 - p0) * t);
}

glm::vec3 Density::FindIntersection2D(Density::DensityType type, const glm::vec3 &p0, const glm::vec3 &p1)
{
	float minValue = 100000.f;
	float t = 0.f;
	float currentT = 0.f;
	const int steps = 8;
	const float increment = 1.f / (float)steps;

	vector<glm::vec3> positions(steps + 1);
	for (int i = 0; i <= steps; i++)
	{
		positions[i] = p0 + ((p1 - p0) * currentT);
		currentT += increment;
	}
	for (int i = 0; i <= steps; i++)
	{
		float density = glm::abs(Density::GetNoise2D(type, positions[i]));
		if (density < minValue)
		{
			minValue = density;
			t = increment * i;
		}
	}

	return p0 + ((p1 - p0) * t);
}

glm::vec3 Density::CalculateNormals(Density::DensityType type, const glm::vec3 &pos)
{
	const float H = 0.1f;

	vector<glm::vec3> positions(6);

	//finite difference method to get partial derivatives
	positions[0] = pos + glm::vec3(H, 0.f, 0.f);
	positions[1] = pos + glm::vec3(0.f, H, 0.f);
	positions[2] = pos + glm::vec3(0.f, 0.f, H);

	positions[3] = pos - glm::vec3(H, 0.f, 0.f);
	positions[4] = pos - glm::vec3(0.f, H, 0.f);
	positions[5] = pos - glm::vec3(0.f, 0.f, H);

	float *noiseSet = Density::GetDensitySet(type, positions);

	glm::vec3 d1(noiseSet[0], noiseSet[1], noiseSet[2]);
	glm::vec3 d2(noiseSet[3], noiseSet[4], noiseSet[5]);

	Density::FreeSet(noiseSet);
	return glm::normalize(d1 - d2);
}

glm::vec3 Density::CalculateNormals2D(Density::DensityType type, const glm::vec3 &pos)
{
	const float H = 0.1f;

	if (pos.y <= 1.0f)
	{
		return glm::vec3(0.0f, 1.0f, 0.0f);
	}

	//finite difference method to get partial derivatives
	const float dx = GetNoise2D(type, pos + glm::vec3(H, 0.f, 0.f)) - GetNoise2D(type, pos - glm::vec3(H, 0.f, 0.f));
	const float dy = GetNoise2D(type, pos + glm::vec3(0.f, H, 0.f)) - GetNoise2D(type, pos - glm::vec3(0.f, H, 0.f));
	const float dz = GetNoise2D(type, pos + glm::vec3(0.f, 0.f, H)) - GetNoise2D(type, pos - glm::vec3(0.f, 0.f, H));

	return glm::normalize(glm::vec3(dx, dy, dz));
}

float Density::GetSphere(const glm::vec3& worldPosition, const glm::vec3& origin, float radius)
{
	return glm::length(worldPosition - origin) - radius;
}

float Density::GetTerrainDensity(const glm::vec3& worldPosition, float noise3D, float noise2D)
{
	float terrain = 0.0f;
	
	if(worldPosition.y <= 1)
		terrain = worldPosition.y - 1.f;
	else if(worldPosition.y > (noise2D + noise3D) * voxelSize * maxHeight)
		terrain = worldPosition.y - noise2D * voxelSize * maxHeight;
	else if(worldPosition.y > 1.0f)
		terrain = worldPosition.y - (noise2D + noise3D) * maxHeight * voxelSize;

	return terrain;
}

float Density::GetTerrain(const glm::vec3 & worldPosition)
{
	float noise3D, noise2D;
	float noise = 0.0f;

	glm::vec3 voxelPosition = worldPosition * invVoxelSize;

	noise3D = terrainFN.GetSimplexFractal(voxelPosition.x, voxelPosition.y, voxelPosition.z);
	noise2D = noise3D * .7989;
	//noise2D = terrainFN.GetSimplex(voxelPosition.x, voxelPosition.z);
	noise = GetTerrainDensity(worldPosition, noise3D, noise2D);

	return noise;
}

float Density::GetDensity(DensityType type, const glm::vec3 & worldPosition)
{
	switch(type)
	{
	case Terrain:
		return GetTerrain(worldPosition);
		break;
	case Cave:
		return GetCaveNoise(worldPosition);
		break;
	default:
		break;
	}
}

float Density::GetNoise2D(DensityType type, const glm::vec3 & worldPosition)
{
	glm::vec3 voxelPos = worldPosition * invVoxelSize * noiseScale;
	float height = terrainFN.GetSimplexFractal(voxelPos.x, voxelPos.z) * maxHeight * voxelSize; //convert to world Position

	return worldPosition.y - height;
}

float *Density::GetDensitySet(DensityType type, const vector<glm::vec3> & positions)
{

	float *set = FastNoiseSIMD::GetEmptySet(positions.size());
	FastNoiseVectorSet positionSet(positions.size());

	for (int i = 0; i < positions.size(); i++)
	{
		positionSet.xSet[i] = positions[i].x * invVoxelSize;
		positionSet.ySet[i] = positions[i].y * invVoxelSize;
		positionSet.zSet[i] = positions[i].z * invVoxelSize;
	}

	switch (type)
	{
	case Terrain:
	{
		terrainFNSIMD->FillNoiseSet(set, &positionSet);
		for (int i = 0; i < positions.size(); i++)
		{
			set[i] = GetTerrainDensity(positions[i], set[i], set[i] * .7989);
		}
		break;
	}
	case Cave:
		caveFNSIMD->FillNoiseSet(set, &positionSet);
		break;
	default:
		break;
	}

	positionSet.Free();

	return set;
}

void Density::FreeSet(float *set)
{
	FastNoiseSIMD::FreeNoiseSet(set);
}

float Density::GetCaveNoise(glm::vec3 worldPosition)
{
	glm::vec3 voxelPosition = worldPosition * invVoxelSize;

	float noise = 0.0f;

	float *noiseSet = caveFNSIMD->GetNoiseSet(voxelPosition.x, voxelPosition.y, voxelPosition.z, 1, 1, 1, 1.0f);
	noise = noiseSet[0];
	FastNoiseSIMD::FreeNoiseSet(noiseSet);

	return noise;
}

void Density::GenerateTerrainIndices(const glm::vec3 &chunkPos, const glm::vec3 &chunkSize, vector<int> &materialIndices)
{
	materialIndices.resize((chunkSize.x + 1) *  (chunkSize.y + 1) * (chunkSize.z + 1), -1);
	
	glm::vec3 voxelPos = glm::ivec3(chunkPos * invVoxelSize);
	glm::vec3 setSize = chunkSize + glm::vec3(1.0f);
	
	float *noiseSet = terrainFNSIMD->GetNoiseSet(voxelPos.x, voxelPos.y, voxelPos.z, setSize.x, setSize.y, setSize.z);
	
	for (int x = 0; x <= chunkSize.x; x++)
	{
		for (int y = 0; y <= chunkSize.y; y++)
		{
			for (int z = 0; z <= chunkSize.z; z++)
			{
				int index = GETINDEXCHUNK(glm::ivec3(chunkSize + glm::vec3(1.0f)), x, y, z);
				glm::vec3 worldPosition = chunkPos + glm::vec3(x, y, z) * voxelSize;
				
				float density = GetTerrainDensity(worldPosition, noiseSet[index], noiseSet[index] * .7989);
				materialIndices[index] = density > 0 ? MATERIAL_AIR : MATERIAL_SOLID;
			}
		}
	}

	FastNoiseSIMD::FreeNoiseSet(noiseSet);
}

void Density::GenerateCaveIndices(const glm::vec3 &chunkPos, const glm::vec3 &chunkSize, vector<int> &materialIndices)
{
	materialIndices.resize((chunkSize.x + 1) *  (chunkSize.y + 1) * (chunkSize.z + 1), -1);

	glm::vec3 voxelPos = chunkPos * invVoxelSize;
	glm::vec3 setSize = chunkSize + glm::vec3(1.0f);

	float *noiseSet = caveFNSIMD->GetNoiseSet(voxelPos.x, voxelPos.y, voxelPos.z, setSize.x, setSize.y, setSize.z);
	float caveThreshold = 0.75f;

	for (int x = 0; x <= chunkSize.x; x++)
	{
		for (int y = 0; y <= chunkSize.y; y++)
		{
			for (int z = 0; z <= chunkSize.z; z++)
			{
				int index = GETINDEXCHUNK(glm::ivec3(chunkSize + glm::vec3(1.0f)), x, y, z);				
				materialIndices[index] = noiseSet[index] > caveThreshold ? MATERIAL_AIR : MATERIAL_SOLID;
			}
		}
	}

	FastNoiseSIMD::FreeNoiseSet(noiseSet);
}

void Density::GenerateMaterialIndices(DensityType type, const glm::vec3 &chunkPos, const glm::vec3 &chunkSize, vector<int> &materialIndices)
{
	switch (type)
	{
	case Terrain:
		GenerateTerrainIndices(chunkPos, chunkSize, materialIndices);
		break;
	case Cave:
		GenerateCaveIndices(chunkPos, chunkSize, materialIndices);
		break;
	default:
		break;
	}
}

//2D heightmap noise
void Density::GenerateHeightMap(const glm::vec3 &chunkPos, const glm::vec3 &chunkSize, vector<float> &heightMap)
{
	heightMap.resize((chunkSize.x + 1) * (chunkSize.z + 1), -696969.69696969);
	glm::vec3 voxelPos = glm::ivec3(chunkPos * invVoxelSize);
	//voxelPos *= noiseScale;

	//set heightmap
	for (int x = 0; x <= chunkSize.x; x++)
	{
		for (int z = 0; z <= chunkSize.z; z++)
		{ 
			int index = GETINDEXCHUNKXZ(glm::ivec3(chunkSize + glm::vec3(1.0f)), x, z);
			float height = terrainFN.GetSimplexFractal(noiseScale * (voxelPos.x + x), noiseScale * (voxelPos.z + z));
			height = (height * maxHeight * voxelSize);//convert to world Position
			height = glm::max(height, 1.0f);
			heightMap[index] = height;
		}
	}
}

//returns false if empty
bool Density::GenerateMaterialIndices(const glm::vec3 &chunkPos, const glm::vec3 &chunkSize, vector<int> &materialIndices, vector <float> &heightMap)
{
	materialIndices.resize((chunkSize.x + 1) *  (chunkSize.y + 1) * (chunkSize.z + 1), -1);
	glm::vec3 gridSize = chunkSize + glm::vec3(1.0f);

	bool activeChunk = false;
	int material;

	for (int x = 0; x <= chunkSize.x; x++)
	{
		for (int z = 0; z <= chunkSize.z; z++)
		{
			int index = GETINDEXCHUNKXZ(gridSize, x, z);
			float height = heightMap[index];

			//set material indices
			//probably bad cache performance
			for (int y = 0; y <= chunkSize.y; y++)
			{
				float worldHeight = chunkPos.y + y * voxelSize;

				////set material for voxels
				int chunkIndex = GETINDEXCHUNK(gridSize, x, y, z);
				materialIndices[chunkIndex] = worldHeight <= height ? MATERIAL_SOLID : MATERIAL_AIR;
				
				//check if chunk has a sign change
				if (chunkIndex == 0)
				{
					material = materialIndices[chunkIndex];
				}
				else if (material != materialIndices[chunkIndex])
				{
					activeChunk = true;
				}
			}
		}
	}

	return activeChunk;
}