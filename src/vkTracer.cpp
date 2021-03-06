#include "vkTracer.h"

#include <chrono>
static uint64_t GetCurrentTimeMilliseconds() {
    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
    std::chrono::system_clock::time_point::duration epoch = tp.time_since_epoch();

    std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(epoch);

    return static_cast<uint64_t>(ms.count());
}

vkTracer::vkTracer()
    : mTopASMemory(VK_NULL_HANDLE)
    , mTopAS(VK_NULL_HANDLE)
    , mRTPipelineLayout(VK_NULL_HANDLE)
    , mRTPipeline(VK_NULL_HANDLE)
    , mRTDescriptorPool(VK_NULL_HANDLE)
    , mRTDescriptorSetLayouts({ VK_NULL_HANDLE })
    , mRTDescriptorSets({ VK_NULL_HANDLE })
    , mRTFaceMaterialIDBufferViews({ VK_NULL_HANDLE })
    , mRTFacesBufferViews({ VK_NULL_HANDLE })
    , mRTNormalsBufferViews({ VK_NULL_HANDLE })
    , mCursorPos(0.0f, 0.0f)
{
    _appName = L"vkTracer";
    _shadersFolder = L"/_data/shaders/";
    _imagesFolder = L"/_data/textures/";
    _geometryFolder = L"/_data/geometries/";

    _deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    _deviceExtensions.push_back(VK_NVX_RAYTRACING_EXTENSION_NAME);
    _deviceExtensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

    _settings.DesiredWindowWidth = 1280;
    _settings.DesiredWindowHeight = 720;
    _settings.ValidationEnabled = true;
}

vkTracer::~vkTracer() {

}

void vkTracer::Init() {
    this->InitRaytracing();

    this->CreateCamera();
    this->LoadIBLTexture();
    this->CreateAccelerationStructures();
    this->CreateSceneShaderData();
    this->CreateDescriptorSetLayouts();
    this->CreatePipeline();
    this->CreateShaderBindingTable();
    this->CreateDescriptorSets();
    this->UpdateDescriptorSets();
}

void vkTracer::RecordCommandBufferForFrame(VkCommandBuffer commandBuffer, uint32_t) {
    vkCmdBindPipeline(commandBuffer,
                      VK_PIPELINE_BIND_POINT_RAYTRACING_NVX,
                      mRTPipeline);

    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_RAYTRACING_NVX,
                            mRTPipelineLayout, 0,
                            static_cast<uint32_t>(mRTDescriptorSets.size()), mRTDescriptorSets.data(),
                            0, 0);

    // Here's how the shader binding table looks like in our case
    // |[ raygen shader ]|[ hit shader 0 ][ hit shaders 1 ]|[ miss shader 0 ][ miss shader 1 ]|
    // |                 |                                 |                                  |
    // | 0               | 1               2               | 3                4               | 5

    vkCmdTraceRaysNVX(commandBuffer,
        mShaderBindingTable.Buffer, 0,
        mShaderBindingTable.Buffer, 3 * _raytracingProperties.shaderHeaderSize, _raytracingProperties.shaderHeaderSize,
        mShaderBindingTable.Buffer, 1 * _raytracingProperties.shaderHeaderSize, _raytracingProperties.shaderHeaderSize,
        _actualWindowWidth, _actualWindowHeight);
}

