#pragma once

#include "Mesh.h"
#include "Vertex.h"         

class BaseGeometry {
public:
	static BaseMeshData CreateSphere(float radius, uint32_t slice, uint32_t stack);
	static BaseMeshData CreateCube(float width, float height, float depth, uint32_t numSubdivision);
	static BaseMeshData CreateQuad(float x, float y, float w, float h, float depth);
	static BaseMeshData CreateGrid(float width, float depth, uint32_t m, uint32_t n);
private:
	static void Subdivide(BaseMeshData& meshData);
	static Vertex_CPU MidPoint(const Vertex_CPU& v0, const Vertex_CPU& v1);
};

