#pragma once

#include "ikernel_runner.hpp"
#include <chrono>
#include <map>

namespace kerntopia {

// Forward declarations for Vulkan types
struct VulkanContext;
struct VulkanDevice;
struct VulkanComputePipeline;
struct VulkanCommandPool;
struct VulkanQueryPool;

/**
 * @brief Vulkan buffer implementation
 */
class VulkanBuffer : public IBuffer {
public:
    VulkanBuffer(VulkanDevice* device, size_t size, Type type, Usage usage);
    ~VulkanBuffer();
    
    size_t GetSize() const override { return size_; }
    Type GetType() const override { return type_; }
    
    void* Map() override;
    void Unmap() override;
    
    Result<void> UploadData(const void* data, size_t size, size_t offset = 0) override;
    Result<void> DownloadData(void* data, size_t size, size_t offset = 0) override;
    
    // Vulkan-specific methods  
    void* GetBuffer() const { return reinterpret_cast<void*>(buffer_); }
    void* GetDeviceMemory() const { return reinterpret_cast<void*>(device_memory_); }
    void DestroyBuffer();
    
private:
    VulkanDevice* device_;
    size_t size_;
    Type type_;
    Usage usage_;
    
    // Vulkan handles (stored as void* for header compatibility, cast to VkBuffer/VkDeviceMemory in implementation)
    void* buffer_ = nullptr;
    void* device_memory_ = nullptr;
    
    void* mapped_ptr_ = nullptr;
    bool is_mapped_ = false;
    
    bool CreateBuffer();
};

/**
 * @brief Vulkan texture implementation
 */
class VulkanTexture : public ITexture {
public:
    VulkanTexture(VulkanDevice* device, const TextureDesc& desc);
    ~VulkanTexture();
    
    const TextureDesc& GetDesc() const override { return desc_; }
    
    Result<void> UploadData(const void* data, uint32_t mip_level = 0, uint32_t array_layer = 0) override;
    Result<void> DownloadData(void* data, size_t data_size, uint32_t mip_level = 0, uint32_t array_layer = 0) override;
    
    // Vulkan-specific methods
    void* GetImage() const { return reinterpret_cast<void*>(image_); }
    void* GetImageView() const { return reinterpret_cast<void*>(image_view_); }
    void* GetDeviceMemory() const { return reinterpret_cast<void*>(device_memory_); }
    
private:
    VulkanDevice* device_;
    TextureDesc desc_;
    
    // Vulkan handles (stored as void* for header compatibility, cast to VkImage/VkImageView/VkDeviceMemory in implementation)
    void* image_ = nullptr;
    void* image_view_ = nullptr;
    void* device_memory_ = nullptr;
    
    bool CreateImage();
    void DestroyImage();
    uint32_t GetVulkanFormat() const;
};

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