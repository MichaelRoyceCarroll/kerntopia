# Contributing to Kerntopia

## Welcome Contributors! 🌾✨

Thank you for your interest in contributing to Kerntopia! This project aims to democratize GPU compute development through SLANG. Kerntopia welcomes contributions that advance this mission.

## Contributor License Agreement (CLA)

To ensure the project can potentially evolve in the future while protecting all contributors, a **Contributor License Agreement** is required for all pull requests.

### What This Means

By submitting a contribution to this project, you agree to the following terms:

1. **License Grant**: You grant the project maintainer (Michael Carroll) and the project a perpetual, worldwide, non-exclusive, royalty-free license to use, reproduce, modify, display, perform, sublicense, and distribute your contribution.

2. **Original Work**: You certify that your contribution is your original work or you have sufficient rights to submit it under these terms.

3. **Developer Certificate of Origin**: All commits must include a "Signed-off-by" line indicating acceptance of the Developer Certificate of Origin (DCO).

### How to Sign Your Commits

Add the `-s` flag to your git commits:

```bash
git commit -s -m "Add awesome new kernel for matrix multiplication"
```

This adds a `Signed-off-by: Your Name <your.email@example.com>` line to your commit message, indicating you certify the contribution under the DCO.

## Developer Certificate of Origin (DCO)

By signing off on commits, you certify that:

- The contribution was created in whole or in part by you and you have the right to submit it under the open source license indicated in the file; or
- The contribution is based upon previous work that, to the best of your knowledge, is covered under an appropriate open source license and you have the right under that license to submit that work with modifications; or  
- The contribution was provided directly to you by some other person who certified the above and you have not modified it.

**Full DCO text**: https://developercertificate.org/

## Types of Contributions

We welcome several types of contributions:

### 1. New SLANG Kernels
- Image processing algorithms
- Linear algebra operations
- Computer vision techniques
- Physics simulations
- Procedural generation

### 2. Backend Improvements
- Enhanced CUDA support
- Vulkan optimizations
- New backend implementations (i.e. DirectX 12, OpenCL, others)

### 3. Documentation
- Kernel documentation
- Tutorial improvements
- API documentation
- Usage examples

### 4. Testing & Quality
- Additional test cases
- Performance benchmarks
- Bug fixes
- Code quality improvements

## Contribution Process

### 1. Before You Start

- **Check Issues**: Look for existing issues or create one to discuss your contribution
- **Contact First**: For major changes, please contact the maintainer first
- **Follow Conventions**: Review existing code to understand project conventions

### 2. Development Workflow

```bash
# Fork the repository and clone your fork
git clone https://github.com/yourusername/kerntopia.git
cd kerntopia

# Create a feature branch
git checkout -b feature/awesome-new-kernel

# Make your changes with signed commits
git add .
git commit -s -m "Add matrix multiplication kernel with CUDA optimization"

# Push to your fork
git push origin feature/awesome-new-kernel

# Create a pull request
```

### 3. Pull Request Requirements

Your pull request must:

- ✅ **Include signed commits** (DCO compliance)
- ✅ **Pass all existing tests** 
- ✅ **Include new tests** for new functionality
- ✅ **Follow code style conventions**
- ✅ **Include documentation** for new features
- ✅ **Reference related issues** (if applicable)

### 4. Review Process

1. **Code Review**: Maintainer will review code, documentation, and tests
2. **Feedback**: Address any requested changes
3. **Merge**: Once approved, your contribution will be merged

## Code Standards

### Code Style
- **C++**: Follow existing formatting and naming conventions
- **SLANG**: Use clear comments for complex algorithms
- **CMake**: Follow project CMake patterns
- **Python**: PEP 8 compliance for wrapper scripts

### Documentation
- **Function Comments**: Document all public functions with purpose and parameters
- **Kernel Documentation**: Each kernel should have its own README.md
- **Educational Focus**: Comments should help others learn, not just maintain

### Testing
- **Unit Tests**: All new functionality requires tests
- **Integration Tests**: Backend-specific tests for new kernels
- **Performance Tests**: Benchmarks for performance-critical code

## Recognition

All contributors will be recognized in:
- [CONTRIBUTORS.md](CONTRIBUTORS.md) file
- Project acknowledgments
- Release notes (for significant contributions)

## Getting Help

- **Questions**: Open a GitHub issue with the `question` label
- **Discussion**: Use GitHub Discussions for broader topics
- **Direct Contact**: Email for sensitive issues or major contributions

## Code of Conduct

### Our Standards

- **Inclusive**: Welcome contributors of all skill levels
- **Educational**: Focus on learning and knowledge sharing
- **Respectful**: Constructive feedback and professional communication
- **Technical**: Decisions based on technical merit

### Unacceptable Behavior

- Harassment or discrimination of any kind
- Offensive comments or personal attacks
- Disruptive behavior in discussions
- Violation of intellectual property rights

## Legal Notes

- **MIT License**: All contributions will be licensed under the project's MIT license
- **No Transfer of Copyright**: You retain copyright to your contributions
- **Future Licensing**: The CLA allows the project to offer alternative licensing in the future
- **Patent Grant**: Contributors grant patent rights for their contributions

---

Thanks! ✨🌾✨

**Questions about contributing?** Open an issue or start a discussion
