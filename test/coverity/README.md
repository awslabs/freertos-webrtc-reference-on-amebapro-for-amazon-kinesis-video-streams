# Static code analysis for FreeRTOS WebRTC Application
This directory is made for the purpose of statically analyzing the FreeRTOS WebRTC Application Library using
[Black Duck Coverity](https://www.blackduck.com/static-analysis-tools-sast/coverity.html) static analysis tool.
This configuration focuses on detecting code defects such as memory leaks, null pointer dereferences,
buffer overflows, and other code quality issues.

> **Note**
For generating the report as outlined below, we have used Coverity version 2025.6.0.

## Getting Started
### Prerequisites
You can run this on a platform supported by Coverity. The list of supported platforms and other details can be found [here](https://documentation.blackduck.com/bundle/coverity-docs/page/deploy-install-guide/topics/supported_platforms_for_coverity_analysis.html).
To compile and run the Coverity target successfully, you must have the following:

1. CMake version > 3.13.0 (You can check whether you have this by typing `cmake --version`).
1. Download the repo including the submodules using the following commands:
    - `git clone --recurse-submodules https://github.com/awslabs/freertos-webrtc-reference-on-amebapro-for-amazon-kinesis-video-streams`
    - `cd ./freertos-webrtc-reference-on-amebapro-for-amazon-kinesis-video-streams`
1. Follow [Compile commands](../../README.md) to download/set-up toolchain
1. Follow [Required Configuration](../../README.md) to create demo_config.h
    - For the coverity scan, the actual configuration values are not important - they just need to be valid enough for successful compilation.

### To build and run coverity:
1. Open terminal and change directory to the project location:
   ```sh
   cd project/realtek_amebapro2_webrtc_application/GCC-RELEASE
   ```
1. Create `build` directory and enter `build` directory:
   ```sh
   mkdir build
   cd build
   ```
1. Run the following command to generate Makefile:
   ```sh
   cmake .. -G"Unix Makefiles" -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake
   ```
1. Run the following command to scan:
   ```sh
   cov-configure --config covConfig/coverity.xml --compiler arm-none-eabi-gcc --comptype gcc --template
   cov-build --config covConfig/coverity.xml --emit-complementary-info --dir . make -j $(nproc)
   cov-analyze --dir . --tu-pattern "file('.*/examples/.*')"
   cov-format-errors --dir . --html-output coverity_report --exclude-files "mpu_armv8.h"
   ```

These commands will:
1. Configure the build system using the Coverity-specific CMake configuration.
2. Build and execute the Coverity scan target, which handles all the necessary steps including:
   - Compiler configuration.
   - Static analysis execution.
   - Report generation in both HTML and JSON formats.

You should now have the HTML formatted violations list in a directory named `project/realtek_amebapro2_webrtc_application/GCC-RELEASE/build/coverity_report`.
