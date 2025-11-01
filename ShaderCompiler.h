#pragma once
#include<string>
class ShaderCompiler {
public:
	ShaderCompiler();
	void Init(std::vector<std::string> entryPoint) {
		this->entryPoint = entryPoint;
	}
	void LoadShader();

	void CompileShader();

	const char* GetVertexShaderCode();
	const char* GetPixelShaderCode();

	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;

	ComPtr<ID3DBlob> vertexShaderError;
	ComPtr<ID3DBlob> pixelShaderError;


private:
	std::vector<std::string> entryPoint;
	std::string vertexShaderCode = "";
	std::string pixelShaderCode = "";
	u_int vertexShaderCodeLen = 0;
	u_int pixelShaderCodeLen = 0;
};