void vkTracer::UpdateDataForFrame(uint32_t) {
    static uint64_t lastTime = 0;

    const uint64_t curTime = GetCurrentTimeMilliseconds();
    if (!lastTime) {
        lastTime = curTime;
    }
    const float dt = static_cast<float>(curTime - lastTime) / 1000.0f;
    lastTime = curTime;

    // upload camera
    this->UpdateCamera(dt);
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

void vkTracer::CreateCamera() {
    VkResult error = mCamDataBuffer.Create(sizeof(CamData_s), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    NVVK_CHECK_ERROR(error, L"rt mCamDataBuffer.Create");

    mCamera.SetViewport({ 0, 0, static_cast<int>(_actualWindowWidth), static_cast<int>(_actualWindowHeight) });
    mCamera.SetViewPlanes(0.01f, 5000.0f);
    mCamera.SetFovY(45.0f);
    mCamera.LookAt(vec3(155.15f, 297.802f, 0.0f), vec3(8.0f, 20.0f, -250.0f));
}

void vkTracer::UpdateCamera(const float dt) {
    static const float sMoveSpeed = 35.0f;
    static const float sAccelMult = 10.0f;
    static const float sRotateSpeed = 5.0f;

    const bool keyWDown = (0 != (::GetKeyState('W') & 0x8000));
    const bool keySDown = (0 != (::GetKeyState('S') & 0x8000));
    const bool keyADown = (0 != (::GetKeyState('A') & 0x8000));
    const bool keyDDown = (0 != (::GetKeyState('D') & 0x8000));

    const bool keyShiftDown = (0 != (::GetKeyState(VK_SHIFT) & 0x8000));

    const bool lmbDown = (0 != (::GetKeyState(VK_LBUTTON) & 0x8000));

    POINT pt;
    ::GetCursorPos(&pt);

    vec2 newPos(static_cast<float>(pt.x), static_cast<float>(pt.y));
    vec2 delta = (mCursorPos - newPos) * sRotateSpeed * dt;

    if (lmbDown) {
        mCamera.Rotate(delta.x, delta.y);
    }

    mCursorPos = newPos;

    vec2 moveDelta(0.0f, 0.0f);
    if (keyWDown) {
        moveDelta.y += 1.0f;
    }
    if (keySDown) {
        moveDelta.y -= 1.0f;
    }
    if (keyADown) {
        moveDelta.x -= 1.0f;
    }
    if (keyDDown) {
        moveDelta.x += 1.0f;
    }

    moveDelta *= sMoveSpeed * dt * (keyShiftDown ? sAccelMult : 1.0f);
    mCamera.Move(moveDelta.x, moveDelta.y);

    CamData_s camData;
    camData.pos = vec4(mCamera.GetPosition(), 0.0f);
    camData.dir = vec4(mCamera.GetDirection(), 0.0f);
    camData.up = vec4(mCamera.GetUp(), 0.0f);
    camData.side = vec4(mCamera.GetSide(), 0.0f);
    camData.nearFarFov = vec4(0.01f, 5000.0f, Deg2Rad(45.0f), 0.0f);

    if (!mCamDataBuffer.CopyToBufferUsingMapUnmap(&camData, sizeof(camData))) {
        ExitError(L"Failed to update mCamDataBuffer");
    }
}


void vkTracer::LoadIBLTexture() {
    VkResult error;
    if (!mIBLTexture.LoadTexture2DFromFile(L"goegap_2k.hdr", error)) {
        ExitError(L"Failed to load IBL texture");
    }
    NVVK_CHECK_ERROR(error, L"Loading IBL");

    VkImageSubresourceRange subresourceRange;
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    subresourceRange.baseArrayLayer = 0;
    subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    error = mIBLTexture.CreateImageView(VK_IMAGE_VIEW_TYPE_2D, mIBLTexture.Format, subresourceRange);
    NVVK_CHECK_ERROR(error, L"Failed to create image view.");

    error = mIBLTexture.CreateSampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    NVVK_CHECK_ERROR(error, L"Failed to create sampler.");
}

void vkTracer::CreateAccelerationStructures() {
    const float transform[12] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
    };

    std::wstring sceneFileName = _basePath + _geometryFolder + L"OrganodronCity/Organodron_City.obj";//L"cornellbox.obj";

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

    auto CreateRTGeometry = [&](const GeometryLoader& loader, size_t meshIdx, RTGeometry& rtgeom) {
        const uint32_t numVertices = static_cast<uint32_t>(mGeometryLoader.GetNumVertices(meshIdx));
        const uint32_t positionsDataSize = numVertices * sizeof(vec3);
        const uint32_t normalsDataSize = numVertices * sizeof(vec3);
        const uint32_t uvsDataSize = numVertices * sizeof(vec2);

        const uint32_t numFaces = static_cast<uint32_t>(mGeometryLoader.GetNumFaces(meshIdx));
        const uint32_t numIndices = numFaces * 3;
        const uint32_t indicesDataSize = numIndices * sizeof(uint32_t);

        const uint32_t matIDsDataSize = numFaces * sizeof(uint32_t);

        const vec3* positions = mGeometryLoader.GetPositions(meshIdx);
        const vec3* normals = mGeometryLoader.GetNormals(meshIdx);
        const vec3* uvs = mGeometryLoader.GetNormals(meshIdx);
        const Face* faces = mGeometryLoader.GetFaces(meshIdx);
        const uint32_t* matIDs = mGeometryLoader.GetFaceMaterialIDs(meshIdx);

        VkResult error = rtgeom.matIDs.Create(matIDsDataSize, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(error, L"rt matIDs.Create");
        if (!rtgeom.matIDs.CopyToBufferUsingMapUnmap(matIDs, matIDsDataSize)) {
            ExitError(L"Failed to copy matIDs buffer");
        }

        error = rtgeom.positions.Create(positionsDataSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(error, L"rt positions.Create");
        if (!rtgeom.positions.CopyToBufferUsingMapUnmap(positions, positionsDataSize)) {
            ExitError(L"Failed to copy positions buffer");
        }

        error = rtgeom.normals.Create(normalsDataSize, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(error, L"rt normals.Create");
        if (!rtgeom.normals.CopyToBufferUsingMapUnmap(normals, normalsDataSize)) {
            ExitError(L"Failed to copy normals buffer");
        }

        error = rtgeom.uvs.Create(uvsDataSize, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(error, L"rt uvs.Create");
        if (!rtgeom.uvs.CopyToBufferUsingMapUnmap(uvs, uvsDataSize)) {
            ExitError(L"Failed to copy uvs buffer");
        }

        error = rtgeom.ib.Create(indicesDataSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(error, L"rt ib.Create");
        if (!rtgeom.ib.CopyToBufferUsingMapUnmap(faces, indicesDataSize)) {
            ExitError(L"Failed to copy index buffer");
        }

        error = rtgeom.faces.Create(indicesDataSize, VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        NVVK_CHECK_ERROR(error, L"rt ib.Create");
        if (!rtgeom.faces.CopyToBufferUsingMapUnmap(faces, indicesDataSize)) {
            ExitError(L"Failed to copy index buffer");
        }

        VkGeometryNVX& vkgeo = rtgeom.vkgeo;

        vkgeo.sType = VK_STRUCTURE_TYPE_GEOMETRY_NVX;
        vkgeo.pNext = nullptr;
        vkgeo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_NVX;
        vkgeo.geometry.triangles.sType = VK_STRUCTURE_TYPE_GEOMETRY_TRIANGLES_NVX;
        vkgeo.geometry.triangles.pNext = nullptr;
        vkgeo.geometry.triangles.vertexData = rtgeom.positions.Buffer;
        vkgeo.geometry.triangles.vertexOffset = 0;
        vkgeo.geometry.triangles.vertexCount = numVertices;
        vkgeo.geometry.triangles.vertexStride = sizeof(vec3);
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
    };

    const size_t numMeshes = mGeometryLoader.GetNumMeshes();

    mRTGeometries.resize(numMeshes);
    std::vector<VkGeometryInstance> instances(numMeshes);

    for (size_t i = 0; i < numMeshes; ++i) {
        RTGeometry& rtgeom = mRTGeometries[i];

        CreateRTGeometry(mGeometryLoader, i, rtgeom);

        uint64_t accelerationStructureHandle;
        VkResult error = vkGetAccelerationStructureHandleNVX(_device, rtgeom.as, sizeof(uint64_t), &accelerationStructureHandle);
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

void vkTracer::CreateSceneShaderData() {
    const size_t numMeshes = mGeometryLoader.GetNumMeshes();

    mRTFaceMaterialIDBufferViews.resize(numMeshes);
    mRTFacesBufferViews.resize(numMeshes);
    mRTNormalsBufferViews.resize(numMeshes);

    for (size_t i = 0; i < numMeshes; ++i) {
        RTGeometry& rtgeom = mRTGeometries[i];

        VkBufferViewCreateInfo bufferViewInfo;
        bufferViewInfo.sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
        bufferViewInfo.pNext = nullptr;
        bufferViewInfo.flags = 0;
        bufferViewInfo.buffer = rtgeom.matIDs.Buffer;
        bufferViewInfo.format = VK_FORMAT_R32_UINT;
        bufferViewInfo.offset = 0;
        bufferViewInfo.range = VK_WHOLE_SIZE;

        VkResult error = vkCreateBufferView(_device, &bufferViewInfo, nullptr, &mRTFaceMaterialIDBufferViews[i]);
        NVVK_CHECK_ERROR(error, L"vkCreateBufferView");

        bufferViewInfo.buffer = rtgeom.faces.Buffer;
        bufferViewInfo.format = VK_FORMAT_R32G32B32_UINT;

        error = vkCreateBufferView(_device, &bufferViewInfo, nullptr, &mRTFacesBufferViews[i]);
        NVVK_CHECK_ERROR(error, L"vkCreateBufferView");

        bufferViewInfo.buffer = rtgeom.normals.Buffer;
        bufferViewInfo.format = VK_FORMAT_R32G32B32_SFLOAT;

        error = vkCreateBufferView(_device, &bufferViewInfo, nullptr, &mRTNormalsBufferViews[i]);
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
    //  binding 2  ->  Camera data
    //  binding 3  ->  IBL texture
    //  binding 4  ->  materials SSBO

    VkDescriptorSetLayoutBinding accelerationStructureLayoutBinding;
    accelerationStructureLayoutBinding.binding = SWS_SCENE_AS_BINDING;
    accelerationStructureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
    accelerationStructureLayoutBinding.descriptorCount = 1;
    accelerationStructureLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX | VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX;
    accelerationStructureLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding outputImageLayoutBinding;
    outputImageLayoutBinding.binding = SWS_OUT_IMAGE_BINDING;
    outputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageLayoutBinding.descriptorCount = 1;
    outputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
    outputImageLayoutBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding camdataBufferBinding;
    camdataBufferBinding.binding = SWS_CAMDATA_BINDING;
    camdataBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    camdataBufferBinding.descriptorCount = 1;
    camdataBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_NVX;
    camdataBufferBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding iblTextureBinding;
    iblTextureBinding.binding = SWS_IBL_BINDING;
    iblTextureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    iblTextureBinding.descriptorCount = 1;
    iblTextureBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX | VK_SHADER_STAGE_MISS_BIT_NVX;
    iblTextureBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutBinding materialsBufferLayoutBinding;
    materialsBufferLayoutBinding.binding = SWS_MATERIALS_BINDING;
    materialsBufferLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialsBufferLayoutBinding.descriptorCount = 1;
    materialsBufferLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX;
    materialsBufferLayoutBinding.pImmutableSamplers = nullptr;

    std::vector<VkDescriptorSetLayoutBinding> bindings({
        accelerationStructureLayoutBinding,
        outputImageLayoutBinding,
        camdataBufferBinding,
        iblTextureBinding,
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

    // Third set:
    //  binding 0 .. N  ->  faces for geometries  (N = num geometries)
    //   (re-using second's set info)

    error = vkCreateDescriptorSetLayout(_device, &set2LayoutInfo, nullptr, &mRTDescriptorSetLayouts[2]);
    NVVK_CHECK_ERROR(error, L"vkCreateDescriptorSetLayout");

    // Fourth set:
    //  binding 0 .. N  ->  normals for geometries  (N = num geometries)
    //   (re-using second's set info)

    error = vkCreateDescriptorSetLayout(_device, &set2LayoutInfo, nullptr, &mRTDescriptorSetLayouts[3]);
    NVVK_CHECK_ERROR(error, L"vkCreateDescriptorSetLayout");
}

void vkTracer::CreatePipeline() {
    VkResult error;

    auto LoadShader = [](ShaderResource& shader, std::wstring shaderName) {
        bool fileError;
        VkResult code = shader.LoadFromFile(shaderName, fileError);
        if (fileError) {
            ExitError(L"Failed to read " + shaderName + L" file");
        }
        NVVK_CHECK_ERROR(code, shaderName);
    };

    ShaderResource raygen,
                   r0_chit,
                   r0_miss,
                   r1_chit,
                   r1_ahit,
                   r1_miss;
    LoadShader(raygen,  L"raygen.bin");
    LoadShader(r0_chit, L"r0_chit.bin");
    LoadShader(r0_miss, L"r0_miss.bin");
    LoadShader(r1_chit, L"r1_chit.bin");
    LoadShader(r1_ahit, L"r1_ahit.bin");
    LoadShader(r1_miss, L"r1_miss.bin");

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages({
        raygen.GetShaderStage(VK_SHADER_STAGE_RAYGEN_BIT_NVX),
        r0_chit.GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX),
        r1_chit.GetShaderStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_NVX),
        r1_ahit.GetShaderStage(VK_SHADER_STAGE_ANY_HIT_BIT_NVX),
        r0_miss.GetShaderStage(VK_SHADER_STAGE_MISS_BIT_NVX),
        r1_miss.GetShaderStage(VK_SHADER_STAGE_MISS_BIT_NVX)
    });

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.pNext = nullptr;
    pipelineLayoutCreateInfo.flags = 0;
    pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(mRTDescriptorSetLayouts.size());
    pipelineLayoutCreateInfo.pSetLayouts = mRTDescriptorSetLayouts.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
    pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

    error = vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &mRTPipelineLayout);
    NVVK_CHECK_ERROR(error, L"rt vkCreatePipelineLayout");

    const uint32_t groupNumbers[] = { 0, 1, 2, 2, 3, 4 };

    // The indices form the following groups:
    // group0 = [ raygen ]
    // group1 = [ r0_chit ]
    // group2 = [ r1_chit, r1_ahit ]
    // group3 = [ r0_miss ]
    // group4 = [ r1_miss ]

    VkRaytracingPipelineCreateInfoNVX rayPipelineInfo;
    rayPipelineInfo.sType = VK_STRUCTURE_TYPE_RAYTRACING_PIPELINE_CREATE_INFO_NVX;
    rayPipelineInfo.pNext = nullptr;
    rayPipelineInfo.flags = 0;
    rayPipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    rayPipelineInfo.pStages = shaderStages.data();
    rayPipelineInfo.pGroupNumbers = groupNumbers;
    rayPipelineInfo.maxRecursionDepth = SWS_MAX_RECURSION;
    rayPipelineInfo.layout = mRTPipelineLayout;
    rayPipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    rayPipelineInfo.basePipelineIndex = 0;

    error = vkCreateRaytracingPipelinesNVX(_device, nullptr, 1, &rayPipelineInfo, nullptr, &mRTPipeline);
    NVVK_CHECK_ERROR(error, L"vkCreateRaytracingPipelinesNV");
}

void vkTracer::CreateShaderBindingTable() {
    const uint32_t groupNum = 5; // 5 groups are listed in pGroupNumbers in VkRaytracingPipelineCreateInfoNV
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
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },                   // Camera data
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },           // IBL texture
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },                   // materials
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, numGeometries }, // per-face material IDs for each geometry
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, numGeometries }, // faces buffer for each geometry
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, numGeometries }  // normals buffer for each geometry
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

    std::array<uint32_t, 3> variableDescriptorCounts = {
        numGeometries,  // per-face material IDs for each geometry
        numGeometries,  // faces buffer for each geometry
        numGeometries   // normals buffer for each geometry
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
    ///////////////////////////////////////////////////////////

    VkDescriptorAccelerationStructureInfoNVX descriptorAccelerationStructureInfo;
    descriptorAccelerationStructureInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ACCELERATION_STRUCTURE_INFO_NVX;
    descriptorAccelerationStructureInfo.pNext = nullptr;
    descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
    descriptorAccelerationStructureInfo.pAccelerationStructures = &mTopAS;

    VkWriteDescriptorSet accelerationStructureWrite;
    accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo; // Notice that pNext is assigned here!
    accelerationStructureWrite.dstSet = mRTDescriptorSets[SWS_SCENE_AS_SET];
    accelerationStructureWrite.dstBinding = SWS_SCENE_AS_BINDING;
    accelerationStructureWrite.dstArrayElement = 0;
    accelerationStructureWrite.descriptorCount = 1;
    accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_NVX;
    accelerationStructureWrite.pImageInfo = nullptr;
    accelerationStructureWrite.pBufferInfo = nullptr;
    accelerationStructureWrite.pTexelBufferView = nullptr;

    ///////////////////////////////////////////////////////////

    VkDescriptorImageInfo descriptorOutputImageInfo;
    descriptorOutputImageInfo.sampler = nullptr;
    descriptorOutputImageInfo.imageView = _offsreenImageResource.ImageView;
    descriptorOutputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet outputImageWrite;
    outputImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    outputImageWrite.pNext = nullptr;
    outputImageWrite.dstSet = mRTDescriptorSets[SWS_OUT_IMAGE_SET];
    outputImageWrite.dstBinding = SWS_OUT_IMAGE_BINDING;
    outputImageWrite.dstArrayElement = 0;
    outputImageWrite.descriptorCount = 1;
    outputImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    outputImageWrite.pImageInfo = &descriptorOutputImageInfo;
    outputImageWrite.pBufferInfo = nullptr;
    outputImageWrite.pTexelBufferView = nullptr;

    ///////////////////////////////////////////////////////////

    VkDescriptorBufferInfo camdataBufferInfo;
    camdataBufferInfo.buffer = mCamDataBuffer.Buffer;
    camdataBufferInfo.offset = 0;
    camdataBufferInfo.range = mCamDataBuffer.Size;

    VkWriteDescriptorSet camdataBufferWrite;
    camdataBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    camdataBufferWrite.pNext = nullptr;
    camdataBufferWrite.dstSet = mRTDescriptorSets[SWS_CAMDATA_SET];
    camdataBufferWrite.dstBinding = SWS_CAMDATA_BINDING;
    camdataBufferWrite.dstArrayElement = 0;
    camdataBufferWrite.descriptorCount = 1;
    camdataBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    camdataBufferWrite.pImageInfo = nullptr;
    camdataBufferWrite.pBufferInfo = &camdataBufferInfo;
    camdataBufferWrite.pTexelBufferView = nullptr;

    ///////////////////////////////////////////////////////////

    VkDescriptorImageInfo descriptorIBLTextureInfo;
    descriptorIBLTextureInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    descriptorIBLTextureInfo.imageView = mIBLTexture.ImageView;
    descriptorIBLTextureInfo.sampler = mIBLTexture.Sampler;

    VkWriteDescriptorSet iblTextureWrite;
    iblTextureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    iblTextureWrite.pNext = nullptr;
    iblTextureWrite.dstSet = mRTDescriptorSets[SWS_IBL_SET];
    iblTextureWrite.dstBinding = SWS_IBL_BINDING;
    iblTextureWrite.dstArrayElement = 0;
    iblTextureWrite.descriptorCount = 1;
    iblTextureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    iblTextureWrite.pImageInfo = &descriptorIBLTextureInfo;
    iblTextureWrite.pBufferInfo = nullptr;
    iblTextureWrite.pTexelBufferView = nullptr;

    ///////////////////////////////////////////////////////////

    VkDescriptorBufferInfo descriptorMaterialsBufferInfo;
    descriptorMaterialsBufferInfo.buffer = mRTMaterialsBuffer.Buffer;
    descriptorMaterialsBufferInfo.offset = 0;
    descriptorMaterialsBufferInfo.range = mRTMaterialsBuffer.Size;

    VkWriteDescriptorSet materialsBufferWrite;
    materialsBufferWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    materialsBufferWrite.pNext = nullptr;
    materialsBufferWrite.dstSet = mRTDescriptorSets[SWS_MATERIALS_SET];
    materialsBufferWrite.dstBinding = SWS_MATERIALS_BINDING;
    materialsBufferWrite.dstArrayElement = 0;
    materialsBufferWrite.descriptorCount = 1;
    materialsBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialsBufferWrite.pImageInfo = nullptr;
    materialsBufferWrite.pBufferInfo = &descriptorMaterialsBufferInfo;
    materialsBufferWrite.pTexelBufferView = nullptr;

    ///////////////////////////////////////////////////////////

    VkWriteDescriptorSet faceMatIDsBuffersWrite;
    faceMatIDsBuffersWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    faceMatIDsBuffersWrite.pNext = nullptr;
    faceMatIDsBuffersWrite.dstSet = mRTDescriptorSets[SWS_FACEMATIDS_SET];
    faceMatIDsBuffersWrite.dstBinding = 0;
    faceMatIDsBuffersWrite.dstArrayElement = 0;
    faceMatIDsBuffersWrite.descriptorCount = static_cast<uint32_t>(mRTFaceMaterialIDBufferViews.size());
    faceMatIDsBuffersWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    faceMatIDsBuffersWrite.pImageInfo = nullptr;
    faceMatIDsBuffersWrite.pBufferInfo = nullptr;
    faceMatIDsBuffersWrite.pTexelBufferView = mRTFaceMaterialIDBufferViews.data();

    ///////////////////////////////////////////////////////////

    VkWriteDescriptorSet facesBuffersWrite;
    facesBuffersWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    facesBuffersWrite.pNext = nullptr;
    facesBuffersWrite.dstSet = mRTDescriptorSets[SWS_FACES_SET];
    facesBuffersWrite.dstBinding = 0;
    facesBuffersWrite.dstArrayElement = 0;
    facesBuffersWrite.descriptorCount = static_cast<uint32_t>(mRTFacesBufferViews.size());
    facesBuffersWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    facesBuffersWrite.pImageInfo = nullptr;
    facesBuffersWrite.pBufferInfo = nullptr;
    facesBuffersWrite.pTexelBufferView = mRTFacesBufferViews.data();

    ///////////////////////////////////////////////////////////

    VkWriteDescriptorSet normalsBuffersWrite;
    normalsBuffersWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    normalsBuffersWrite.pNext = nullptr;
    normalsBuffersWrite.dstSet = mRTDescriptorSets[SWS_NORMALS_SET];
    normalsBuffersWrite.dstBinding = 0;
    normalsBuffersWrite.dstArrayElement = 0;
    normalsBuffersWrite.descriptorCount = static_cast<uint32_t>(mRTNormalsBufferViews.size());
    normalsBuffersWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
    normalsBuffersWrite.pImageInfo = nullptr;
    normalsBuffersWrite.pBufferInfo = nullptr;
    normalsBuffersWrite.pTexelBufferView = mRTNormalsBufferViews.data();

    ///////////////////////////////////////////////////////////

    std::vector<VkWriteDescriptorSet> descriptorWrites({
        accelerationStructureWrite,
        outputImageWrite,
        camdataBufferWrite,
        iblTextureWrite,
        materialsBufferWrite,
        faceMatIDsBuffersWrite,
        facesBuffersWrite,
        normalsBuffersWrite
    });

    vkUpdateDescriptorSets(_device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}
