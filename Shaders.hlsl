struct Light
{
    float4 Position;
    float4 Color;
};

cbuffer PSConstantBuffer : register(b0)
{
    matrix World;
    matrix Model;
    
    Light light;
    
    float4 Eye;
}
struct Layout
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 fragPos : FRAG;
};

SamplerState sample : register(s0);
Texture2D tex : register(t0);

Layout VSMain(float3 pos : POSITION, float2 texCoord : TEXCOORD, float3 normal : NORMAL)
{
    Layout layout;
    layout.position = mul(float4(pos, 1.0f), World);
    layout.texCoord = texCoord;
    layout.normal = mul(normal, (float3x3)Model);
    layout.fragPos = mul(float4(pos, 1.0f), Model).xyz;
        
    return layout;
}

float4 PSMain(Layout layout) : SV_TARGET
{
    float4 diffuseAlbedo = float4(0.2f, 0.2f, 0.2f, 1.0f);
    
    float3 totalLight = float3(0.0f, 0.0f, 0.0f);
    
    float4 pixelColor = tex.Sample(sample, layout.texCoord);
    
    float3 attenuation = length(light.Position.xyz - layout.fragPos);
    
    // Diffuse
    float3 diffuse = float3(0.0f, 0.0f, 0.0f), specular = float3(0.0f, 0.0f, 0.0f);
    float3 norm = normalize(layout.normal);
    float3 lightDir = normalize(light.Position.xyz - layout.fragPos);
    float3 diff = max(dot(norm, lightDir), 0.0f) / (attenuation);
    
    diffuse = (diff * light.Color.xyz);
    
    // Specular
    float3 viewDir = normalize(Eye.xyz - layout.fragPos);
    float3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0f), 32) / (attenuation);
    specular = (1.0f * spec * light.Color.xyz);
    

    
    totalLight = diffuseAlbedo.xyz + diffuse + specular;
    
   
    return pixelColor * float4(totalLight, 1.0f);
}