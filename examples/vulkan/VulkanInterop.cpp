
////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#define GLAD_VULKAN_IMPLEMENTATION
#include <vulkan.h>

#include <SFML/Graphics.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace
{
std::filesystem::path resourcesDir()
{
    // Shared with the OpenGL variant of this example
    return "../opengl/resources";
}

// Helper function we pass to GLAD to load Vulkan functions via SFML
GLADapiproc getVulkanFunction(const char* name)
{
    return sf::Vulkan::getFunction(name);
}

// Load a SPIR-V shader binary as the 32-bit words Vulkan consumes
std::vector<std::uint32_t> readSpirv(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return {};

    const auto size = static_cast<std::size_t>(file.tellg());
    file.seekg(0);

    std::vector<std::uint32_t> words((size + 3) / 4);
    file.read(reinterpret_cast<char*>(words.data()), static_cast<std::streamsize>(size));
    return file ? words : std::vector<std::uint32_t>{};
}

using Matrix = std::array<float, 16>; // column-major, like the matrices of the built-in pipeline

////////////////////////////////////////////////////////////
// Column-major matrix helpers for the cube transform
////////////////////////////////////////////////////////////
Matrix multiply(const Matrix& left, const Matrix& right)
{
    Matrix result{};
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            for (int i = 0; i < 4; ++i)
                result[column * 4 + row] += left[i * 4 + row] * right[column * 4 + i];
    return result;
}

Matrix translation(float x, float y, float z)
{
    // clang-format off
    return {1.f, 0.f, 0.f, 0.f,
            0.f, 1.f, 0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            x,   y,   z,   1.f};
    // clang-format on
}

Matrix rotation(float degrees, int axis)
{
    const float radians = degrees * 3.141592654f / 180.f;
    const float c       = std::cos(radians);
    const float s       = std::sin(radians);

    // clang-format off
    if (axis == 0)
        return {1.f, 0.f, 0.f, 0.f,
                0.f, c,   s,   0.f,
                0.f, -s,  c,   0.f,
                0.f, 0.f, 0.f, 1.f};
    if (axis == 1)
        return {c,   0.f, -s,  0.f,
                0.f, 1.f, 0.f, 0.f,
                s,   0.f, c,   0.f,
                0.f, 0.f, 0.f, 1.f};
    return {c,   s,   0.f, 0.f,
            -s,  c,   0.f, 0.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f};
    // clang-format on
}

// The same frustum the Direct3D variant sets up, Vulkan shares its z in [0 .. 1] clip
// space; the upwards y axis is handled by the negative viewport height in draw()
Matrix perspective(float ratio, float zNear, float zFar)
{
    // clang-format off
    return {zNear / ratio, 0.f,   0.f,                           0.f,
            0.f,           zNear, 0.f,                           0.f,
            0.f,           0.f,   zFar / (zNear - zFar),         -1.f,
            0.f,           0.f,   zNear * zFar / (zNear - zFar), 0.f};
    // clang-format on
}

////////////////////////////////////////////////////////////
// Raw Vulkan pipeline drawing a textured, depth-tested cube
// between SFML draws, the Vulkan counterpart of the raw
// Direct3D cube in the direct3d example
////////////////////////////////////////////////////////////
class RawCube
{
public:
    ~RawCube()
    {
        const VkDevice device = sf::Vulkan::getDevice();
        if (!device)
            return;

        // Wait until no submitted frame uses the objects anymore before destroying them
        vkDeviceWaitIdle(device);

        vkDestroyPipeline(device, m_pipeline, nullptr);
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
        vkDestroySampler(device, m_mipmapSampler, nullptr);
        vkDestroySampler(device, m_baseLevelSampler, nullptr);
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
    }

    bool create(VkRenderPass renderPass, const sf::ContextSettings& settings)
    {
        const VkDevice device = sf::Vulkan::getDevice();
        if (!device || !renderPass)
            return false;

        m_hasDepthBuffer = settings.depthBits > 0;

        // The shaders were compiled offline from resources/cube.hlsl, see the comment there
        const std::vector<std::uint32_t> vertexShader   = readSpirv("resources/cube-vs.spv");
        const std::vector<std::uint32_t> fragmentShader = readSpirv("resources/cube-ps.spv");
        if (vertexShader.empty() || fragmentShader.empty())
            return false;

        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.codeSize = vertexShader.size() * sizeof(std::uint32_t);
        moduleInfo.pCode    = vertexShader.data();

        VkShaderModule vertexModule{};
        if (vkCreateShaderModule(device, &moduleInfo, nullptr, &vertexModule) != VK_SUCCESS)
            return false;

        moduleInfo.codeSize = fragmentShader.size() * sizeof(std::uint32_t);
        moduleInfo.pCode    = fragmentShader.data();

        VkShaderModule fragmentModule{};
        if (vkCreateShaderModule(device, &moduleInfo, nullptr, &fragmentModule) != VK_SUCCESS)
        {
            vkDestroyShaderModule(device, vertexModule, nullptr);
            return false;
        }

        const bool created = createPipeline(device, renderPass, settings, vertexModule, fragmentModule) &&
                             createDescriptors(device) && createVertexBuffer(device);

        // The modules are only needed while the pipeline is created
        vkDestroyShaderModule(device, vertexModule, nullptr);
        vkDestroyShaderModule(device, fragmentModule, nullptr);

        return created;
    }

    void draw(VkCommandBuffer commandBuffer, const Matrix& mvp, VkImageView texture, bool mipmapped, sf::Vector2u windowSize)
    {
        // A minimized window reports a zero size, and zero-sized viewports and
        // clear rectangles are invalid in Vulkan
        if (!commandBuffer || !texture || (windowSize.x == 0) || (windowSize.y == 0))
            return;

        // Point the descriptor sets at the texture; regenerating its mipmap re-creates
        // the underlying image, so the view is re-written whenever it changed
        if (texture != m_writtenTexture)
        {
            // A descriptor set must not be re-written while a submitted frame still reads it
            vkDeviceWaitIdle(sf::Vulkan::getDevice());

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageView   = texture;
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            std::array<VkWriteDescriptorSet, 2> writes{};
            for (std::size_t i = 0; i < writes.size(); ++i)
            {
                writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet          = m_descriptorSets[i];
                writes[i].dstBinding      = 0;
                writes[i].descriptorCount = 1;
                writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
                writes[i].pImageInfo      = &imageInfo;
            }

            vkUpdateDescriptorSets(sf::Vulkan::getDevice(),
                                   static_cast<std::uint32_t>(writes.size()),
                                   writes.data(),
                                   0,
                                   nullptr);

            m_writtenTexture = texture;
        }

        // Clear the depth buffer
        if (m_hasDepthBuffer)
        {
            VkClearAttachment clearDepth{};
            clearDepth.aspectMask                      = VK_IMAGE_ASPECT_DEPTH_BIT;
            clearDepth.clearValue.depthStencil.depth   = 1.f;

            VkClearRect clearRect{};
            clearRect.rect.extent = {windowSize.x, windowSize.y};
            clearRect.layerCount  = 1;

            vkCmdClearAttachments(commandBuffer, 1, &clearDepth, 1, &clearRect);
        }

        // Set every state this rendering relies on; unlike Direct3D nothing is inherited
        // from SFML, a Vulkan pipeline carries all of its state itself. The negative
        // height flips the viewport so the y axis points up like it does in Direct3D
        VkViewport viewport{};
        viewport.y        = static_cast<float>(windowSize.y);
        viewport.width    = static_cast<float>(windowSize.x);
        viewport.height   = -static_cast<float>(windowSize.y);
        viewport.maxDepth = 1.f;

        VkRect2D scissor{};
        scissor.extent = {windowSize.x, windowSize.y};

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Matrix), mvp.data());

        const VkDescriptorSet descriptorSet = mipmapped ? m_descriptorSets[0] : m_descriptorSets[1];
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_vertexBuffer, &offset);
        vkCmdDraw(commandBuffer, 36, 1, 0, 0);
    }

