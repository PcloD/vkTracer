#pragma once

#include "framework/RaytracingApplication.h"

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
    VkDeviceMemory              mTopASMemory;
    VkDeviceMemory              mBottomASMemory;
    VkAccelerationStructureNVX  mTopAS;
    VkAccelerationStructureNVX  mBottomAS;
    VkDescriptorSetLayout       mRTDescriptorSetLayout;
    VkPipelineLayout            mRTPipelineLayout;
    VkPipeline                  mRTPipeline;
    VkDescriptorPool            mRTDescriptorPool;
    VkDescriptorSet             mRTDescriptorSet;

    BufferResource              mShaderBindingTable;
};
