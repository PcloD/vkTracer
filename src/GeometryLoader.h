#pragma once
#include <string>
#include <vector>

#include "shared_with_shaders.h"

struct Vertex {
    float posX, posY, posZ;
    float normalX, normalY, normalZ;
    float u, v;
};

struct Face {
    uint32_t a, b, c;
};

struct Mesh {
    using VerticesArray = std::vector<Vertex>;
    using FacesArray = std::vector<Face>;
    using FaceMaterialIDs = std::vector<uint32_t>;

    VerticesArray   vertices;
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
    const Vertex*       GetVertices(const size_t meshIdx) const;

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
