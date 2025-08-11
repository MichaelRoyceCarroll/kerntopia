// 2D Convolution Kernel - Educational SLANG Example  
// Demonstrates image processing with 2D thread dispatch
//
// Applies a 3x3 convolution filter to input image:
// Each thread processes one output pixel by sampling neighborhood

// Entry point for 2D compute shader
[numthreads(16, 16, 1)]
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // Get current pixel coordinates
    uint2 coord = dispatchThreadID.xy;
    
    // Bounds check
    if (coord.x >= image_width || coord.y >= image_height) {
        return;
    }
    
    // Apply 3x3 convolution filter
    float3 result = float3(0.0, 0.0, 0.0);
    
    // Sample 3x3 neighborhood with edge clamping
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            // Clamp coordinates to image bounds
            uint2 sample_coord = uint2(
                clamp(int(coord.x) + dx, 0, int(image_width) - 1),
                clamp(int(coord.y) + dy, 0, int(image_height) - 1)
            );
            
            // Get filter weight and input pixel
            float weight = filter_kernel[dy + 1][dx + 1];
            uint input_index = sample_coord.y * image_width + sample_coord.x;
            float3 pixel = input_image[input_index];
            
            // Accumulate weighted contribution
            result += weight * pixel;
        }
    }
    
    // Write result to output
    uint output_index = coord.y * image_width + coord.x;
    output_image[output_index] = result;
}

// Image buffers - RGB float format
RWStructuredBuffer<float3> input_image;
RWStructuredBuffer<float3> output_image;

// 3x3 convolution kernel (e.g., edge detection, blur)
cbuffer Constants
{
    uint image_width;
    uint image_height;
    float filter_kernel[3][3];  // 3x3 filter weights
}

// Educational Notes:
// - [numthreads(16, 16, 1)] creates 16x16 thread groups for 2D processing
// - uint2/float3 are SLANG vector types for coordinates and RGB
// - Edge clamping prevents sampling outside image bounds
// - Common filters: Gaussian blur, Sobel edge detection, sharpening
// - This pattern scales to larger kernels (5x5, 7x7, etc.)