#include "conv2d_core.hpp"
#include <iostream>

int main() {
    std::cout << "Kerntopia Conv2D Standalone Test" << std::endl;
    std::cout << "=================================" << std::endl;
    
    kerntopia::conv2d::Conv2dCore conv2d;
    
    // Hard-coded paths as requested
    const std::string ptx_path = "/home/mcarr/kerntopia/src/tests/conv2d/conv2d.ptx";
    const std::string input_path = "/home/mcarr/kerntopia/assets/images/StockSnap_2Q79J32WX2_512x512.png";
    const std::string output_path = "conv2d_output.png";
    
    std::cout << "Input image: " << input_path << std::endl;
    std::cout << "PTX kernel: " << ptx_path << std::endl;
    std::cout << "Output image: " << output_path << std::endl;
    std::cout << std::endl;
    
    // Execute pipeline
    if (!conv2d.Setup(ptx_path, input_path)) {
        std::cerr << "Setup failed!" << std::endl;
        return 1;
    }
    
    if (!conv2d.Execute()) {
        std::cerr << "Execution failed!" << std::endl;
        return 1;
    }
    
    if (!conv2d.WriteOut(output_path)) {
        std::cerr << "Write output failed!" << std::endl;
        return 1;
    }
    
    std::cout << std::endl;
    std::cout << "Success! Check " << output_path << " for the blurred result." << std::endl;
    std::cout << "Pipeline: Setup -> Execute -> WriteOut -> TearDown" << std::endl;
    
    return 0;
}