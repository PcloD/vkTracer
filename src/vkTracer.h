#pragma once

#include "framework/RaytracingApplication.h"
#include "GeometryLoader.h"

#include <vector>

struct RTGeometry {
    RTGeometry()
        : as(VK_NULL_HANDLE)
        , asMemory(VK_NULL_HANDLE)
    { }

    VkGeometryNVX               vkgeo;
    VkAccelerationStructureNVX  as;
    VkDeviceMemory              asMemory;
    BufferResource              vb;
    BufferResource              ib;
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
    void CreatePipeline();
    void CreateShaderBindingTable();
    void CreateDescriptorSet();

private:
    VkDeviceMemory                          mTopASMemory;
    VkAccelerationStructureNVX              mTopAS;
    VkDescriptorSetLayout                   mRTDescriptorSetLayout;
    VkPipelineLayout                        mRTPipelineLayout;
    VkPipeline                              mRTPipeline;
    VkDescriptorPool                        mRTDescriptorPool;
    VkDescriptorSet                         mRTDescriptorSet;

    BufferResource                          mShaderBindingTable;

    GeometryLoader                          mGeometryLoader;
    std::vector<RTGeometry>                 mRTGeometries;
};
