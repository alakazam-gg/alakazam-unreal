# Alakazam Portal - Unreal Engine Plugin

Unreal Engine plugin for real-time AI stylization. Transform any greybox scene into stylized game renders.

## Installation

1. Copy `AlakazamPortal` folder to your project's `Plugins/` directory
2. Build the alakazam-portal library (see below)
3. Regenerate project files
4. Enable the plugin in Edit → Plugins

## Building alakazam-portal Library

The plugin depends on the alakazam-portal C++ library:

```bash
cd ../alakazam-portal
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

Copy the built library to `Plugins/AlakazamPortal/ThirdParty/alakazam-portal/lib/`

## Quick Start

### Blueprint Setup

1. Add a **Scene Capture Component 2D** to your actor
2. Add an **Alakazam Controller** component
3. Set the **Scene Capture Component** reference
4. Configure **Server URL** and **Prompt**
5. Call **Connect** → **Start Streaming**

### C++ Setup

```cpp
#include "AlakazamController.h"

// In your actor
UAlakazamController* Alakazam = CreateDefaultSubobject<UAlakazamController>(TEXT("Alakazam"));
Alakazam->ServerUrl = TEXT("ws://localhost:9001");
Alakazam->Prompt = TEXT("anime style");

// When ready
Alakazam->Connect();
Alakazam->StartStreaming();

// Use Alakazam->OutputTexture for the stylized result
```

## Configuration

| Property | Description |
|----------|-------------|
| ServerUrl | WebSocket server URL |
| Prompt | Style description |
| CaptureWidth/Height | Resolution for capture |
| TargetFPS | Frame rate for streaming |
| JpegQuality | Compression quality (1-100) |

## Events

| Event | Description |
|-------|-------------|
| OnConnected | Fired when ready to stream |
| OnFrameReceived | Fired when stylized frame arrives |
| OnError | Fired on connection error |

## Requirements

- Unreal Engine 5.0+
- Active Alakazam server

## Documentation

[docs.alakazam.gg](https://docs.alakazam.gg)
