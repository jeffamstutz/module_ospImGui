// ======================================================================== //
// Copyright 2009-2016 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

// std
#include <atomic>
#include <deque>
#include <thread>

// ospcommon
#include <ospcommon/box.h>

// ospray::cpp
#include <ospray_cpp/Camera.h>
#include <ospray_cpp/Model.h>
#include <ospray_cpp/Renderer.h>

// ospImGui util
#include "fenced_property.h"

namespace ospray {

enum class ExecState {STOPPED, RUNNING, INVALID};

class async_render_engine
{
public:

  async_render_engine() = default;
  ~async_render_engine();

  // Properties //

  void setRenderer(cpp::Renderer renderer);
  void setCamera(cpp::Camera camera);
  void setFbSize(const ospcommon::vec2i &size);

  // Methods to say when an objects needs to be comitted before next frame //

  void markViewChanged();
  void markRendererChanged();

  // Engine conrols //

  void start();
  void stop();

  ExecState runningState();

  // Output queries //

  bool      hasNewFrame();
  uint32_t* mapFramebuffer();
  void      unmapFrame();

private:

  // Helper functions //

  void validate();
  bool checkForFbResize();
  bool updateProperties();
  void run();

  // Data //

  std::thread backgroundThread;
  std::atomic<ExecState> state {ExecState::INVALID};

  cpp::FrameBuffer frameBuffer;
  cpp::Camera      camera;

  fenced_property<cpp::Renderer>    renderer;
  fenced_property<int>              whichModelToRender {0};
  fenced_property<ospcommon::vec2i> fbSize;

  int nPixels {0};

  int currentPB {0};
  int mappedPB  {1};
  std::mutex fbMutex;
  std::vector<uint32_t> pixelBuffer[2];

  std::atomic<bool> viewChanged     {false};
  std::atomic<bool> rendererChanged {false};
  std::atomic<bool> newPixels       {false};
};

}// namespace ospray