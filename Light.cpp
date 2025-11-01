#include "PCH.h"
#include "Light.h"

void Light::Init(float pos[3], float dir[3])
{
	for (int i = 0; i < 3; ++i) {
		this->pos[i] = pos[i];
	}
	for (int i = 0; i < 3; ++i) {
		this->dir[i] = dir[i];
	}
}
