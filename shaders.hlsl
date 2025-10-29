// Constant buffer, must match the C++ side struct and register (b0)
cbuffer SceneConstantBuffer : register(b0)
{
    float4x4 mvp;
};

// Vertex Shader input structure
// Must match the Input Layout in C++
struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

// Pixel Shader input structure
// (This is the output of the Vertex Shader)
struct PSInput
{
    float4 position : SV_Position; // SV_Position is a system value for clip-space position
    float4 color : COLOR;
};

// Main function for the Vertex Shader
PSInput VSMain(VSInput input)
{
    PSInput output;

    // Your C++ code sends a column-major matrix.
    // For column-major matrices, the multiplication order is mul(matrix, vector).
    // This is the CORRECT way that matches your C++ code.
    output.position = mul(mvp, float4(input.position, 1.0f));
    
    // Pass the color through to the pixel shader
    output.color = input.color;

    return output;
}


// Main function for the Pixel Shader
float4 PSMain(PSInput input) : SV_Target
{
    // Just return the interpolated color from the vertex
    return input.color;
}