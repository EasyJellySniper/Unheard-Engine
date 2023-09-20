// simple fallback shader
void FallbackPS(out float4 OutColor : SV_Target0
	, out float4 OutNormal : SV_Target1
	, out float4 OutMaterial : SV_Target2
	, out float4 OutEmissive : SV_Target3
	, out float OutMipRate : SV_Target4)
{
	OutColor = 0;
	OutNormal = 0;
	OutMaterial = 0;
	OutEmissive = float4(1, 0, 1, 0);
	OutMipRate = 0;
}