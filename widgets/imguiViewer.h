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

#include <atomic>
#include <mutex>

// mini scene graph for loading the model
#include "common/miniSG/miniSG.h"

#include <ospray_cpp/Camera.h>
#include <ospray_cpp/Model.h>
#include <ospray_cpp/Renderer.h>

#include "imgui3D.h"
#include "Imgui3dExport.h"

#include <deque>

namespace ospray {

  /*! mini scene graph viewer widget. \internal Note that all handling
    of camera is almost exactly similar to the code in volView;
    might make sense to move that into a common class! */
  class OSPRAY_IMGUI3D_INTERFACE ImGuiViewer
    : public ospray::imgui3D::ImGui3DWidget
  {
  public:

    ImGuiViewer(const std::deque<ospcommon::box3f> &worldBounds,
                std::deque<cpp::Model> model,
                cpp::Renderer renderer,
                cpp::Camera camera);

    void setRenderer(OSPRenderer renderer);
    void resetAccumulation();
    void toggleFullscreen();
    void resetView();
    void printViewport();
    void saveScreenshot(const std::string &basename);
    void setScale(const ospcommon::vec3f& v )  {scale = v;}
    void setTranslation(const ospcommon::vec3f& v)  {translate = v;}
    void setLockFirstAnimationFrame(bool st) {lockFirstAnimationFrame = st;}
    // We override this so we can update the AO ray length
    void setWorldBounds(const ospcommon::box3f &worldBounds) override;

  protected:

    virtual void reshape(const ospcommon::vec2i &newSize) override;
    virtual void keypress(char key) override;
    virtual void updateAnimation(double deltaSeconds);

    virtual void buildGui() override;

    void display() override;

    void switchRenderers();

    // Data //

    std::deque<cpp::Model>       sceneModels;
    std::deque<ospcommon::box3f> worldBounds;
    cpp::FrameBuffer frameBuffer;
    cpp::Renderer    renderer;
    cpp::Camera      camera;

    ospray::imgui3D::FPSCounter fps;

    std::mutex rendererMutex;
    cpp::Renderer queuedRenderer;

    bool alwaysRedraw;

    ospcommon::vec2i windowSize;
    bool fullScreen;
    imgui3D::ImGui3DWidget::ViewPort glutViewPort;

    std::atomic<bool> resetAccum;
    double frameTimer;
    double animationTimer;
    double animationFrameDelta;
    size_t animationFrameId;
    bool animationPaused;
    bool lockFirstAnimationFrame;  //use for static scene
    ospcommon::vec3f translate;
    ospcommon::vec3f scale;
    int frameID={0};
  };

}// namespace ospray
