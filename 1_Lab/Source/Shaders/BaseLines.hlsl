cbuffer ConstantBuffer : register(b0)
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4 ObjectColor;
    float2 UVOffset;
    float HasTexture;
    float padding;
    float4 CameraPosition;
    float4 LightPositions[8];
    float4 LightColors[8];
    float4 LightParams[8];
    float4 LightMeta;
};

struct VS_IN
{
    float4 pos : POSITION;
    float4 col : COLOR;
    float2 uv : TEXCOORD;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
    float3 worldPos : TEXCOORD1;
    float3 normal : TEXCOORD2;
};

Texture2D objTexture : register(t0);
SamplerState objSampler : register(s0);

PS_IN VSMain(VS_IN input)
{
    PS_IN output = (PS_IN)0;
    
    float4 worldPos = mul(input.pos, worldMatrix);
    float4 viewPos = mul(worldPos, viewMatrix);
    float4 projectionPos = mul(viewPos, projectionMatrix);
    float3 objectCenter = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), worldMatrix).xyz;
    
    output.pos = projectionPos;
    output.uv = input.col.xy;
    output.color = float4(1,1,1,1) * ObjectColor;
    output.worldPos = worldPos.xyz;
    output.normal = normalize(worldPos.xyz - objectCenter);
    
    return output;
}

float4 PSMain(PS_IN input) : SV_Target
{
    float4 texColor = objTexture.Sample(objSampler, input.uv);

    float3 normal = normalize(input.normal);
    float3 viewDir = normalize(CameraPosition.xyz - input.worldPos);

    float ambient = LightMeta.y;
    float shininess = LightMeta.z;
    int lightCount = (int)LightMeta.x;

    float3 diffuseAccum = float3(0.0f, 0.0f, 0.0f);
    float3 specularAccum = float3(0.0f, 0.0f, 0.0f);

    [loop]
    for (int i = 0; i < 8; ++i)
    {
        if (i >= lightCount)
        {
            break;
        }

        if (LightParams[i].z < 0.5f)
        {
            continue;
        }

        float3 toLight = LightPositions[i].xyz - input.worldPos;
        float dist = length(toLight);
        float radius = max(LightParams[i].y, 0.001f);
        float3 lightDir = toLight / max(dist, 0.0001f);
        float attenuation = saturate(1.0f - dist / radius);
        attenuation *= attenuation;

        float diffuse = max(dot(normal, lightDir), 0.0f);
        float3 reflectDir = reflect(-lightDir, normal);
        float specular = pow(max(dot(viewDir, reflectDir), 0.0f), shininess);
        float3 lightContribution = LightColors[i].xyz * LightParams[i].x * attenuation;

        diffuseAccum += diffuse * lightContribution;
        specularAccum += specular * lightContribution;
    }

    float4 baseColor = texColor * input.color;
    float3 lit = baseColor.rgb * (ambient + diffuseAccum) + 0.35f * specularAccum;
    return float4(saturate(lit), baseColor.a);
}
