#pragma once

#include "ikernel_runner.hpp"
#include "vulkan_memory.hpp"
#include <chrono>
#include <map>

namespace kerntopia {

// Forward declarations for Vulkan types
struct VulkanContext;
struct VulkanDevice;
struct VulkanComputePipeline;
struct VulkanCommandPool;
struct VulkanQueryPool;

// Memory classes are now defined in vulkan_memory.hpp

/**
 * @brief Vulkan backend kernel runner implementation
 */
class VulkanKernelRunner : public IKernelRunner {
public:
    VulkanKernelRunner(const DeviceInfo& device_info);
    ~VulkanKernelRunner();
    
    // IKernelRunner interface implementation
    std::string GetBackendName() const override { return "VULKAN"; }
    std::string GetDeviceName() const override;
    DeviceInfo GetDeviceInfo() const override;
    
    Result<void> LoadKernel(const std::vector<uint8_t>& bytecode, const std::string& entry_point) override;
    Result<void> SetParameters(const void* params, size_t size) override;
    Result<void> SetBuffer(int binding, std::shared_ptr<IBuffer> buffer) override;
    Result<void> SetTexture(int binding, std::shared_ptr<ITexture> texture) override;
    Result<void> Dispatch(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z) override;
    Result<void> WaitForCompletion() override;
    TimingResults GetLastExecutionTime() override;
    Result<std::shared_ptr<IBuffer>> CreateBuffer(size_t size, IBuffer::Type type, IBuffer::Usage usage) override;
    Result<std::shared_ptr<ITexture>> CreateTexture(const TextureDesc& desc) override;
    void CalculateDispatchSize(uint32_t width, uint32_t height, uint32_t depth,
                              uint32_t& groups_x, uint32_t& groups_y, uint32_t& groups_z) override;
    std::string GetDebugInfo() const override;
    bool SupportsFeature(const std::string& feature) const override;
    Result<void> SetSlangGlobalParameters(const void* params, size_t size) override;

private:
    std::unique_ptr<VulkanContext> context_;
    std::unique_ptr<VulkanDevice> device_;
    std::unique_ptr<VulkanComputePipeline> pipeline_;
    std::unique_ptr<VulkanCommandPool> command_pool_;
    std::unique_ptr<VulkanQueryPool> query_pool_;
    
    // Resource bindings
    std::map<int, std::shared_ptr<IBuffer>> bound_buffers_;
    std::map<int, std::shared_ptr<ITexture>> bound_textures_;
    std::vector<uint8_t> parameter_data_;
    std::string entry_point_; // Store shader entry point for pipeline creation
    
    // Timing
    std::chrono::high_resolution_clock::time_point dispatch_start_;
    std::chrono::high_resolution_clock::time_point dispatch_end_;
    TimingResults last_timing_;
    
    bool InitializeVulkan(const DeviceInfo& device_info);
    void ShutdownVulkan();
    Result<void> CreateComputePipeline();
    Result<void> CreateDescriptorSets();
    Result<void> UpdateDescriptorSets();
    Result<void> EnsureCommandBuffer();
};

/**
 * @brief Vulkan backend factory
 */
class VulkanKernelRunnerFactory : public IKernelRunnerFactory {
public:
    bool IsAvailable() const override;
    std::vector<DeviceInfo> EnumerateDevices() const override;
    Result<std::unique_ptr<IKernelRunner>> CreateRunner(int device_id) const override;
    Backend GetBackendType() const override { return Backend::VULKAN; }
    std::string GetVersion() const override;
};

} // namespace kerntopia