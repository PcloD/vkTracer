#include "vkTracer.h"

vkTracer::vkTracer()
    : mTopASMemory(VK_NULL_HANDLE)
    , mTopAS(VK_NULL_HANDLE)
    , mRTPipelineLayout(VK_NULL_HANDLE)
    , mRTPipeline(VK_NULL_HANDLE)
    , mRTDescriptorPool(VK_NULL_HANDLE)
    , mRTDescriptorSetLayouts({ VK_NULL_HANDLE })
    , mRTDescriptorSets({ VK_NULL_HANDLE })
{
    _appName = L"vkTracer";
    _shadersFolder = L"/_data/shaders/";
    _imagesFolder = L"/_data/textures/";
    _geometryFolder = L"/_data/geometries/";

    _deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    _deviceExtensions.push_back(VK_NVX_RAYTRACING_EXTENSION_NAME);

    _settings.DesiredWindowWidth = 800;
    _settings.DesiredWindowHeight = 600;
}

vkTracer::~vkTracer() {

}

void vkTracer::Init() {
    this->InitRaytracing();

    this->CreateAccelerationStructures();
    this->CreateDataBuffers();
    this->CreateDescriptorSetLayouts();
    this->CreatePipeline();
    this->CreateShaderBindingTable();
    this->CreateDescriptorSets();
    this->UpdateDescriptorSets();
}

void vkTracer::RecordCommandBufferForFrame(VkCommandBuffer commandBuffer, uint32_t frameIndex) {
    vkCmdBindPipeline(commandBuffer,
                      VK_PIPELINE_BIND_POINT_RAYTRACING_NVX,
                      mRTPipeline);

    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_RAYTRACING_NVX,
                            mRTPipelineLayout, 0,
                            static_cast<uint32_t>(mRTDescriptorSets.size()), mRTDescriptorSets.data(),
                            0, 0);

    // Here's how the shader binding table looks like in our case
    // |[ raygen shader ]|[ hit shaders ]|[ miss shader ]|
    // |                 |               |               |
    // | 0               | 1             | 2             | 3

    vkCmdTraceRaysNVX(commandBuffer,
        mShaderBindingTable.Buffer, 0,
        mShaderBindingTable.Buffer, 2 * _raytracingProperties.shaderHeaderSize, _raytracingProperties.shaderHeaderSize,
        mShaderBindingTable.Buffer, 1 * _raytracingProperties.shaderHeaderSize, _raytracingProperties.shaderHeaderSize,
        _actualWindowWidth, _actualWindowHeight);
}

void vkTracer::Cleanup() {
    if (mTopAS) {
        vkDestroyAccelerationStructureNVX(_device, mTopAS, nullptr);
    }
    if (mTopASMemory) {
        vkFreeMemory(_device, mTopASMemory, nullptr);
    }

    for (auto& rtgeom : mRTGeometries) {
        if (rtgeom.as) {
            vkDestroyAccelerationStructureNVX(_device, rtgeom.as, nullptr);
            vkFreeMemory(_device, rtgeom.asMemory, nullptr);
        }
    }
    mRTGeometries.resize(0);

    if (mRTDescriptorPool) {
        vkDestroyDescriptorPool(_device, mRTDescriptorPool, nullptr);
    }

    mShaderBindingTable.Cleanup();

    if (mRTPipeline) {
        vkDestroyPipeline(_device, mRTPipeline, nullptr);
    }
    if (mRTPipelineLayout) {
        vkDestroyPipelineLayout(_device, mRTPipelineLayout, nullptr);
    }

    for (auto& ds : mRTDescriptorSetLayouts) {
        if (ds) {
            vkDestroyDescriptorSetLayout(_device, ds, nullptr);
        }
    }

    RaytracingApplication::Cleanup();
}

