#include "GeometryLoader.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <locale>
#include <codecvt>

inline std::string UnicodeToUtf8(const std::wstring & _unicode) {
    std::wstring_convert<std::codecvt_utf8<std::wstring::value_type>, std::wstring::value_type> convert;
    return std::move(convert.to_bytes(_unicode));
}

GeometryLoader::GeometryLoader() {

}
GeometryLoader::~GeometryLoader() {

}

bool GeometryLoader::LoadFromOBJ(const std::wstring& fileName) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, error;

    std::string utf8FileName = UnicodeToUtf8(fileName);
    std::string baseDir = utf8FileName;
    const size_t slash = baseDir.find_last_of('/');
    if (slash != std::string::npos) {
        baseDir.erase(slash);
    }

    const bool result = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &error, utf8FileName.c_str(), baseDir.c_str(), true);
    if (result) {
        mMeshes.resize(shapes.size());

        for (size_t meshIdx = 0; meshIdx < mMeshes.size(); ++meshIdx) {
            Mesh& mesh = mMeshes[meshIdx];
            const tinyobj::shape_t& shape = shapes[meshIdx];

            const size_t numFaces = shape.mesh.num_face_vertices.size();
            mesh.faces.resize(numFaces);
            mesh.materialIDs.resize(numFaces);
            mesh.vertices.resize(numFaces * 3);

            size_t vIdx = 0;
            for (size_t f = 0; f < numFaces; ++f) {
                assert(shape.mesh.num_face_vertices[f] == 3);
                for (size_t j = 0; j < 3; ++j, ++vIdx) {
                    Vertex& vertex = mesh.vertices[vIdx];
                    const tinyobj::index_t& i = shape.mesh.indices[vIdx];

                    vertex.posX = attrib.vertices[3 * i.vertex_index + 0];
                    vertex.posY = attrib.vertices[3 * i.vertex_index + 1];
                    vertex.posZ = attrib.vertices[3 * i.vertex_index + 2];
                    vertex.normalX = attrib.normals[3 * i.normal_index + 0];
                    vertex.normalY = attrib.normals[3 * i.normal_index + 1];
                    vertex.normalZ = attrib.normals[3 * i.normal_index + 2];
                    vertex.u = attrib.texcoords[2 * i.texcoord_index + 0];
                    vertex.v = attrib.texcoords[2 * i.texcoord_index + 1];
                }

                Face& face = mesh.faces[f];
                face.a = static_cast<uint32_t>(3 * f + 0);
                face.b = static_cast<uint32_t>(3 * f + 1);
                face.c = static_cast<uint32_t>(3 * f + 2);

                mesh.materialIDs[f] = static_cast<uint32_t>(shape.mesh.material_ids[f]);
            }
        }

        const size_t numMaterials = materials.size();
        mMaterials.resize(numMaterials);
        for (size_t i = 0; i < numMaterials; ++i) {
            const tinyobj::material_t& srcMat = materials[i];
            Material_s& dstMat = mMaterials[i];

            dstMat.diffuse[0] = srcMat.diffuse[0];
            dstMat.diffuse[1] = srcMat.diffuse[1];
            dstMat.diffuse[2] = srcMat.diffuse[2];
            dstMat.diffuse[3] = 1.0f;

            dstMat.emission[0] = srcMat.emission[0];
            dstMat.emission[1] = srcMat.emission[1];
            dstMat.emission[2] = srcMat.emission[2];
            dstMat.emission[3] = 1.0f;
        }
    }

    return result;
}

size_t GeometryLoader::GetNumMeshes() const {
    return mMeshes.size();
}

size_t GeometryLoader::GetNumVertices(const size_t meshIdx) const {
    return mMeshes[meshIdx].vertices.size();
}

const Vertex* GeometryLoader::GetVertices(const size_t meshIdx) const {
    return mMeshes[meshIdx].vertices.data();
}

size_t GeometryLoader::GetNumFaces(const size_t meshIdx) const {
    return mMeshes[meshIdx].faces.size();
}

const Face* GeometryLoader::GetFaces(const size_t meshIdx) const {
    return mMeshes[meshIdx].faces.data();
}

uint32_t* GeometryLoader::GetFaceMaterialIDs(const size_t meshIdx) {
    return mMeshes[meshIdx].materialIDs.data();
}

size_t GeometryLoader::GetNumMaterials() const {
    return mMaterials.size();
}

const Material_s* GeometryLoader::GetMaterials() const {
    return mMaterials.data();
}
