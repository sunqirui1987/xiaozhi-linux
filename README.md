# Linx Audio Client

一个跨平台的音频客户端项目，支持实时音频录制、播放和WebSocket通信。项目支持Linux（ALSA）和macOS（PortAudio）两个平台。

## 支持的平台

- **Linux**: 使用ALSA音频系统
- **macOS**: 使用PortAudio音频系统

## 系统要求

### Linux
- Ubuntu 18.04+ 或其他支持ALSA的Linux发行版
- CMake 3.22+
- GCC 7.0+ 或 Clang 6.0+

### macOS
- macOS 10.15+
- Xcode Command Line Tools
- Homebrew 包管理器
- CMake 3.22+

## 核心特性

- **跨平台音频支持**: 自动检测平台并使用相应的音频API
- **实时音频处理**: 支持音频录制和播放
- **Opus编解码**: 高质量音频压缩
- **WebSocket通信**: 实时双向通信
- **HTTP客户端**: 支持RESTful API调用
- **结构化日志**: 使用spdlog进行日志记录

## 项目结构

```
Linx-linux/
├── CMakeLists.txt          # 主CMake配置文件
├── Makefile               # 便捷构建脚本
├── README.md              # 项目说明文档
├── demo/                  # 演示应用
│   ├── CMakeLists.txt
│   └── linx.cc           # 主程序入口
├── docs/                  # 项目文档
│   ├── README.md          # 项目概述
│   ├── quickstart.md      # 快速开始指南
│   ├── api-reference.md   # API参考文档
│   └── modules/           # 模块文档
│       ├── audio.md
│       ├── websocket.md
│       ├── opus.md
│       ├── http.md
│       ├── log.md
│       └── filestream.md
└── linxsdk/              # 核心SDK库
    ├── CMakeLists.txt
    ├── alsaaudio/        # Linux ALSA音频实现
    ├── audio/            # 跨平台音频接口
    ├── filestream/       # 文件流处理
    ├── http/             # HTTP客户端
    ├── json/             # JSON处理
    ├── log/              # 日志系统
    ├── opus/             # Opus音频编解码
    ├── thirdparty/       # 第三方库
    └── websocket/        # WebSocket客户端
```

## 架构设计

### 音频系统抽象
项目使用抽象接口设计，支持多平台：

- `AudioInterface`: 音频操作的抽象基类
- `AlsaAudio`: Linux平台的ALSA实现
- `PortAudioImpl`: macOS平台的PortAudio实现
- `CreateAudioInterface()`: 工厂函数，自动选择平台实现

### 平台检测
使用预处理器宏进行平台检测：
```cpp
#ifdef __APPLE__
    // macOS特定代码
#else
    // Linux特定代码
#endif
```

### 核心组件

#### 音频接口 (AudioInterface)
- 提供跨平台音频录制和播放的统一接口
- 支持Linux ALSA和macOS PortAudio
- 自动平台检测和适配

#### WebSocket客户端 (WebSocketClient)
- 基于websocketpp实现
- 支持文本和二进制消息
- 异步回调机制

#### Opus编解码 (OpusAudio)
- 高质量音频压缩
- 支持多种采样率和声道配置
- 低延迟编解码

#### HTTP客户端 (HttpClient)
- RESTful API调用
- 文件上传支持
- 异步请求处理

## 配置说明

### 音频参数
- 采样率: 16000 Hz
- 声道数: 1 (单声道)
- 帧大小: 320 samples (20ms)
- 音频格式: 16-bit PCM

### WebSocket配置
- 服务器地址: `wss://api.tenclass.net/Linx/v1/`
- 音频编码: Opus
- 帧持续时间: 60ms

## 故障排除

### 常见问题

1. **编译错误**
   ```bash
   make clean
   make build
   ```

2. **依赖库缺失**
   - Linux: `sudo apt update && make prepare`
   - macOS: `brew update && make prepare`

3. **音频权限问题** (macOS)
   - 在系统偏好设置 → 安全性与隐私 → 隐私 → 麦克风中授权

4. **WebSocket连接失败**
   - 检查网络连接
   - 确认服务器地址和认证信息

### 调试模式
项目默认使用Debug模式编译，包含详细的日志输出。

## 开发指南

### 添加新功能
1. 遵循现有的代码结构和命名规范
2. 使用`AudioInterface`而不是直接使用平台特定的音频类
3. 添加适当的错误处理和日志记录
4. 确保新功能在所有支持的平台上都能正常工作

### 代码风格
- 使用C++17标准
- 遵循Google C++代码风格
- 使用智能指针管理内存
- 适当使用命名空间

## 快速开始

### 1. 安装依赖

#### Linux (Ubuntu/Debian)
```bash
make prepare
```
这将安装：
- libwebsocketpp-dev: WebSocket客户端库
- libasound-dev: ALSA音频开发库

#### macOS
```bash
make prepare
```
这将安装：
- websocketpp: WebSocket客户端库
- portaudio: 跨平台音频I/O库

### 2. 构建项目
```bash
make build
```

### 3. 运行项目
```bash
make run
```

## 详细构建说明

### 手动构建步骤

1. **创建构建目录**
   ```bash
   mkdir -p build
   cd build
   ```

2. **配置CMake**
   ```bash
   cmake -DBUILD_SHARED_LIBS=OFF \
         -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
         -DCMAKE_BUILD_TYPE=Debug \
         -DCMAKE_INSTALL_PREFIX=./install ..
   ```

3. **编译和安装**
   ```bash
   make -j 4
   make install
   ```

4. **运行程序**
   ```bash
   cd install/bin
   ./linx_app
   ```

## 文档导航

- [快速开始指南](docs/quickstart.md) - 环境配置和基本使用
- [API参考文档](docs/api-reference.md) - 完整的API接口说明
- [模块文档](docs/modules/) - 各模块详细使用指南
  - [音频模块](docs/modules/audio.md)
  - [WebSocket模块](docs/modules/websocket.md)
  - [Opus编解码](docs/modules/opus.md)
  - [HTTP客户端](docs/modules/http.md)
  - [日志系统](docs/modules/log.md)
  - [文件流处理](docs/modules/filestream.md)

## 许可证

本项目采用开源许可证，具体许可证信息请查看LICENSE文件。

## 贡献

欢迎提交Issue和Pull Request来改进项目。在提交代码前，请确保：
1. 代码通过所有测试
2. 遵循项目的代码风格
3. 添加适当的文档和注释
4. 在Linux和macOS平台上都进行了测试