void vkTracer::CreateAccelerationStructures() {
    const float transform[12] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
    };

    std::wstring sceneFileName = _basePath + _geometryFolder + L"cornellbox.obj";

    if (!mGeometryLoader.LoadFromOBJ(sceneFileName)) {
        ExitError(L"Failed to load scene: " + sceneFileName);
    }

    auto CreateAccelerationStructure = [&](VkAccelerationStructureTypeNVX type, size_t geometryCount, VkGeometryNVX* geometries, size_t instanceCount, VkAccelerationStructureNVX& AS, VkDeviceMemory& memory) {
        VkAccelerationStructureCreateInfoNVX accelerationStructureInfo;
        accelerationStructureInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_NVX;
        accelerationStructureInfo.pNext = nullptr;
        accelerationStructureInfo.type = type;
        accelerationStructureInfo.flags = 0;
        accelerationStructureInfo.compactedSize = 0;
        accelerationStructureInfo.instanceCount = static_cast<uint32_t>(instanceCount);
        accelerationStructureInfo.geometryCount = static_cast<uint32_t>(geometryCount);
        accelerationStructureInfo.pGeometries = geometries;

        VkResult code = vkCreateAccelerationStructureNVX(_device, &accelerationStructureInfo, nullptr, &AS);
        NVVK_CHECK_ERROR(code, L"vkCreateAccelerationStructureNV");

        VkAccelerationStructureMemoryRequirementsInfoNVX memoryRequirementsInfo;
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX;
        memoryRequirementsInfo.pNext = nullptr;
        memoryRequirementsInfo.accelerationStructure = AS;

        VkMemoryRequirements2 memoryRequirements;
        vkGetAccelerationStructureMemoryRequirementsNVX(_device, &memoryRequirementsInfo, &memoryRequirements);

        VkMemoryAllocateInfo memoryAllocateInfo;
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.pNext = nullptr;
        memoryAllocateInfo.allocationSize = memoryRequirements.memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = ResourceBase::GetMemoryType(memoryRequirements.memoryRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        code = vkAllocateMemory(_device, &memoryAllocateInfo, nullptr, &memory);
        NVVK_CHECK_ERROR(code, L"rt AS vkAllocateMemory");

        VkBindAccelerationStructureMemoryInfoNVX bindInfo;
        bindInfo.sType = VK_STRUCTURE_TYPE_BIND_ACCELERATION_STRUCTURE_MEMORY_INFO_NVX;
        bindInfo.pNext = nullptr;
        bindInfo.accelerationStructure = AS;
        bindInfo.memory = memory;
        bindInfo.memoryOffset = 0;
        bindInfo.deviceIndexCount = 0;
        bindInfo.pDeviceIndices = nullptr;

        code = vkBindAccelerationStructureMemoryNVX(_device, 1, &bindInfo);
        NVVK_CHECK_ERROR(code, L"vkBindAccelerationStructureMemoryNV");
    };

    const size_t numMeshes = mGeometryLoader.GetNumMeshes();

    mRTGeometries.resize(numMeshes);
    std::vector<VkGeometryInstance> instances(numMeshes);

    for (size_t i = 0; i < numMeshes; ++i) {
        const uint32_t vertexSize = sizeof(Vertex);
        const uint32_t numVertices = static_cast<uint32_t>(mGeometryLoader.GetNumVertices(i));
        const uint32_t verticesDataSize = numVertices * vertexSize;

        const uint32_t numFaces = static_cast<uint32_t>(mGeometryLoader.GetNumFaces(i));
        const uint32_t numIndices = numFaces * 3;
        const uint32_t indicesDataSize = numIndices * sizeof(uint32_t);

        const uint32_t matIDsDataSize = numFaces * sizeof(uint32_t);

        const Vertex* vertices = mGeometryLoader.GetVertices(i);
        const Face* faces = mGeometryLoader.GetFaces(i);
        const uint32_t* matIDs = mGeometryLoader.GetFaceMaterialIDs(i);

        RTGeometry& rtgeom = mRTGeometries[i];

        VkResult error = rtgeom.matIDs.Create(matIDsDataSize, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(error, L"rt matIDs.Create");
        if (!rtgeom.matIDs.CopyToBufferUsingMapUnmap(matIDs, matIDsDataSize)) {
            ExitError(L"Failed to copy matIDs buffer");
        }

        error = rtgeom.vb.Create(verticesDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(error, L"rt vb.Create");
        if (!rtgeom.vb.CopyToBufferUsingMapUnmap(vertices, verticesDataSize)) {
            ExitError(L"Failed to copy vertex buffer");
        }

        error = rtgeom.ib.Create(indicesDataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(error, L"rt ib.Create");
        if (!rtgeom.ib.CopyToBufferUsingMapUnmap(faces, indicesDataSize)) {
            ExitError(L"Failed to copy index buffer");
        }

        VkGeometryNVX& vkgeo = rtgeom.vkgeo;

        vkgeo.sType = VK_STRUCTURE_TYPE_GEOMETRY_NVX;
        vkgeo.pNext = nullptr;
        vkgeo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NVX;
        vkgeo.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NVX;
        vkgeo.geometry.triangles.pNext = nullptr;
        vkgeo.geometry.triangles.vertexData = rtgeom.vb.Buffer;
        vkgeo.geometry.triangles.vertexOffset = 0;
        vkgeo.geometry.triangles.vertexCount = numVertices;
        vkgeo.geometry.triangles.vertexStride = vertexSize;
        vkgeo.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        vkgeo.geometry.triangles.indexData = rtgeom.ib.Buffer;
        vkgeo.geometry.triangles.indexOffset = 0;
        vkgeo.geometry.triangles.indexCount = numIndices;
        vkgeo.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        vkgeo.geometry.triangles.transformData = VK_NULL_HANDLE;
        vkgeo.geometry.triangles.transformOffset = 0;
        vkgeo.geometry.aabbs = { };
        vkgeo.geometry.aabbs.sType = VK_STRUCTURE_TYPE_GEOMETRY_AABB_NVX;
        vkgeo.flags = 0;

        // Bottom level acceleration structures represents the actual geometry
        CreateAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX, 1, &vkgeo, 0, rtgeom.as, rtgeom.asMemory);

        uint64_t accelerationStructureHandle;
        error = vkGetAccelerationStructureHandleNVX(_device, rtgeom.as, sizeof(uint64_t), &accelerationStructureHandle);
        NVVK_CHECK_ERROR(error, L"vkGetAccelerationStructureHandleNV");

        VkGeometryInstance& instance = instances[i];

        memcpy(instance.transform, transform, sizeof(instance.transform));
        instance.instanceId = static_cast<uint32_t>(i);
        instance.mask = 0xff;
        instance.instanceOffset = 0;
        instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NVX;
        instance.accelerationStructureHandle = accelerationStructureHandle;
    }

    BufferResource instanceBuffer;
    const uint32_t instanceBufferSize = static_cast<uint32_t>(instances.size() * sizeof(VkGeometryInstance));
    VkResult error = instanceBuffer.Create(instanceBufferSize, VK_BUFFER_USAGE_RAYTRACING_BIT_NVX, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    NVVK_CHECK_ERROR(error, L"rt instanceBuffer.Create");
    instanceBuffer.CopyToBufferUsingMapUnmap(instances.data(), instanceBufferSize);

    // Top level acceleration structure represents our scene, that contains all the geometry and instances
    CreateAccelerationStructure(VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX, 0, nullptr, 1, mTopAS, mTopASMemory);

    // Now we build our acceleration structures

    auto GetScratchBufferSize = [&](VkAccelerationStructureNVX handle) -> VkDeviceSize {
        VkAccelerationStructureMemoryRequirementsInfoNVX memoryRequirementsInfo;
        memoryRequirementsInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_MEMORY_REQUIREMENTS_INFO_NVX;
        memoryRequirementsInfo.pNext = nullptr;
        memoryRequirementsInfo.accelerationStructure = handle;

        VkMemoryRequirements2 memoryRequirements;
        vkGetAccelerationStructureScratchMemoryRequirementsNVX(_device, &memoryRequirementsInfo, &memoryRequirements);

        return memoryRequirements.memoryRequirements.size;
    };

    VkDeviceSize bottomAccelerationStructureBufferSize = 0;
    for (const auto& rtgeom : mRTGeometries) {
        const VkDeviceSize asSize = GetScratchBufferSize(rtgeom.as);
        if (asSize > bottomAccelerationStructureBufferSize) {
            bottomAccelerationStructureBufferSize = asSize;
        }
    }

    VkDeviceSize topAccelerationStructureBufferSize = GetScratchBufferSize(mTopAS);
    VkDeviceSize scratchBufferSize = std::max(bottomAccelerationStructureBufferSize, topAccelerationStructureBufferSize);

    BufferResource scratchBuffer;
    scratchBuffer.Create(scratchBufferSize, VK_BUFFER_USAGE_RAYTRACING_BIT_NVX, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkCommandBufferAllocateInfo commandBufferAllocateInfo;
    commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocateInfo.pNext = nullptr;
    commandBufferAllocateInfo.commandPool = _commandPool;
    commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    error = vkAllocateCommandBuffers(_device, &commandBufferAllocateInfo, &commandBuffer);
    NVVK_CHECK_ERROR(error, L"rt vkAllocateCommandBuffers");

    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;
    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkMemoryBarrier memoryBarrier;
    memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memoryBarrier.pNext = nullptr;
    memoryBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;
    memoryBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_NVX | VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_NVX;

    for (auto& rtgeom : mRTGeometries) {
        const VkGeometryNVX& geometry = rtgeom.vkgeo;

        vkCmdBuildAccelerationStructureNVX(commandBuffer,
                                           VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_NVX,
                                           0, VK_NULL_HANDLE, 0,
                                           1, &geometry, 0, VK_FALSE,
                                           rtgeom.as, VK_NULL_HANDLE,
                                           scratchBuffer.Buffer, 0);

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);
    }

    vkCmdBuildAccelerationStructureNVX(commandBuffer,
                                       VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_NVX,
                                       static_cast<uint32_t>(instances.size()),
                                       instanceBuffer.Buffer,
                                       0, 0, nullptr, 0, VK_FALSE,
                                       mTopAS, VK_NULL_HANDLE,
                                       scratchBuffer.Buffer, 0);

    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, VK_PIPELINE_STAGE_RAYTRACING_BIT_NVX, 0, 1, &memoryBarrier, 0, 0, 0, 0);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo;
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.pWaitSemaphores = nullptr;
    submitInfo.pWaitDstStageMask = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.pSignalSemaphores = nullptr;

    vkQueueSubmit(_queuesInfo.Graphics.Queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(_queuesInfo.Graphics.Queue);
    vkFreeCommandBuffers(_device, _commandPool, 1, &commandBuffer);
}

void vkTracer::CreateDataBuffers() {
    const size_t numMeshes = mGeometryLoader.GetNumMeshes();

    mRTFaceMaterialIDBufferViews.resize(numMeshes);
    for (size_t i = 0; i < numMeshes; ++i) {
        RTGeometry& rtgeom = mRTGeometries[i];

        const uint32_t numFaces = static_cast<uint32_t>(mGeometryLoader.GetNumFaces(i));
        const uint32_t matIDsDataSize = numFaces * sizeof(uint32_t);
        const uint32_t* matIDs = mGeometryLoader.GetFaceMaterialIDs(i);

        VkResult error = rtgeom.matIDs.Create(matIDsDataSize, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(error, L"rt matIDs.Create");
        if (!rtgeom.matIDs.CopyToBufferUsingMapUnmap(matIDs, matIDsDataSize)) {
            ExitError(L"Failed to copy matIDs buffer");
        }

        VkBufferViewCreateInfo bufferViewInfo;
        bufferViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        bufferViewInfo.pNext = nullptr;
        bufferViewInfo.flags = 0;
        bufferViewInfo.buffer = rtgeom.matIDs.Buffer;
        bufferViewInfo.format = VK_FORMAT_R32_UINT;
        bufferViewInfo.offset = 0;
        bufferViewInfo.range = VK_WHOLE_SIZE;

        error = vkCreateBufferView(_device, &bufferViewInfo, nullptr, &mRTFaceMaterialIDBufferViews[i]);
        NVVK_CHECK_ERROR(error, L"vkCreateBufferView");
    }

    const uint32_t numMaterials = static_cast<uint32_t>(mGeometryLoader.GetNumMaterials());
    const uint32_t materialsDataSize = numMaterials * sizeof(Material_s);
    const Material_s* materials = mGeometryLoader.GetMaterials();

    VkResult error = mRTMaterialsBuffer.Create(materialsDataSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    NVVK_CHECK_ERROR(error, L"rt matIDs.Create");
    if (!mRTMaterialsBuffer.CopyToBufferUsingMapUnmap(materials, materialsDataSize)) {
        ExitError(L"Failed to copy matIDs buffer");
    }
}

void vkTracer::CreateDescriptorSetLayouts() {
    const uint32_t numMeshes = static_cast<uint32_t>(mGeometryLoader.GetNumMeshes());

    // First set:
    //  binding 0  ->  AS
    //  binding 1  ->  output image
    //  binding 2  ->  materials SSBO

    VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding;
    accelerationStructureLayoutBinding.binding = 0;
    accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
    accelerationStructureLayoutBinding.descriptorCount = 1;
    accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
    accelerationStructureLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding outputImageLayoutBinding;
    outputImageLayoutBinding.binding = 1;
    outputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageLayoutBinding.descriptorCount = 1;
    outputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
    outputImageLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding materialsBufferLayoutBinding;
    materialsBufferLayoutBinding.binding = 2;
    materialsBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialsBufferLayoutBinding.descriptorCount = 1;
    materialsBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX;
    materialsBufferLayoutBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> bindings({
        accelerationStructureLayoutBinding,
        outputImageLayoutBinding,
        materialsBufferLayoutBinding
    });

    VkDescriptorSetLayoutCreateInfo set1LayoutInfo;
    set1LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set1LayoutInfo.pNext = nullptr;
    set1LayoutInfo.flags = 0;
    set1LayoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    set1LayoutInfo.pBindings = bindings.data();

    VkResult error = vkCreateDescriptorSetLayout(_device, &set1LayoutInfo, nullptr, &mRTDescriptorSetLayouts[0]);
    NVVK_CHECK_ERROR(error, L"vkCreateDescriptorSetLayout");

    
    // Second set:
    //  binding 0 .. N  ->  per-face material IDs for geometries  (N = num geometries)

    const VkDescriptorBindingFlagsEXT flag = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT bindingFlags;
    bindingFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
    bindingFlags.pNext = nullptr;
    bindingFlags.pBindingFlags = &flag;
    bindingFlags.bindingCount = 1;

    VkDescriptorSetLayoutBinding texelBufferBinding;
    texelBufferBinding.binding = 0;
    texelBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    texelBufferBinding.descriptorCount = numMeshes;
    texelBufferBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX;
    texelBufferBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo set2LayoutInfo;
    set2LayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    set2LayoutInfo.pNext = &bindingFlags;
    set2LayoutInfo.flags = 0;
    set2LayoutInfo.bindingCount = 1;
    set2LayoutInfo.pBindings = &texelBufferBinding;

    error = vkCreateDescriptorSetLayout(_device, &set2LayoutInfo, nullptr, &mRTDescriptorSetLayouts[1]);
    NVVK_CHECK_ERROR(error, L"vkCreateDescriptorSetLayout");
}

void vkTracer::CreatePipeline() {
    auto LoadShader = [](ShaderResource& shader, std::wstring shaderName) {
        bool fileError;
        VkResult code = shader.LoadFromFile(shaderName, fileError);
        if (fileError) {
            ExitError(L"Failed to read " + shaderName + L" file");
        }
        NVVK_CHECK_ERROR(code, shaderName);
    };

    ShaderResource rgenShader, chitShader, missShader, ahitShader;
    LoadShader(rgenShader, L"raygen.bin");
    LoadShader(chitShader, L"closest_hit.bin");
    LoadShader(ahitShader, L"any_hit.bin");
    LoadShader(missShader, L"ray_miss.bin");

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages({
        rgenShader.GetShaderStage(VK_SHADER_STAGE_RAYGEN_BIT_NVX),
        chitShader.GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX),
        ahitShader.GetShaderStage(VK_SHADER_STAGE_ANY_HIT_BIT_NVX),
        missShader.GetShaderStage(VK_SHADER_STAGE_MISS_BIT_NVX),
    });

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(mRTDescriptorSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = mRTDescriptorSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    VkResult error = vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &mRTPipelineLayout);
    NVVK_CHECK_ERROR(error, L"rt vkCreatePipelineLayout");

    const uint32_t groupNumbers[] = { 0, 1, 1, 2 };

    // The indices form the following groups:
    // group0 = [ raygen ]
    // group1 = [ chit ][ ahit ]
    // group2 = [ miss ]

    VkRaytracingPipelineCreateInfoNVX rayPipelineInfo;
    rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAYTRACING_PIPELINE_CREATE_INFO_NVX;
    rayPipelineInfo.pNext = nullptr;
    rayPipelineInfo.flags = 0;
    rayPipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    rayPipelineInfo.pStages = shaderStages.data();
    rayPipelineInfo.pGroupNumbers = groupNumbers;
    rayPipelineInfo.maxRecursionDepth = 1;
    rayPipelineInfo.layout = mRTPipelineLayout;
    rayPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    rayPipelineInfo.basePipelineIndex = 0;

    error = vkCreateRaytracingPipelinesNVX(_device, nullptr, 1, &rayPipelineInfo, nullptr, &mRTPipeline);
    NVVK_CHECK_ERROR(error, L"vkCreateRaytracingPipelinesNV");
}

void vkTracer::CreateShaderBindingTable() {
    const uint32_t groupNum = 3; // 3 groups are listed in pGroupNumbers in VkRaytracingPipelineCreateInfoNV
    const uint32_t shaderBindingTableSize = _raytracingProperties.shaderHeaderSize * groupNum;

    VkResult code = mShaderBindingTable.Create(shaderBindingTableSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
    NVVK_CHECK_ERROR(code, L"mShaderBindingTable.Create");

    void* mappedMemory = mShaderBindingTable.Map(shaderBindingTableSize);
    code = vkGetRaytracingShaderHandlesNVX(_device, mRTPipeline, 0, groupNum, shaderBindingTableSize, mappedMemory);
    NVVK_CHECK_ERROR(code, L"vkGetRaytracingShaderHandleNV");
    mShaderBindingTable.Unmap();
}

void vkTracer::CreateDescriptorSets() {
    const uint32_t numGeometries = static_cast<uint32_t>(mRTGeometries.size());

    std::vector<VkDescriptorPoolSize> poolSizes({
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },                    // output image buffer
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX, 1 },       // top-level AS
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },                   // materials
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, numGeometries }  // per-face material IDs for each geometry
    });

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
    descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolCreateInfo.pNext = nullptr;
    descriptorPoolCreateInfo.flags = 0;
    descriptorPoolCreateInfo.maxSets = static_cast<uint32_t>(mRTDescriptorSetLayouts.size());
    descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    descriptorPoolCreateInfo.pPoolSizes = poolSizes.data();

    VkResult error = vkCreateDescriptorPool(_device, &descriptorPoolCreateInfo, nullptr, &mRTDescriptorPool);
    NVVK_CHECK_ERROR(error, L"vkCreateDescriptorPool");

    std::array<uint32_t, 1> variableDescriptorCounts = {
        numGeometries   // per-face material IDs for each geometry
    };

    VkDescriptorSetVariableDescriptorCountAllocateInfoEXT variableDescriptorCountInfo;
    variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT;
    variableDescriptorCountInfo.pNext = nullptr;
    variableDescriptorCountInfo.descriptorSetCount = static_cast<uint32_t>(mRTDescriptorSetLayouts.size());
    variableDescriptorCountInfo.pDescriptorCounts = variableDescriptorCounts.data(); // actual number of descriptors

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.pNext = nullptr;
    descriptorSetAllocateInfo.descriptorPool = mRTDescriptorPool;
    descriptorSetAllocateInfo.descriptorSetCount = static_cast<uint32_t>(mRTDescriptorSetLayouts.size());
    descriptorSetAllocateInfo.pSetLayouts = mRTDescriptorSetLayouts.data();

    error = vkAllocateDescriptorSets(_device, &descriptorSetAllocateInfo, mRTDescriptorSets.data());
    NVVK_CHECK_ERROR(error, L"vkAllocateDescriptorSets");
}

void vkTracer::UpdateDescriptorSets() {
    VkDescriptorAccelerationStructureInfoNVX descriptorAccelerationStructureInfo;
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ACCELERATION_STRUCTURE_INFO_NVX;
    descriptorAccelerationStructureInfo.pNext = nullptr;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &mTopAS;

    VkWriteDescriptorSet accelerationStructureWrite;
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo; // Notice that pNext is assigned here!
    accelerationStructureWrite.dstSet = mRTDescriptorSets[0];
    accelerationStructureWrite.dstBinding = 0;
    accelerationStructureWrite.dstArrayElement = 0;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
    accelerationStructureWrite.pImageInfo = nullptr;
    accelerationStructureWrite.pBufferInfo = nullptr;
    accelerationStructureWrite.pTexelBufferView = nullptr;

    VkDescriptorImageInfo descriptorOutputImageInfo;
    descriptorOutputImageInfo.sampler = nullptr;
    descriptorOutputImageInfo.imageView = _offsreenImageResource.ImageView;
    descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet outputImageWrite;
    outputImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    outputImageWrite.pNext = nullptr;
    outputImageWrite.dstSet = mRTDescriptorSets[0];
    outputImageWrite.dstBinding = 1;
    outputImageWrite.dstArrayElement = 0;
    outputImageWrite.descriptorCount = 1;
    outputImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageWrite.pImageInfo = &descriptorOutputImageInfo;
    outputImageWrite.pBufferInfo = nullptr;
    outputImageWrite.pTexelBufferView = nullptr;

    VkDescriptorBufferInfo descriptorMaterialsBufferInfo;
    descriptorMaterialsBufferInfo.buffer = mRTMaterialsBuffer.Buffer;
    descriptorMaterialsBufferInfo.offset = 0;
    descriptorMaterialsBufferInfo.range = mRTMaterialsBuffer.Size;

    VkWriteDescriptorSet materialsBuffer;
    materialsBuffer.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    materialsBuffer.pNext = nullptr;
    materialsBuffer.dstSet = mRTDescriptorSets[0];
    materialsBuffer.dstBinding = 2;
    materialsBuffer.dstArrayElement = 0;
    materialsBuffer.descriptorCount = 1;
    materialsBuffer.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialsBuffer.pImageInfo = nullptr;
    materialsBuffer.pBufferInfo = &descriptorMaterialsBufferInfo;
    materialsBuffer.pTexelBufferView = nullptr;

    VkWriteDescriptorSet faceMatIDsBuffers;
    faceMatIDsBuffers.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    faceMatIDsBuffers.pNext = nullptr;
    faceMatIDsBuffers.dstSet = mRTDescriptorSets[1];
    faceMatIDsBuffers.dstBinding = 0;
    faceMatIDsBuffers.dstArrayElement = 0;
    faceMatIDsBuffers.descriptorCount = static_cast<uint32_t>(mRTFaceMaterialIDBufferViews.size());
    faceMatIDsBuffers.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    faceMatIDsBuffers.pImageInfo = nullptr;
    faceMatIDsBuffers.pBufferInfo = nullptr;
    faceMatIDsBuffers.pTexelBufferView = mRTFaceMaterialIDBufferViews.data();

    std::vector<VkWriteDescriptorSet> descriptorWrites({
        accelerationStructureWrite,
        outputImageWrite,
        materialsBuffer,
        faceMatIDsBuffers
    });

    vkUpdateDescriptorSets(_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}
