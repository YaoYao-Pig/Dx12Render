#pragma once
class Light {
public:
	void Init(float pos[3], float dir[3]);
	float pos[3];
	float dir[3];
};