private:
    bool createPipeline(VkDevice                   device,
                        VkRenderPass               renderPass,
                        const sf::ContextSettings& settings,
                        VkShaderModule             vertexModule,
                        VkShaderModule             fragmentModule)
    {
        // The texture is bound as a sampled image and a sampler, matching the
        // [[vk::binding]] declarations in the shader source
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        bindings[0].binding         = 0;
        bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        bindings[0].descriptorCount = 1;
        bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        bindings[1].binding         = 1;
        bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
        bindings[1].descriptorCount = 1;
        bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layoutInfo.pBindings    = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &m_descriptorSetLayout) != VK_SUCCESS)
            return false;

        // The transform is passed through push constants, the lightest way to hand a
        // small amount of per-draw data to a shader
        VkPushConstantRange pushConstants{};
        pushConstants.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstants.size       = sizeof(Matrix);

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = 1;
        pipelineLayoutInfo.pSetLayouts            = &m_descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges    = &pushConstants;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
            return false;

        std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertexModule;
        stages[0].pName  = "VSMain";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragmentModule;
        stages[1].pName  = "PSMain";

        // Positions and texture coordinates, tightly packed like the cube array below
        VkVertexInputBindingDescription vertexBinding{};
        vertexBinding.stride = sizeof(float) * 5;

        std::array<VkVertexInputAttributeDescription, 2> vertexAttributes{};
        vertexAttributes[0].location = 0;
        vertexAttributes[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        vertexAttributes[1].location = 1;
        vertexAttributes[1].format   = VK_FORMAT_R32G32_SFLOAT;
        vertexAttributes[1].offset   = sizeof(float) * 3;

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount   = 1;
        vertexInput.pVertexBindingDescriptions      = &vertexBinding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(vertexAttributes.size());
        vertexInput.pVertexAttributeDescriptions    = vertexAttributes.data();

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        // The viewport and scissor are set at draw time so the pipeline survives resizes
        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        const std::array dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates    = dynamicStates.data();

        VkPipelineRasterizationStateCreateInfo rasterization{};
        rasterization.sType     = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterization.cullMode  = VK_CULL_MODE_NONE;
        rasterization.lineWidth = 1.f;

        // The rendering has to run at the sample count of the window it draws into
        VkPipelineMultisampleStateCreateInfo multisample{};
        multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisample.rasterizationSamples = (settings.antiAliasingLevel > 1)
                                               ? static_cast<VkSampleCountFlagBits>(settings.antiAliasingLevel)
                                               : VK_SAMPLE_COUNT_1_BIT;

        // Enable z-buffer read and write
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable  = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp   = VK_COMPARE_OP_LESS;

        // The same alpha blending SFML draws with, so the cube blends like the raw
        // objects of the other variants of this example do
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.blendEnable         = VK_TRUE;
        blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                         VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments    = &blendAttachment;

        // The pipeline renders into the render pass SFML draws the window with; load
        // and store operations do not matter for compatibility, so the pipeline keeps
        // working when SFML opens later passes of the frame with different ones
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = static_cast<std::uint32_t>(stages.size());
        pipelineInfo.pStages             = stages.data();
        pipelineInfo.pVertexInputState   = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterization;
        pipelineInfo.pMultisampleState   = &multisample;
        pipelineInfo.pDepthStencilState  = &depthStencil;
        pipelineInfo.pColorBlendState    = &blend;
        pipelineInfo.pDynamicState       = &dynamicState;
        pipelineInfo.layout              = m_pipelineLayout;
        pipelineInfo.renderPass          = renderPass;

        return vkCreateGraphicsPipelines(device, nullptr, 1, &pipelineInfo, nullptr, &m_pipeline) == VK_SUCCESS;
    }

    bool createDescriptors(VkDevice device)
    {
        // One sampler for mipmapped sampling, one that stays on the base level
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter    = VK_FILTER_LINEAR;
        samplerInfo.minFilter    = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod       = VK_LOD_CLAMP_NONE;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &m_mipmapSampler) != VK_SUCCESS)
            return false;

        samplerInfo.maxLod = 0.f;

        if (vkCreateSampler(device, &samplerInfo, nullptr, &m_baseLevelSampler) != VK_SUCCESS)
            return false;

        // One descriptor set per sampler, both pointing at the same texture
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        poolSizes[0].descriptorCount = 2;
        poolSizes[1].type            = VK_DESCRIPTOR_TYPE_SAMPLER;
        poolSizes[1].descriptorCount = 2;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets       = 2;
        poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes    = poolSizes.data();

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
            return false;

        const std::array setLayouts = {m_descriptorSetLayout, m_descriptorSetLayout};

        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool     = m_descriptorPool;
        allocateInfo.descriptorSetCount = static_cast<std::uint32_t>(setLayouts.size());
        allocateInfo.pSetLayouts        = setLayouts.data();

        if (vkAllocateDescriptorSets(device, &allocateInfo, m_descriptorSets.data()) != VK_SUCCESS)
            return false;

        // The samplers never change, write them once; the texture is written in draw()
        std::array<VkDescriptorImageInfo, 2> samplerInfos{};
        samplerInfos[0].sampler = m_mipmapSampler;
        samplerInfos[1].sampler = m_baseLevelSampler;

        std::array<VkWriteDescriptorSet, 2> writes{};
        for (std::size_t i = 0; i < writes.size(); ++i)
        {
            writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet          = m_descriptorSets[i];
            writes[i].dstBinding      = 1;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
            writes[i].pImageInfo      = &samplerInfos[i];
        }

        vkUpdateDescriptorSets(device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
        return true;
    }

    bool createVertexBuffer(VkDevice device)
    {
        // Define a 3D cube (6 faces made of 2 triangles composed by 3 vertices)
        // clang-format off
        constexpr std::array<float, 180> cube =
        {
            // positions    // texture coordinates
            -20, -20, -20,  0, 0,
            -20,  20, -20,  1, 0,
            -20, -20,  20,  0, 1,
            -20, -20,  20,  0, 1,
            -20,  20, -20,  1, 0,
            -20,  20,  20,  1, 1,

             20, -20, -20,  0, 0,
             20,  20, -20,  1, 0,
             20, -20,  20,  0, 1,
             20, -20,  20,  0, 1,
             20,  20, -20,  1, 0,
             20,  20,  20,  1, 1,

            -20, -20, -20,  0, 0,
             20, -20, -20,  1, 0,
            -20, -20,  20,  0, 1,
            -20, -20,  20,  0, 1,
             20, -20, -20,  1, 0,
             20, -20,  20,  1, 1,

            -20,  20, -20,  0, 0,
             20,  20, -20,  1, 0,
            -20,  20,  20,  0, 1,
            -20,  20,  20,  0, 1,
             20,  20, -20,  1, 0,
             20,  20,  20,  1, 1,

            -20, -20, -20,  0, 0,
             20, -20, -20,  1, 0,
            -20,  20, -20,  0, 1,
            -20,  20, -20,  0, 1,
             20, -20, -20,  1, 0,
             20,  20, -20,  1, 1,

            -20, -20,  20,  0, 0,
             20, -20,  20,  1, 0,
            -20,  20,  20,  0, 1,
            -20,  20,  20,  0, 1,
             20, -20,  20,  1, 0,
             20,  20,  20,  1, 1
        };
        // clang-format on

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size  = sizeof(cube);
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

        if (vkCreateBuffer(device, &bufferInfo, nullptr, &m_vertexBuffer) != VK_SUCCESS)
            return false;

        // Vulkan leaves picking the memory the buffer lives in to the application;
        // host-visible memory can be written directly, coherent memory needs no flush
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, m_vertexBuffer, &requirements);

        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(sf::Vulkan::getPhysicalDevice(), &memoryProperties);

        constexpr VkMemoryPropertyFlags wanted = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

        std::uint32_t memoryType = memoryProperties.memoryTypeCount;
        for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
        {
            if ((requirements.memoryTypeBits & (1u << i)) &&
                ((memoryProperties.memoryTypes[i].propertyFlags & wanted) == wanted))
            {
                memoryType = i;
                break;
            }
        }

        if (memoryType == memoryProperties.memoryTypeCount)
            return false;

        VkMemoryAllocateInfo allocateInfo{};
        allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocateInfo.allocationSize  = requirements.size;
        allocateInfo.memoryTypeIndex = memoryType;

        if ((vkAllocateMemory(device, &allocateInfo, nullptr, &m_vertexBufferMemory) != VK_SUCCESS) ||
            (vkBindBufferMemory(device, m_vertexBuffer, m_vertexBufferMemory, 0) != VK_SUCCESS))
            return false;

        void* mapped = nullptr;
        if (vkMapMemory(device, m_vertexBufferMemory, 0, sizeof(cube), 0, &mapped) != VK_SUCCESS)
            return false;

        std::memcpy(mapped, cube.data(), sizeof(cube));
        vkUnmapMemory(device, m_vertexBufferMemory);
        return true;
    }

    VkPipeline            m_pipeline{};
    VkPipelineLayout      m_pipelineLayout{};
    VkDescriptorSetLayout m_descriptorSetLayout{};
    VkDescriptorPool      m_descriptorPool{};

    std::array<VkDescriptorSet, 2> m_descriptorSets{}; //!< Mipmapped and base-level variant

    VkSampler   m_mipmapSampler{};
    VkSampler   m_baseLevelSampler{};
    VkImageView m_writtenTexture{}; //!< Texture view currently written into the descriptor sets

    VkBuffer       m_vertexBuffer{};
    VkDeviceMemory m_vertexBufferMemory{};

    bool m_hasDepthBuffer{};
};

} // namespace


