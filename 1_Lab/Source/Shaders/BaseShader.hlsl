cbuffer

ConstantBuffer: 

register (b0)
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4 ObjectColor;
};

struct VS_IN
{
    float4 pos : POSITION;
    float4 col : COLOR;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;
     
    float4 worldPos = mul(input.pos, worldMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    float4 projectionPos = mul(viewPos, projectionMatrix);
     
    output.pos = projectionPos;
     
    output.col = float4{0,1,0,1};
    
    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float4 color = input.col;
    return color;
}