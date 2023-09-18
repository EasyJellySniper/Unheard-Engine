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
		std::vector<XMFLOAT3> Positions(24);
		std::vector<XMFLOAT2> UV0(24);
		std::vector<XMFLOAT3> Normal(24);
		std::vector<XMFLOAT4> Tangent(24);

		// -z face
		Positions[0] = XMFLOAT3(-Pos, Pos, -Pos);
		Positions[1] = XMFLOAT3(Pos, Pos, -Pos);
		Positions[2] = XMFLOAT3(-Pos, -Pos, -Pos);
		Positions[3] = XMFLOAT3(Pos, -Pos, -Pos);
		UV0[0] = XMFLOAT2(1, 1);
		UV0[1] = XMFLOAT2(0, 1);
		UV0[2] = XMFLOAT2(0, 0);
		UV0[3] = XMFLOAT2(1, 0);
		Normal[0] = XMFLOAT3(0, 0, -1);
		Normal[1] = XMFLOAT3(0, 0, -1);
		Normal[2] = XMFLOAT3(0, 0, -1);
		Normal[3] = XMFLOAT3(0, 0, -1);
		Tangent[0] = XMFLOAT4(1, 0, 0, 1);
		Tangent[1] = XMFLOAT4(1, 0, 0, 1);
		Tangent[2] = XMFLOAT4(1, 0, 0, 1);
		Tangent[3] = XMFLOAT4(1, 0, 0, 1);

		// +z face
		Positions[4] = XMFLOAT3(Pos, Pos, Pos);
		Positions[5] = XMFLOAT3(-Pos, Pos, Pos);
		Positions[6] = XMFLOAT3(Pos, -Pos, Pos);
		Positions[7] = XMFLOAT3(-Pos, -Pos, Pos);
		UV0[4] = XMFLOAT2(0, 1);
		UV0[5] = XMFLOAT2(0, 0);
		UV0[6] = XMFLOAT2(1, 0);
		UV0[7] = XMFLOAT2(1, 1);
		Normal[4] = XMFLOAT3(0, 0, 1);
		Normal[5] = XMFLOAT3(0, 0, 1);
		Normal[6] = XMFLOAT3(0, 0, 1);
		Normal[7] = XMFLOAT3(0, 0, 1);
		Tangent[4] = XMFLOAT4(-1, 0, 0, 1);
		Tangent[5] = XMFLOAT4(-1, 0, 0, 1);
		Tangent[6] = XMFLOAT4(-1, 0, 0, 1);
		Tangent[7] = XMFLOAT4(-1, 0, 0, 1);

		// -y face
		Positions[8] = XMFLOAT3(Pos, -Pos, Pos);
		Positions[9] = XMFLOAT3(-Pos, -Pos, Pos);
		Positions[10] = XMFLOAT3(Pos, -Pos, -Pos);
		Positions[11] = XMFLOAT3(-Pos, -Pos, -Pos);
		UV0[8] = XMFLOAT2(1, 1);
		UV0[9] = XMFLOAT2(0, 1);
		UV0[10] = XMFLOAT2(0, 0);
		UV0[11] = XMFLOAT2(1, 0);
		Normal[8] = XMFLOAT3(0, -1, 0);
		Normal[9] = XMFLOAT3(0, -1, 0);
		Normal[10] = XMFLOAT3(0, -1, 0);
		Normal[11] = XMFLOAT3(0, -1, 0);
		Tangent[8] = XMFLOAT4(-1, 0, 0, 1);
		Tangent[9] = XMFLOAT4(-1, 0, 0, 1);
		Tangent[10] = XMFLOAT4(-1, 0, 0, 1);
		Tangent[11] = XMFLOAT4(-1, 0, 0, 1);

		// +y face
		Positions[12] = XMFLOAT3(-Pos, Pos, Pos);
		Positions[13] = XMFLOAT3(Pos, Pos, Pos);
		Positions[14] = XMFLOAT3(-Pos, Pos, -Pos);
		Positions[15] = XMFLOAT3(Pos, Pos, -Pos);
		UV0[12] = XMFLOAT2(0, 1);
		UV0[13] = XMFLOAT2(0, 0);
		UV0[14] = XMFLOAT2(1, 0);
		UV0[15] = XMFLOAT2(1, 1);
		Normal[12] = XMFLOAT3(0, 1, 0);
		Normal[13] = XMFLOAT3(0, 1, 0);
		Normal[14] = XMFLOAT3(0, 1, 0);
		Normal[15] = XMFLOAT3(0, 1, 0);
		Tangent[12] = XMFLOAT4(1, 0, 0, 1);
		Tangent[13] = XMFLOAT4(1, 0, 0, 1);
		Tangent[14] = XMFLOAT4(1, 0, 0, 1);
		Tangent[15] = XMFLOAT4(1, 0, 0, 1);

		// -x face
		Positions[16] = XMFLOAT3(-Pos, Pos, Pos);
		Positions[17] = XMFLOAT3(-Pos, Pos, -Pos);
		Positions[18] = XMFLOAT3(-Pos, -Pos, Pos);
		Positions[19] = XMFLOAT3(-Pos, -Pos, -Pos);
		UV0[16] = XMFLOAT2(0, 1);
		UV0[17] = XMFLOAT2(0, 0);
		UV0[18] = XMFLOAT2(1, 0);
		UV0[19] = XMFLOAT2(1, 1);
		Normal[16] = XMFLOAT3(-1, 0, 0);
		Normal[17] = XMFLOAT3(-1, 0, 0);
		Normal[18] = XMFLOAT3(-1, 0, 0);
		Normal[19] = XMFLOAT3(-1, 0, 0);
		Tangent[16] = XMFLOAT4(0, 0, -1, 1);
		Tangent[17] = XMFLOAT4(0, 0, -1, 1);
		Tangent[18] = XMFLOAT4(0, 0, -1, 1);
		Tangent[19] = XMFLOAT4(0, 0, -1, 1);

		// +x face
		Positions[20] = XMFLOAT3(Pos, Pos, -Pos);
		Positions[21] = XMFLOAT3(Pos, Pos, Pos);
		Positions[22] = XMFLOAT3(Pos, -Pos, -Pos);
		Positions[23] = XMFLOAT3(Pos, -Pos, Pos);
		UV0[20] = XMFLOAT2(0, 1);
		UV0[21] = XMFLOAT2(0, 0);
		UV0[22] = XMFLOAT2(1, 0);
		UV0[23] = XMFLOAT2(1, 1);
		Normal[20] = XMFLOAT3(1, 0, 0);
		Normal[21] = XMFLOAT3(1, 0, 0);
		Normal[22] = XMFLOAT3(1, 0, 0);
		Normal[23] = XMFLOAT3(1, 0, 0);
		Tangent[20] = XMFLOAT4(0, 0, 1, 1);
		Tangent[21] = XMFLOAT4(0, 0, 1, 1);
		Tangent[22] = XMFLOAT4(0, 0, 1, 1);
		Tangent[23] = XMFLOAT4(0, 0, 1, 1);

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