////////////////////////////////////////////////////////////
/// Entry point of application
///
/// \return Application exit code
///
////////////////////////////////////////////////////////////
int main()
{
    // Select the Vulkan renderer before any graphics resource is created
    sf::setRenderer(sf::Renderer::Vulkan);
    if (sf::getRenderer() != sf::Renderer::Vulkan)
        std::cerr << "Vulkan is not available, running on the default renderer without the raw Vulkan cube" << std::endl;

    bool exit = false;
    bool sRgb = false;

    while (!exit)
    {
        // Request a 24-bits depth buffer when creating the window
        sf::ContextSettings contextSettings;
        contextSettings.depthBits   = 24;
        contextSettings.sRgbCapable = sRgb;

        // Create the main window
        sf::RenderWindow window(sf::VideoMode({800, 600}),
                                "SFML graphics with raw Vulkan",
                                sf::Style::Default,
                                sf::State::Windowed,
                                contextSettings);
        window.setVerticalSyncEnabled(true);
        window.setMinimumSize(sf::Vector2u(400, 300));
        window.setMaximumSize(sf::Vector2u(1200, 900));

        // Create a sprite for the background
        const sf::Texture backgroundTexture(resourcesDir() / "background.jpg", sRgb);
        const sf::Sprite  background(backgroundTexture);

        // Create some text to draw on top of our Vulkan object
        const sf::Font font(resourcesDir() / "tuffy.ttf");

        sf::Text text(font, "SFML / Vulkan demo");
        sf::Text sRgbInstructions(font, "Press space to toggle sRGB conversion");
        sf::Text mipmapInstructions(font, "Press return to toggle mipmapping");
        text.setFillColor(sf::Color(255, 255, 255, 170));
        sRgbInstructions.setFillColor(sf::Color(255, 255, 255, 170));
        mipmapInstructions.setFillColor(sf::Color(255, 255, 255, 170));
        text.setPosition({280.f, 450.f});
        sRgbInstructions.setPosition({175.f, 500.f});
        mipmapInstructions.setPosition({200.f, 550.f});

        // Load a texture to apply to our 3D cube
        sf::Texture texture(resourcesDir() / "logo.png");

        // Attempt to generate a mipmap for our cube texture
        // We don't check the return value here since
        // mipmapping is purely optional in this example
        (void)texture.generateMipmap();

        // Create the raw Vulkan cube pipeline against the render pass drawing into the
        // window, which requires the window to be set up as the active target first
        RawCube rawCube;
        bool    rawCubeReady = false;

        if (sf::Vulkan::getDevice())
        {
            // Load the Vulkan entry points via SFML's own loader
            gladLoadVulkan({}, getVulkanFunction);
            gladLoadVulkan(sf::Vulkan::getPhysicalDevice(), getVulkanFunction);

            if (window.setActive(true))
                rawCubeReady = rawCube.create(sf::Vulkan::getRenderPass(), window.getSettings());
        }

        // Create a clock for measuring the time elapsed
        const sf::Clock clock;

        // Flag to track whether mipmapping is currently enabled
        bool mipmapEnabled = true;

        // Start game loop
        while (window.isOpen())
        {
            // Process events
            while (const std::optional event = window.pollEvent())
            {
                // Window closed or escape key pressed: exit
                if (event->is<sf::Event::Closed>() ||
                    (event->is<sf::Event::KeyPressed>() &&
                     event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Escape))
                {
                    exit = true;
                    window.close();
                }

                // Return key: toggle mipmapping
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>();
                    keyPressed && keyPressed->code == sf::Keyboard::Key::Enter)
                {
                    if (mipmapEnabled)
                    {
                        // We simply reload the texture to disable mipmapping
                        texture = sf::Texture(resourcesDir() / "logo.png");

                        mipmapEnabled = false;
                    }
                    else if (texture.generateMipmap())
                    {
                        mipmapEnabled = true;
                    }
                }

                // Space key: toggle sRGB conversion
                if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>();
                    keyPressed && keyPressed->code == sf::Keyboard::Key::Space)
                {
                    sRgb = !sRgb;
                    window.close();
                }

                // Adjust the background view when the window is resized
                if (event->is<sf::Event::Resized>())
                {
                    const sf::Vector2u textureSize = backgroundTexture.getSize();

                    sf::View view;
                    view.setSize(sf::Vector2f(textureSize));
                    view.setCenter(sf::Vector2f(textureSize) / 2.f);
                    window.setView(view);
                }
            }

            // Draw the background
            window.draw(background);

            if (rawCubeReady)
            {
                // We get the position of the mouse cursor, so that we can move the box accordingly
                const sf::Vector2i pos = sf::Mouse::getPosition(window);

                const float x = static_cast<float>(pos.x) * 200.f / static_cast<float>(window.getSize().x) - 100.f;
                const float y = -static_cast<float>(pos.y) * 200.f / static_cast<float>(window.getSize().y) + 100.f;

                // Apply some transformations
                const float seconds = clock.getElapsedTime().asSeconds();
                const float ratio   = static_cast<float>(window.getSize().x) / static_cast<float>(window.getSize().y);

                Matrix modelViewProjection = perspective(ratio, 1.f, 500.f);
                modelViewProjection        = multiply(modelViewProjection, translation(x, y, -100.f));
                modelViewProjection        = multiply(modelViewProjection, rotation(seconds * 50.f, 0));
                modelViewProjection        = multiply(modelViewProjection, rotation(seconds * 30.f, 1));
                modelViewProjection        = multiply(modelViewProjection, rotation(seconds * 90.f, 2));

                // Make sure the SFML draws above are recorded before our own commands,
                // guarantee an open render pass to record into, then draw the cube with
                // raw Vulkan calls and hand the pipeline back to SFML
                sf::Vulkan::flush();
                if (sf::Vulkan::getRenderPass())
                    rawCube.draw(sf::Vulkan::getCommandBuffer(),
                                 modelViewProjection,
                                 sf::Vulkan::getImageView(texture),
                                 mipmapEnabled,
                                 window.getSize());
                sf::Vulkan::resetStates(window);
            }

            // Draw some text on top of our Vulkan object
            window.draw(text);
            window.draw(sRgbInstructions);
            window.draw(mipmapInstructions);

            // Finally, display the rendered frame on screen
            window.display();
        }
    }

    return EXIT_SUCCESS;
}
