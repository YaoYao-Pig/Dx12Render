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
	void LoadGeometry();
};