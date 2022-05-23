#include "BaseGeometry.h"

BaseMeshData BaseGeometry::CreateSphere(float radius, uint32_t slice, uint32_t stack)
{
	BaseMeshData meshData;

	//
	// Compute the vertices stating at the top pole and moving down the stacks.
	//

	// Poles: note that there will be texture coordinate distortion as there is
	// not a unique point on the texture map to assign to the pole when mapping
	// a rectangular texture onto a sphere.
	Vertex_CPU topVertex(0.0f, +radius, 0.0f, 0.0f, +1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	Vertex_CPU bottomVertex(0.0f, -radius, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

	meshData.VBOs.emplace_back( topVertex );

	float phiStep   = XM_PI / static_cast<float>(stack);
	float thetaStep = 2.0f * XM_PI / static_cast<float>(slice);

	// Compute vertices for each stack ring (do not count the poles as rings).
	for(uint32_t i = 1; i <= stack - 1; ++i)
	{
		float phi = i * phiStep;

		// VBOs of ring.
        for(uint32_t j = 0; j <= slice; ++j)
		{
			float theta = j * thetaStep;

			Vertex_CPU v;

			// spherical to cartesian
			v.pos.x = radius*sinf(phi)*cosf(theta);
			v.pos.y = radius*cosf(phi);
			v.pos.z = radius*sinf(phi)*sinf(theta);

			// Partial derivative of P with respect to theta
			v.tangent.x = -radius*sinf(phi)*sinf(theta);
			v.tangent.y = 0.0f;
			v.tangent.z = +radius*sinf(phi)*cosf(theta);

			XMVECTOR T = XMLoadFloat3(&v.tangent);
			XMStoreFloat3(&v.tangent, XMVector3Normalize(T));

			XMVECTOR p = XMLoadFloat3(&v.pos);
			XMStoreFloat3(&v.normal, XMVector3Normalize(p));

			v.tex.x = theta / XM_2PI;
			v.tex.y = phi / XM_PI;

			meshData.VBOs.emplace_back( v );
		}
	}

	meshData.VBOs.emplace_back( bottomVertex );

	//
	// Compute indices for top stack.  The top stack was written first to the vertex buffer
	// and connects the top pole to the first ring.
	//

    for(uint32_t i = 1; i <= slice; ++i)
	{
		meshData.EBOs.emplace_back(0);
		meshData.EBOs.emplace_back(i+1);
		meshData.EBOs.emplace_back(i);
	}
	
	//
	// Compute indices for inner stacks (not connected to poles).
	//

	// Offset the indices to the index of the first vertex in the first ring.
	// This is just skipping the top pole vertex.
    uint32_t baseIndex = 1;
    uint32_t ringVertexCount = slice + 1;
	for(uint32_t i = 0; i < stack-2; ++i)
	{
		for(uint32_t j = 0; j < slice; ++j)
		{
			meshData.EBOs.emplace_back(baseIndex + i*ringVertexCount + j);
			meshData.EBOs.emplace_back(baseIndex + i*ringVertexCount + j+1);
			meshData.EBOs.emplace_back(baseIndex + (i+1)*ringVertexCount + j);

			meshData.EBOs.emplace_back(baseIndex + (i+1)*ringVertexCount + j);
			meshData.EBOs.emplace_back(baseIndex + i*ringVertexCount + j+1);
			meshData.EBOs.emplace_back(baseIndex + (i+1)*ringVertexCount + j+1);
		}
	}

	//
	// Compute indices for bottom stack.  The bottom stack was written last to the vertex buffer
	// and connects the bottom pole to the bottom ring.
	//

	// South pole vertex was added last.
	uint32_t southPoleIndex = static_cast<uint32_t>(meshData.VBOs.size())-1;

	// Offset the indices to the index of the first vertex in the last ring.
	baseIndex = southPoleIndex - ringVertexCount;
	
	for(uint32_t i = 0; i < slice; ++i)
	{
		meshData.EBOs.emplace_back(southPoleIndex);
		meshData.EBOs.emplace_back(baseIndex+i);
		meshData.EBOs.emplace_back(baseIndex+i+1);
	}

    return meshData;
}

BaseMeshData BaseGeometry::CreateCube(float width, float height, float depth, uint32_t numSubdivision)
{
	BaseMeshData meshData;

    //
	// Create the vertices.
	//

	Vertex_CPU v[24];

	float w2 = 0.5f*width;
	float h2 = 0.5f*height;
	float d2 = 0.5f*depth;
    
	// Fill in the front face vertex data.
	v[0] = Vertex_CPU(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[1] = Vertex_CPU(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[2] = Vertex_CPU(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[3] = Vertex_CPU(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the back face vertex data.
	v[4] = Vertex_CPU(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	v[5] = Vertex_CPU(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[6] = Vertex_CPU(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[7] = Vertex_CPU(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Fill in the top face vertex data.
	v[8]  = Vertex_CPU(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[9]  = Vertex_CPU(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[10] = Vertex_CPU(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
	v[11] = Vertex_CPU(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

	// Fill in the bottom face vertex data.
	v[12] = Vertex_CPU(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	v[13] = Vertex_CPU(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
	v[14] = Vertex_CPU(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	v[15] = Vertex_CPU(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);

	// Fill in the left face vertex data.
	v[16] = Vertex_CPU(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
	v[17] = Vertex_CPU(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
	v[18] = Vertex_CPU(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
	v[19] = Vertex_CPU(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

	// Fill in the right face vertex data.
	v[20] = Vertex_CPU(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
	v[21] = Vertex_CPU(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
	v[22] = Vertex_CPU(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
	v[23] = Vertex_CPU(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);

	meshData.VBOs.assign(&v[0], &v[24]);
 
	//
	// Create the indices.
	//

	uint32_t i[36];

	// Fill in the front face index data
	i[0] = 0; i[1] = 1; i[2] = 2;
	i[3] = 0; i[4] = 2; i[5] = 3;

	// Fill in the back face index data
	i[6] = 4; i[7]  = 5; i[8]  = 6;
	i[9] = 4; i[10] = 6; i[11] = 7;

	// Fill in the top face index data
	i[12] = 8; i[13] =  9; i[14] = 10;
	i[15] = 8; i[16] = 10; i[17] = 11;

	// Fill in the bottom face index data
	i[18] = 12; i[19] = 13; i[20] = 14;
	i[21] = 12; i[22] = 14; i[23] = 15;

	// Fill in the left face index data
	i[24] = 16; i[25] = 17; i[26] = 18;
	i[27] = 16; i[28] = 18; i[29] = 19;

	// Fill in the right face index data
	i[30] = 20; i[31] = 21; i[32] = 22;
	i[33] = 20; i[34] = 22; i[35] = 23;

	meshData.EBOs.assign(&i[0], &i[36]);

    // Put a cap on the number of subdivisions.
    numSubdivision = std::min<uint32_t>(numSubdivision, 6u);

    for(uint32_t i = 0; i < numSubdivision; ++i)
        Subdivide(meshData);

    return meshData;
}

BaseMeshData BaseGeometry::CreateQuad(float x, float y, float w, float h, float depth)
{
    BaseMeshData meshData;

	meshData.VBOs.resize(4);
	meshData.EBOs.resize(6);

	// Position coordinates specified in NDC space.
	meshData.VBOs[0] = Vertex_CPU(
        x, y - h, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 1.0f);

	meshData.VBOs[1] = Vertex_CPU(
		x, y, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		0.0f, 0.0f);

	meshData.VBOs[2] = Vertex_CPU(
		x+w, y, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f);

	meshData.VBOs[3] = Vertex_CPU(
		x+w, y-h, depth,
		0.0f, 0.0f, -1.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 1.0f);

	meshData.EBOs[0] = 0;
	meshData.EBOs[1] = 1;
	meshData.EBOs[2] = 2;

	meshData.EBOs[3] = 0;
	meshData.EBOs[4] = 2;
	meshData.EBOs[5] = 3;

    return meshData;
}

BaseMeshData BaseGeometry::CreateGrid(float width, float depth, uint32_t m, uint32_t n)
{
	BaseMeshData meshData;

	uint32_t vertexCount = m*n;
	uint32_t faceCount   = (m-1)*(n-1)*2;

	//
	// Create the vertices.
	//

	float halfWidth = 0.5f * static_cast<float>(width);
	float halfDepth = 0.5f * static_cast<float>(depth);

	float dx = width / (n-1);
	float dz = depth / (m-1);

	float du = 1.0f / (n-1);
	float dv = 1.0f / (m-1);

	meshData.VBOs.resize(vertexCount);
	for(uint32_t i = 0; i < m; ++i)
	{
		float z = halfDepth - i*dz;
		for(uint32_t j = 0; j < n; ++j)
		{
			float x = -halfWidth + j*dx;

			meshData.VBOs[i*n+j].pos = XMFLOAT3(x, 0.0f, z);
			meshData.VBOs[i*n+j].normal   = XMFLOAT3(0.0f, 1.0f, 0.0f);
			meshData.VBOs[i*n+j].tangent = XMFLOAT3(1.0f, 0.0f, 0.0f);

			// Stretch texture over grid.

			meshData.VBOs[i*n+j].tex.x = j*du;
			meshData.VBOs[i*n+j].tex.y = i*dv;
		}
	}
 
    //
	// Create the indices.
	//

	meshData.EBOs.resize(faceCount*3); // 3 indices per face

	// Iterate over each quad and compute indices.
	uint32_t k = 0;
	for(uint32_t i = 0; i < m-1; ++i)
	{
		for(uint32_t j = 0; j < n-1; ++j)
		{
			meshData.EBOs[k]   = i*n+j;
			meshData.EBOs[k+1] = i*n+j+1;
			meshData.EBOs[k+2] = (i+1)*n+j;

			meshData.EBOs[k+3] = (i+1)*n+j;
			meshData.EBOs[k+4] = i*n+j+1;
			meshData.EBOs[k+5] = (i+1)*n+j+1;

			k += 6; // next quad
		}
	}

    return meshData;
}

BaseMeshData BaseGeometry::CreateCanvas() {
	BaseMeshData data;
	constexpr XMFLOAT2 uv[4] =
	{
	    XMFLOAT2(0.0f, 1.0f),
	    XMFLOAT2(0.0f, 0.0f),
	    XMFLOAT2(1.0f, 0.0f),
	    XMFLOAT2(1.0f, 1.0f)
	};
	constexpr XMFLOAT3 pos[4] =
	{
		XMFLOAT3(2.0f * uv[0].x - 1.0f, 1.0f - 2.0f * uv[0].y, 0.0f),
		XMFLOAT3(2.0f * uv[1].x - 1.0f, 1.0f - 2.0f * uv[1].y, 0.0f),
		XMFLOAT3(2.0f * uv[2].x - 1.0f, 1.0f - 2.0f * uv[2].y, 0.0f),
		XMFLOAT3(2.0f * uv[3].x - 1.0f, 1.0f - 2.0f * uv[3].y, 0.0f),
	};
	data.VBOs.resize(4);
	data.EBOs.resize(6);
	for (int i = 0; i < 4; ++i)
	{
		data.VBOs[i].pos = pos[i];
		data.VBOs[i].tex = uv[i];
	}

	data.EBOs[0] = 0;
	data.EBOs[1] = 1;
	data.EBOs[2] = 2;
	data.EBOs[3] = 0;
	data.EBOs[4] = 2;
	data.EBOs[5] = 3;
	return data;
}

void BaseGeometry::Subdivide(BaseMeshData& meshData)
{
	// Save a copy of  the input geometry.
	BaseMeshData inputCopy = meshData;

	vector<Vertex_CPU>().swap(meshData.VBOs);
	vector<uint32_t>().swap(meshData.EBOs);

	//       v1
	//       *
	//      / \
	//     /   \
	//  m0*-----*m1
	//   / \   / \
	//  /   \ /   \
	// *-----*-----*
	// v0    m2     v2

	uint32_t numTris = (uint32_t)inputCopy.EBOs.size()/3;
	meshData.VBOs.reserve(numTris * 5ull);
	meshData.EBOs.reserve(numTris * 12ull);

	for(uint32_t i = 0; i < numTris; ++i)
	{
		Vertex_CPU v0 = inputCopy.VBOs[ inputCopy.EBOs[i*3+0] ];
		Vertex_CPU v1 = inputCopy.VBOs[ inputCopy.EBOs[i*3+1] ];
		Vertex_CPU v2 = inputCopy.VBOs[ inputCopy.EBOs[i*3+2] ];

		//
		// Generate the midpoints.
		//

        Vertex_CPU m0 = MidPoint(v0, v1);
        Vertex_CPU m1 = MidPoint(v1, v2);
        Vertex_CPU m2 = MidPoint(v0, v2);

		//
		// Add new geometry.
		//

		meshData.VBOs.emplace_back(v0); // 0
		meshData.VBOs.emplace_back(v1); // 1
		meshData.VBOs.emplace_back(v2); // 2
		meshData.VBOs.emplace_back(m0); // 3
		meshData.VBOs.emplace_back(m1); // 4
		meshData.VBOs.emplace_back(m2); // 5
 
		meshData.EBOs.emplace_back(i*6+0);
		meshData.EBOs.emplace_back(i*6+3);
		meshData.EBOs.emplace_back(i*6+5);

		meshData.EBOs.emplace_back(i*6+3);
		meshData.EBOs.emplace_back(i*6+4);
		meshData.EBOs.emplace_back(i*6+5);

		meshData.EBOs.emplace_back(i*6+5);
		meshData.EBOs.emplace_back(i*6+4);
		meshData.EBOs.emplace_back(i*6+2);

		meshData.EBOs.emplace_back(i*6+3);
		meshData.EBOs.emplace_back(i*6+1);
		meshData.EBOs.emplace_back(i*6+4);
	}
}

Vertex_CPU BaseGeometry::MidPoint(const Vertex_CPU& v0, const Vertex_CPU& v1)
{
	XMVECTOR p0 = XMLoadFloat3(&v0.pos);
    XMVECTOR p1 = XMLoadFloat3(&v1.pos);

    XMVECTOR n0 = XMLoadFloat3(&v0.normal);
    XMVECTOR n1 = XMLoadFloat3(&v1.normal);

    XMVECTOR tan0 = XMLoadFloat3(&v0.tangent);
    XMVECTOR tan1 = XMLoadFloat3(&v1.tangent);

    XMVECTOR tex0 = XMLoadFloat2(&v0.tex);
    XMVECTOR tex1 = XMLoadFloat2(&v1.tex);

    // Compute the midpoints of all the attributes.  Vectors need to be normalized
    // since linear interpolating can make them not unit length.  
    XMVECTOR pos = 0.5f*(p0 + p1);
    XMVECTOR normal = XMVector3Normalize(0.5f*(n0 + n1));
    XMVECTOR tangent = XMVector3Normalize(0.5f*(tan0+tan1));
    XMVECTOR tex = 0.5f*(tex0 + tex1);

    Vertex_CPU v;
    XMStoreFloat3(&v.pos, pos);
    XMStoreFloat3(&v.normal, normal);
    XMStoreFloat3(&v.tangent, tangent);
    XMStoreFloat2(&v.tex, tex);

    return v;
}
