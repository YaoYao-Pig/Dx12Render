#pragma once
#include "PCH.h"
struct Vertex { 
	XMFLOAT3 position;
	XMFLOAT4 color;
	XMFLOAT2 texcoord;
	XMFLOAT3 normal;
	XMFLOAT3 tangent;
};
class Geometry {
public:
	std::vector<Vertex> vertexs;
	std::vector<WORD> indices;
	UINT vertexSize;
	UINT indiceSize;

	std::vector<Vertex> planeVertexs;
	std::vector<uint16_t> planeIndices;
	UINT planeVertexSize = 0;
	UINT planeIndiceSize = 0;


	std::vector<Vertex> sphereVertexs;
	std::vector<uint16_t> sphereIndices;
	UINT sphereVertexSize = 0;
	UINT sphereIndiceSize = 0;

	void LoadCubeGeometry();
	void LoadPlaneGeometry();
	void LoadSphereGeometry();
	void LoadGeometry() {
		LoadCubeGeometry();
		LoadPlaneGeometry();
		LoadSphereGeometry();
	}
};