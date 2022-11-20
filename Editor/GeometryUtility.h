#pragma once

// utility header for geometry
#if WITH_DEBUG
namespace UHGeometryHelper
{
	inline UHMesh CreateCubeMesh()
	{
		UHMesh CubeMesh("UHMesh_Cube");

		// classic 24 points 12 triangles (36 index) unit cube, zero at the center
		// so it works well for both location and normal/tangent/uv data
		float Pos = 0.5f;
		std::vector<UHMeshData> Vertices(24);

		// -z face
		Vertices[0].Position = XMFLOAT3(-Pos, Pos, -Pos);
		Vertices[1].Position = XMFLOAT3(Pos, Pos, -Pos);
		Vertices[2].Position = XMFLOAT3(-Pos, -Pos, -Pos);
		Vertices[3].Position = XMFLOAT3(Pos, -Pos, -Pos);
		Vertices[0].Normal = XMFLOAT3(0, 0, -1);
		Vertices[1].Normal = XMFLOAT3(0, 0, -1);
		Vertices[2].Normal = XMFLOAT3(0, 0, -1);
		Vertices[3].Normal = XMFLOAT3(0, 0, -1);
		Vertices[0].Tangent = XMFLOAT4(1, 0, 0, 1);
		Vertices[1].Tangent = XMFLOAT4(1, 0, 0, 1);
		Vertices[2].Tangent = XMFLOAT4(1, 0, 0, 1);
		Vertices[3].Tangent = XMFLOAT4(1, 0, 0, 1);

		// +z face
		Vertices[4].Position = XMFLOAT3(Pos, Pos, Pos);
		Vertices[5].Position = XMFLOAT3(-Pos, Pos, Pos);
		Vertices[6].Position = XMFLOAT3(Pos, -Pos, Pos);
		Vertices[7].Position = XMFLOAT3(-Pos, -Pos, Pos);
		Vertices[4].Normal = XMFLOAT3(0, 0, 1);
		Vertices[5].Normal = XMFLOAT3(0, 0, 1);
		Vertices[6].Normal = XMFLOAT3(0, 0, 1);
		Vertices[7].Normal = XMFLOAT3(0, 0, 1);
		Vertices[4].Tangent = XMFLOAT4(-1, 0, 0, 1);
		Vertices[5].Tangent = XMFLOAT4(-1, 0, 0, 1);
		Vertices[6].Tangent = XMFLOAT4(-1, 0, 0, 1);
		Vertices[7].Tangent = XMFLOAT4(-1, 0, 0, 1);

		// -y face
		Vertices[8].Position = XMFLOAT3(Pos, -Pos, Pos);
		Vertices[9].Position = XMFLOAT3(-Pos, -Pos, Pos);
		Vertices[10].Position = XMFLOAT3(Pos, -Pos, -Pos);
		Vertices[11].Position = XMFLOAT3(-Pos, -Pos, -Pos);
		Vertices[8].Normal = XMFLOAT3(0, -1, 0);
		Vertices[9].Normal = XMFLOAT3(0, -1, 0);
		Vertices[10].Normal = XMFLOAT3(0, -1, 0);
		Vertices[11].Normal = XMFLOAT3(0, -1, 0);
		Vertices[8].Tangent = XMFLOAT4(-1, 0, 0, 1);
		Vertices[9].Tangent = XMFLOAT4(-1, 0, 0, 1);
		Vertices[10].Tangent = XMFLOAT4(-1, 0, 0, 1);
		Vertices[11].Tangent = XMFLOAT4(-1, 0, 0, 1);

		// +y face
		Vertices[12].Position = XMFLOAT3(-Pos, Pos, Pos);
		Vertices[13].Position = XMFLOAT3(Pos, Pos, Pos);
		Vertices[14].Position = XMFLOAT3(-Pos, Pos, -Pos);
		Vertices[15].Position = XMFLOAT3(Pos, Pos, -Pos);
		Vertices[12].Normal = XMFLOAT3(0, 1, 0);
		Vertices[13].Normal = XMFLOAT3(0, 1, 0);
		Vertices[14].Normal = XMFLOAT3(0, 1, 0);
		Vertices[15].Normal = XMFLOAT3(0, 1, 0);
		Vertices[12].Tangent = XMFLOAT4(1, 0, 0, 1);
		Vertices[13].Tangent = XMFLOAT4(1, 0, 0, 1);
		Vertices[14].Tangent = XMFLOAT4(1, 0, 0, 1);
		Vertices[15].Tangent = XMFLOAT4(1, 0, 0, 1);

		// -x face
		Vertices[16].Position = XMFLOAT3(-Pos, Pos, Pos);
		Vertices[17].Position = XMFLOAT3(-Pos, Pos, -Pos);
		Vertices[18].Position = XMFLOAT3(-Pos, -Pos, Pos);
		Vertices[19].Position = XMFLOAT3(-Pos, -Pos, -Pos);
		Vertices[16].Normal = XMFLOAT3(-1, 0, 0);
		Vertices[17].Normal = XMFLOAT3(-1, 0, 0);
		Vertices[18].Normal = XMFLOAT3(-1, 0, 0);
		Vertices[19].Normal = XMFLOAT3(-1, 0, 0);
		Vertices[16].Tangent = XMFLOAT4(0, 0, -1, 1);
		Vertices[17].Tangent = XMFLOAT4(0, 0, -1, 1);
		Vertices[18].Tangent = XMFLOAT4(0, 0, -1, 1);
		Vertices[19].Tangent = XMFLOAT4(0, 0, -1, 1);

		// +x face
		Vertices[20].Position = XMFLOAT3(Pos, Pos, -Pos);
		Vertices[21].Position = XMFLOAT3(Pos, Pos, Pos);
		Vertices[22].Position = XMFLOAT3(Pos, -Pos, -Pos);
		Vertices[23].Position = XMFLOAT3(Pos, -Pos, Pos);
		Vertices[20].Normal = XMFLOAT3(1, 0, 0);
		Vertices[21].Normal = XMFLOAT3(1, 0, 0);
		Vertices[22].Normal = XMFLOAT3(1, 0, 0);
		Vertices[23].Normal = XMFLOAT3(1, 0, 0);
		Vertices[20].Tangent = XMFLOAT4(0, 0, 1, 1);
		Vertices[21].Tangent = XMFLOAT4(0, 0, 1, 1);
		Vertices[22].Tangent = XMFLOAT4(0, 0, 1, 1);
		Vertices[23].Tangent = XMFLOAT4(0, 0, 1, 1);

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

		CubeMesh.SetMeshData(Vertices);
		CubeMesh.SetIndicesData(Indices);
		return CubeMesh;
	}
}
#endif