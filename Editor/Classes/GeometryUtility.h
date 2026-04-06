#pragma once

// utility header for geometry
#if WITH_EDITOR
namespace UHGeometryHelper
{
	inline UHMesh CreateCubeMesh()
	{
		UHMesh CubeMesh("UHMesh_Cube");

		// classic 24 points 12 triangles (36 index) unit cube, zero at the center
		// so it works well for both location and normal/tangent/uv data
		float Pos = 0.5f;
		std::vector<UHVector3> Positions(24);
		std::vector<UHVector2> UV0(24);
		std::vector<UHVector3> Normal(24);
		std::vector<UHVector4> Tangent(24);

		// -z face
		Positions[0] = UHVector3(-Pos, Pos, -Pos);
		Positions[1] = UHVector3(Pos, Pos, -Pos);
		Positions[2] = UHVector3(-Pos, -Pos, -Pos);
		Positions[3] = UHVector3(Pos, -Pos, -Pos);
		UV0[0] = UHVector2(1, 1);
		UV0[1] = UHVector2(0, 1);
		UV0[2] = UHVector2(0, 0);
		UV0[3] = UHVector2(1, 0);
		Normal[0] = UHVector3(0, 0, -1);
		Normal[1] = UHVector3(0, 0, -1);
		Normal[2] = UHVector3(0, 0, -1);
		Normal[3] = UHVector3(0, 0, -1);
		Tangent[0] = UHVector4(1, 0, 0, 1);
		Tangent[1] = UHVector4(1, 0, 0, 1);
		Tangent[2] = UHVector4(1, 0, 0, 1);
		Tangent[3] = UHVector4(1, 0, 0, 1);

		// +z face
		Positions[4] = UHVector3(Pos, Pos, Pos);
		Positions[5] = UHVector3(-Pos, Pos, Pos);
		Positions[6] = UHVector3(Pos, -Pos, Pos);
		Positions[7] = UHVector3(-Pos, -Pos, Pos);
		UV0[4] = UHVector2(0, 1);
		UV0[5] = UHVector2(0, 0);
		UV0[6] = UHVector2(1, 0);
		UV0[7] = UHVector2(1, 1);
		Normal[4] = UHVector3(0, 0, 1);
		Normal[5] = UHVector3(0, 0, 1);
		Normal[6] = UHVector3(0, 0, 1);
		Normal[7] = UHVector3(0, 0, 1);
		Tangent[4] = UHVector4(-1, 0, 0, 1);
		Tangent[5] = UHVector4(-1, 0, 0, 1);
		Tangent[6] = UHVector4(-1, 0, 0, 1);
		Tangent[7] = UHVector4(-1, 0, 0, 1);

		// -y face
		Positions[8] = UHVector3(Pos, -Pos, Pos);
		Positions[9] = UHVector3(-Pos, -Pos, Pos);
		Positions[10] = UHVector3(Pos, -Pos, -Pos);
		Positions[11] = UHVector3(-Pos, -Pos, -Pos);
		UV0[8] = UHVector2(1, 1);
		UV0[9] = UHVector2(0, 1);
		UV0[10] = UHVector2(0, 0);
		UV0[11] = UHVector2(1, 0);
		Normal[8] = UHVector3(0, -1, 0);
		Normal[9] = UHVector3(0, -1, 0);
		Normal[10] = UHVector3(0, -1, 0);
		Normal[11] = UHVector3(0, -1, 0);
		Tangent[8] = UHVector4(-1, 0, 0, 1);
		Tangent[9] = UHVector4(-1, 0, 0, 1);
		Tangent[10] = UHVector4(-1, 0, 0, 1);
		Tangent[11] = UHVector4(-1, 0, 0, 1);

		// +y face
		Positions[12] = UHVector3(-Pos, Pos, Pos);
		Positions[13] = UHVector3(Pos, Pos, Pos);
		Positions[14] = UHVector3(-Pos, Pos, -Pos);
		Positions[15] = UHVector3(Pos, Pos, -Pos);
		UV0[12] = UHVector2(0, 1);
		UV0[13] = UHVector2(0, 0);
		UV0[14] = UHVector2(1, 0);
		UV0[15] = UHVector2(1, 1);
		Normal[12] = UHVector3(0, 1, 0);
		Normal[13] = UHVector3(0, 1, 0);
		Normal[14] = UHVector3(0, 1, 0);
		Normal[15] = UHVector3(0, 1, 0);
		Tangent[12] = UHVector4(1, 0, 0, 1);
		Tangent[13] = UHVector4(1, 0, 0, 1);
		Tangent[14] = UHVector4(1, 0, 0, 1);
		Tangent[15] = UHVector4(1, 0, 0, 1);

		// -x face
		Positions[16] = UHVector3(-Pos, Pos, Pos);
		Positions[17] = UHVector3(-Pos, Pos, -Pos);
		Positions[18] = UHVector3(-Pos, -Pos, Pos);
		Positions[19] = UHVector3(-Pos, -Pos, -Pos);
		UV0[16] = UHVector2(0, 1);
		UV0[17] = UHVector2(0, 0);
		UV0[18] = UHVector2(1, 0);
		UV0[19] = UHVector2(1, 1);
		Normal[16] = UHVector3(-1, 0, 0);
		Normal[17] = UHVector3(-1, 0, 0);
		Normal[18] = UHVector3(-1, 0, 0);
		Normal[19] = UHVector3(-1, 0, 0);
		Tangent[16] = UHVector4(0, 0, -1, 1);
		Tangent[17] = UHVector4(0, 0, -1, 1);
		Tangent[18] = UHVector4(0, 0, -1, 1);
		Tangent[19] = UHVector4(0, 0, -1, 1);

		// +x face
		Positions[20] = UHVector3(Pos, Pos, -Pos);
		Positions[21] = UHVector3(Pos, Pos, Pos);
		Positions[22] = UHVector3(Pos, -Pos, -Pos);
		Positions[23] = UHVector3(Pos, -Pos, Pos);
		UV0[20] = UHVector2(0, 1);
		UV0[21] = UHVector2(0, 0);
		UV0[22] = UHVector2(1, 0);
		UV0[23] = UHVector2(1, 1);
		Normal[20] = UHVector3(1, 0, 0);
		Normal[21] = UHVector3(1, 0, 0);
		Normal[22] = UHVector3(1, 0, 0);
		Normal[23] = UHVector3(1, 0, 0);
		Tangent[20] = UHVector4(0, 0, 1, 1);
		Tangent[21] = UHVector4(0, 0, 1, 1);
		Tangent[22] = UHVector4(0, 0, 1, 1);
		Tangent[23] = UHVector4(0, 0, 1, 1);

		std::vector<uint32_t> Indices =
		{
			// -z face
			0,1,2,2,1,3,
			// +z face
			4,5,6,6,5,7,
			// -y face
			8,9,10,10,9,11,
			// +y face
			12,13,14,14,13,15,
			// -x face
			16,17,18,18,17,19,
			// +x face
			20,21,22,22,21,23
		};

		CubeMesh.SetPositionData(Positions);
		CubeMesh.SetUV0Data(UV0);
		CubeMesh.SetNormalData(Normal);
		CubeMesh.SetTangentData(Tangent);
		CubeMesh.SetIndicesData(Indices);
		return CubeMesh;
	}
}
#endif