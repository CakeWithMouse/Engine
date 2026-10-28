#pragma once

#include <Engine/Render/MeshData.h>

/**
 * Procedural meshes in VertexFormat::Mesh: unit normals, UVs in [0, 1], centred on the origin.
 * Triangles are clockwise seen from the side the normal points to (the front face of the
 * rasterizer), so these meshes can be drawn with back-face culling.
 */
namespace MeshPrimitives
{
    /** UV sphere: u runs once around the equator, v from the north pole (0) to the south pole (1). */
    MeshLodData Sphere(float radius = 0.5f, int slices = 48, int stacks = 24);
    /** Cube with flat faces; every face maps the whole [0, 1] UV square. */
    MeshLodData Cube(float size = 1.0f);
    /** Plane in XZ facing +Y, split into subdivisions x subdivisions quads; u along +X, v along -Z. */
    MeshLodData Plane(float width = 1.0f, float depth = 1.0f, int subdivisions = 1);
    /** Cylinder along Y with flat caps; u runs around the side, v from the top (0) to the bottom (1). */
    MeshLodData Cylinder(float radius = 0.5f, float height = 1.0f, int slices = 32);
    /** Cone along Y with its apex at +height/2 and a flat base; side UVs as for the cylinder. */
    MeshLodData Cone(float radius = 0.5f, float height = 1.0f, int slices = 32);
    /** Pyramid with a size x size base at -height/2 and its apex at +height/2; flat faces. */
    MeshLodData Pyramid(float size = 1.0f, float height = 1.0f);
}
