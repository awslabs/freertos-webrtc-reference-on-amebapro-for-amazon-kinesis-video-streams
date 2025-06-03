# freertos-webrtc-reference-on-amebapro-for-amazon-kinesis-video-streams

> [!IMPORTANT]
> This repository is currently in development and not recommended for production use.

## Ameba Pro2 Mini
![Board Image](docs/images/board.jpg)

## Clone
Execute the following commands to clone this repository along with all the
submodules-
```sh
git clone https://github.com/awslabs/freertos-webrtc-reference-on-amebapro-for-amazon-kinesis-video-streams
cd freertos-webrtc-reference-on-amebapro-for-amazon-kinesis-video-streams
git submodule update --init --recursive
```

## Setup
### Required Configuration
Before choosing an authentication method, configure these common settings:
   * Copy `examples/demo_config/demo_config_template.h` and rename it to `examples/demo_config/demo_config.h` and set the following:
   * Set `AWS_REGION` to your AWS region.
   * Set `AWS_KVS_CHANNEL_NAME` to your KVS signaling channel name.

### Authentication Methods
Choose ONE of the following authentication options:

#### Option 1: Using Access Keys
   * Set `AWS_ACCESS_KEY_ID` to your access key.
   * Set `AWS_SECRET_ACCESS_KEY` to your secret access key.
   * Set `AWS_SESSION_TOKEN` to your session token (required only for temporary credentials).

#### Option 2: Using IoT Role-alias
   * Set `AWS_CREDENTIALS_ENDPOINT` to your AWS Endpoint.
   * Set `AWS_IOT_THING_NAME` to your Thing Name associated with that Certificate.
   * Set `AWS_IOT_THING_ROLE_ALIAS` to your Role Alias.
   * Set `AWS_IOT_THING_CERT` to your IOT Core Certificate.
   * Set `AWS_IOT_THING_PRIVATE_KEY` to your IOT Core Private Key.

NOTE : To add the `AWS_IOT_THING_CERT` and `AWS_IOT_THING_PRIVATE_KEY` in the correct format, run the `format_cert_and_key.sh`.\
A `formatted_certificate_and_ket.txt` file will be generated inside the `examples/demo_config/` path. You can copy the content and paste it in the `demo_config.h`

## Compile commands
1. Download toolchain (based on your environment)
   - https://github.com/Ameba-AIoT/ameba-toolchain/releases/tag/V10.3.0-amebe-rtos-pro2
1. Setup toolchain:
   ```sh
   tar -xvf asdk-10.3.0-*.tar.bz2

   chmod +x `pwd`/libraries/ambpro2_sdk/project/realtek_amebapro2_v0_example/GCC-RELEASE/mp/*

   <!-- if The environment is Linux :  -->
   export PATH=`pwd`/asdk-10.3.0/linux/newlib/bin:$PATH

   <!-- else if The environment is Darwin : -->
   export PATH=`pwd`/asdk-10.3.0/darwin/newlib/bin:$PATH
   ```
