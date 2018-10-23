#pragma once

#include "framework/RaytracingApplication.h"
#include "GeometryLoader.h"

#include <array>
#include <vector>

struct RTGeometry {
    RTGeometry()
        : as(VK_NULL_HANDLE)
        , asMemory(VK_NULL_HANDLE)
    { }

    VkGeometryNVX               vkgeo;
    VkAccelerationStructureNVX  as;
    VkDeviceMemory              asMemory;
    BufferResource              positions;
    BufferResource              normals;
    BufferResource              uvs;
    BufferResource              ib;
    BufferResource              faces;
    BufferResource              matIDs;
};

class vkTracer : public RaytracingApplication {
public:
    vkTracer();
    virtual ~vkTracer();

    virtual void Init() override;
    virtual void RecordCommandBufferForFrame(VkCommandBuffer commandBuffer, uint32_t frameIndex) override;
    virtual void Cleanup() override;

    void CreateAccelerationStructures();
    void CreateSceneShaderData();
    void CreateDescriptorSetLayouts();
    void CreatePipeline();
    void CreateShaderBindingTable();
    void CreateDescriptorSets();
    void UpdateDescriptorSets();

private:
    VkDeviceMemory                          mTopASMemory;
    VkAccelerationStructureNVX              mTopAS;
    VkPipelineLayout                        mRTPipelineLayout;
    VkPipeline                              mRTPipeline;
    VkDescriptorPool                        mRTDescriptorPool;
    std::array<VkDescriptorSetLayout, 4>    mRTDescriptorSetLayouts;
    std::array<VkDescriptorSet, 4>          mRTDescriptorSets;

    BufferResource                          mShaderBindingTable;

    GeometryLoader                          mGeometryLoader;
    std::vector<RTGeometry>                 mRTGeometries;
    BufferResource                          mRTMaterialsBuffer;
    std::vector<VkBufferView>               mRTFaceMaterialIDBufferViews;
    std::vector<VkBufferView>               mRTFacesBufferViews;
    std::vector<VkBufferView>               mRTNormalsBufferViews;
};
