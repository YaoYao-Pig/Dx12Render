#pragma once
#include "PCH.h"
struct Vertex { 
	float position[3]; 
	float color[4];
	float texCoord[2]; 
	float normal[3]; 
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
	void LoadCubeGeometry();
	void LoadPlaneGeometry();
	void LoadGeometry() {
		LoadCubeGeometry();
		LoadPlaneGeometry();
	}
};