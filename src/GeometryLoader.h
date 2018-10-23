#pragma once
#include <string>
#include <vector>

#include "shared_with_shaders.h"

struct Face {
    uint32_t a, b, c;
};

struct Mesh {
    using Vec3Array = std::vector<vec3>;
    using Vec2Array = std::vector<vec2>;
    using FacesArray = std::vector<Face>;
    using FaceMaterialIDs = std::vector<uint32_t>;

    Vec3Array       positions;
    Vec3Array       normals;
    Vec2Array       uvs;
    FacesArray      faces;
    FaceMaterialIDs materialIDs;
};

class GeometryLoader {
public:
    GeometryLoader();
    ~GeometryLoader();

    bool                LoadFromOBJ(const std::wstring& fileName);

    size_t              GetNumMeshes() const;

    size_t              GetNumVertices(const size_t meshIdx) const;
    const vec3*         GetPositions(const size_t meshIdx) const;
    const vec3*         GetNormals(const size_t meshIdx) const;
    const vec2*         GetUVs(const size_t meshIdx) const;

    size_t              GetNumFaces(const size_t meshIdx) const;
    const Face*         GetFaces(const size_t meshIdx) const;
    uint32_t*           GetFaceMaterialIDs(const size_t meshIdx);

    size_t              GetNumMaterials() const;
    const Material_s*   GetMaterials() const;

private:
    using MeshesArray = std::vector<Mesh>;
    using MaterialsArray = std::vector<Material_s>;

    MeshesArray     mMeshes;
    MaterialsArray  mMaterials;
};
