// Vector Addition Kernel - Educational SLANG Example
// Demonstrates basic parallel computation across GPU backends
//
// This kernel adds two input vectors element-wise:
// output[i] = input_a[i] + input_b[i]

// Entry point for compute shader
[numthreads(256, 1, 1)]
#ifdef CUDA_BACKEND
void computeMain(uint3 dispatchThreadID : SV_DispatchThreadID)
#else
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
#endif
{
    // Get current thread index
    uint index = dispatchThreadID.x;
    
    // Bounds check - ensure we don't access beyond array size
    if (index >= buffer_size) {
        return;
    }
    
    // Perform vector addition
    output_buffer[index] = input_buffer_a[index] + input_buffer_b[index];
}

// Buffer declarations - will be bound at runtime
// Input vectors
RWStructuredBuffer<float> input_buffer_a;
RWStructuredBuffer<float> input_buffer_b;

// Output vector  
RWStructuredBuffer<float> output_buffer;

// Buffer size parameter
cbuffer Constants
{
    uint buffer_size;
}

// Educational Notes:
// - [numthreads(256, 1, 1)] defines workgroup size (256 threads per group)
// - SV_DispatchThreadID provides global thread coordinates 
// - RWStructuredBuffer allows read/write access from compute shader
// - cbuffer provides constant parameters from CPU
// - This kernel compiles to both SPIR-V (Vulkan) and PTX (CUDA)