1. Connect the board to the PC using the CH340 micro USB port (marked as "Serial
   Output" in the above image).

## Build
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
1. Run the following command to build:
   ```sh
   cmake --build . --target flash
   ```

## Flash
### Copy Flash Tool [Needed only one time]
1. Unzip the flash tool:
   ```shell
   cd libraries/ambpro2_sdk/tools/
   unzip Pro2_PG_tool _v1.3.0.zip
   ```
   **Note: If you're using Mac, please find repo owner to get Pro2_PG_tool+_v1.4.3_B.zip.**
1. Copy the contents of `libraries/ambpro2_sdk/tools/Pro2_PG_tool _v1.3.0` to
   a directory in the Windows/Mac file system.

### Flash Binary
1. Close TeraTerm (or minicom for Mac) if it is running and connected to the board.
1. Copy the generated binary `project/realtek_amebapro2_webrtc_application/GCC-RELEASE/build/flash_ntz.bin`
   to the directory in the Windows/Mac file system which contains the flash tool
   from the [Copy Flash Tool](#copy-flash-tool-needed-only-one-time) section.
1. Open a terminal (such as PowerShell) and enter the directory in the
   Windows/Mac file system which contains the flash tool from the
   [Copy Flash Tool](#copy-flash-tool-needed-only-one-time) section.
1. Enter the board into program mode:
   * Press the Reset button.
   * Press the Program button while keeping the Reset button pressed.
   * Release the Reset button.
   * Release the Program button.
1. Run the following command in the terminal (such as PowerShell) to
   flash the binary:
   * Windows:
      ```sh
      .\uartfwburn.exe -p COMxx -f flash_ntz.bin -b 2000000 -U
      ```
      Replace COMxx with the actual COM port that you can find in the Device Manager.
   * Mac:
      ```sh
      chmod +x uartfwburn.arm.darwin
      sudo ./uartfwburn.arm.darwin -p /dev/cu.usbserial-**** -f ./flash_ntz.bin -b 2000000 -U
      ```
      Replace /dev/cu.usbserial-**** with the actual COM port that you can find by running:
      ```sh
      ls /dev/cu.*
      ```

## Run
### Set up WiFi [Needed only one time]
1. Open TeraTerm (or minicom for Mac) and connect to the COM port.
1. Press and release the Reset button.
1. Send the following commands to the device using TeraTerm to setup WiFi SSID
   and Password:
   ```sh
   ATW0=<ssid>
   ATW1=<password>
   ATWC
   ```

### Run the Program
1. Open TeraTerm (or minicom for Mac) and connect to the COM port.
1. Press and release the Reset button.

## Feature Options
1. [Data Channel Support](#data-channel-support)
1. [TWCC Support](#twcc-support)
1. [Join Storage Session](#join-storage-session-support)
1. [Enabling Metrics Logging](#enabling-metrics-logging)

### Data Channel Support

WebRTC Data Channel is a bidirectional peer-to-peer communication channel for arbitrary application data. It operates over SCTP (Stream Control Transmission Protocol) and provides both reliable and unreliable data delivery modes.

#### Enabling Data Channel Support

Data channel support is enabled by default in this application through the `BUILD_USRSCTP_LIBRARY` flag, which is set to `ON` in [webrtc_master_demo.cmake](./project/webrtc_master_demo.cmake). To disable data channel support, set this flag to `OFF` using the cmake command below.

```
cmake .. -G"Unix Makefiles" -DBUILD_USRSCTP_LIBRARY=OFF -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake
```

### TWCC Support

Transport Wide Congestion Control (TWCC) is a mechanism in WebRTC designed to enhance the performance and reliability of real-time communication over the internet. TWCC addresses the challenges of network congestion by providing detailed feedback on the transport of packets across the network, enabling adaptive bitrate control and optimization of media streams in real-time. This feedback mechanism is crucial for maintaining high-quality audio and video communication, as it allows senders to adjust their transmission strategies based on comprehensive information about packet losses, delays, and jitter experienced across the entire transport path.

The importance of TWCC in WebRTC lies in its ability to ensure efficient use of available network bandwidth while minimizing the negative impacts of network congestion. By monitoring the delivery of packets across the network, TWCC helps identify bottlenecks and adjust the media transmission rates accordingly. This dynamic approach to congestion control is essential for preventing degradation in call quality, such as pixelation, stuttering, or drops in audio and video streams, especially in environments with fluctuating network conditions.

To learn more about TWCC, check [TWCC spec](https://datatracker.ietf.org/doc/html/draft-holmer-rmcat-transport-wide-cc-extensions-01)

#### Enabling TWCC Support

TWCC is enabled by default in this application (via `ENABLE_TWCC_SUPPORT`) value set as `1` in `demo_config_template.h`. In order to disable it, set this value to `0`.

```c
#define ENABLE_TWCC_SUPPORT 1U
```

If not using the samples directly, following thing need to be done to set up Twcc:

1. Set the callback that will have the business logic to modify the bitrate based on packet loss information. The callback can be set using `PeerConnection_SetSenderBandwidthEstimationCallback()` inside `PeerConnection_Init()`:
```c
ret = PeerConnection_SetSenderBandwidthEstimationCallback( pSession,
                                                           SampleSenderBandwidthEstimationHandler,
                                                           &pSession->twccMetaData );
```

### Join Storage Session Support

> [!WARNING]  
> The "Join Storage Session" feature is currently in a beta state and may exhibit stability issues. Our team is actively working on improvements and fixes. Users may experience:
> - Intermittent connection drops
> - Missing media playback on cloud

Join Storage Session enables video producing devices to join or create WebRTC sessions for real-time media ingestion through Amazon Kinesis Video Streams. For Master configurations, this allows devices to ingest both audio and video media while maintaining synchronized playback capabilities.

In our implementation (Master participant only):
1. First connect to Kinesis Video Streams with WebRTC Signaling.
2. It calls the `JoinStorageSession` API to initiate a storage session WebRTC connection.
3. Once WebRTC connection is established, media is ingested to the configured Kinesis video stream.

#### Media Requirements
- **Video Track**: H.264 codec required.
- **Audio Track**: Opus codec required.
- Both audio and video tracks are mandatory for WebRTC ingestion.

#### Enabling Join Storage Session Support

Join Storage Session is disabled by default in this application (via `JOIN_STORAGE_SESSION`) value set as `0` in `demo_config_template.h`. In order to enable it, set this value to `1`.
```c
#define JOIN_STORAGE_SESSION 0
```

#### Prerequisites for enabling Join Storage Session

Before using Join Storage Session, Set up Signaling Channel with Video Stream :
   - Create a Kinesis Video Streams signaling channel
   - Create a Kinesis Video Streams video stream
   - Connect the channel to the video stream
   - Ensure proper IAM permissions are configured

For detailed setup instructions, refer to: https://docs.aws.amazon.com/kinesisvideostreams-webrtc-dg/latest/devguide/webrtc-ingestion.html

### Enabling Metrics Logging
METRIC_PRINT_ENABLED flag enables detailed metrics logging for WebRTC setup. It logs the following time for each connection :
   - Duration to describe Signaling Channel
   - Duration to get Signaling Endpoints
   - Duration to get Ice Server List
   - Duration to connect Websocket Server
   - Duration to get Authentication Temporary Credentials
   - Duration to gather ICE Host Candidate
   - Duration to gather ICE Srflx Candidate
   - Duration to gather ICE Relay Candidate
   - Duration to join Storage Session
   - Duration to find Peer-To-Peer Connection
   - Duration to DTLS Handshaking Completion
   - Duration to sending First Frame
**Note**: `METRIC_PRINT_ENABLED` flag can be used to enable metrics logging. It can be used like: `cmake -S . -B build -DMETRIC_PRINT_ENABLED=ON`. The flag is disabled by default.

## Security

See [CONTRIBUTING](CONTRIBUTING.md#security-issue-notifications) for more information.

## License

This project is licensed under the Apache-2.0 License.
