// ======================================================================== //
// Copyright 2016 SURVICE Engineering Company                               //
// Copyright 2016 Intel Corporation                                         //
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

#include "imguiViewer.h"

#include <imgui.h>

using std::cout;
using std::endl;

using std::string;

using std::lock_guard;
using std::mutex;

using namespace ospcommon;

// Static local helper functions //////////////////////////////////////////////

// helper function to write the rendered image as PPM file
static void writePPM(const string &fileName, const int sizeX, const int sizeY,
                     const uint32_t *pixel)
{
  FILE *file = fopen(fileName.c_str(), "wb");
  fprintf(file, "P6\n%i %i\n255\n", sizeX, sizeY);
  unsigned char *out = (unsigned char *)alloca(3*sizeX);
  for (int y = 0; y < sizeY; y++) {
    const unsigned char *in = (const unsigned char *)&pixel[(sizeY-1-y)*sizeX];
    for (int x = 0; x < sizeX; x++) {
      out[3*x + 0] = in[4*x + 0];
      out[3*x + 1] = in[4*x + 1];
      out[3*x + 2] = in[4*x + 2];
    }
    fwrite(out, 3*sizeX, sizeof(char), file);
  }
  fprintf(file, "\n");
  fclose(file);
}

// ImGuiViewer definitions ////////////////////////////////////////////////////

namespace ospray {

ImGuiViewer::ImGuiViewer(const std::deque<box3f> &worldBounds,
                         std::deque<cpp::Model> model,
                         cpp::Renderer renderer,
                         cpp::Camera camera)
  : ImGui3DWidget(ImGui3DWidget::FRAMEBUFFER_NONE),
    sceneModels(model),
    frameBuffer(nullptr),
    renderer(renderer),
    camera(camera),
    queuedRenderer(nullptr),
    alwaysRedraw(true),
    fullScreen(false),
    worldBounds(worldBounds),
    lockFirstAnimationFrame(false)
{
  if (!worldBounds.empty()) {
    setWorldBounds(worldBounds[0]);
  }
  renderer.set("model",  sceneModels[0]);
  renderer.set("camera", camera);
  renderer.commit();

  resetAccum = false;
  frameTimer = ospcommon::getSysTime();
  animationTimer = 0.;
  animationFrameDelta = .03;
  animationFrameId = 0;
  animationPaused = false;
  glutViewPort = viewPort;
  scale = vec3f(1,1,1);
}

void ImGuiViewer::setRenderer(OSPRenderer renderer)
{
  lock_guard<mutex> lock{rendererMutex};
  queuedRenderer = renderer;
}

void ImGuiViewer::resetAccumulation()
{
  resetAccum = true;
}

void ImGuiViewer::toggleFullscreen()
{
  fullScreen = !fullScreen;

#if 0
  if(fullScreen) {
    glutFullScreen();
  } else {
    glutPositionWindow(0,10);
  }
#endif
}

void ImGuiViewer::resetView()
{
  auto oldAspect = viewPort.aspect;
  viewPort = glutViewPort;
  viewPort.aspect = oldAspect;
}

void ImGuiViewer::printViewport()
{
  printf("-vp %f %f %f -vu %f %f %f -vi %f %f %f\n",
         viewPort.from.x, viewPort.from.y, viewPort.from.z,
         viewPort.up.x,   viewPort.up.y,   viewPort.up.z,
         viewPort.at.x,   viewPort.at.y,   viewPort.at.z);
  fflush(stdout);
}

void ImGuiViewer::saveScreenshot(const std::string &basename)
{
  const uint32_t *p = (uint32_t*)frameBuffer.map(OSP_FB_COLOR);
  writePPM(basename + ".ppm", windowSize.x, windowSize.y, p);
  cout << "#ospGlutViewer: saved current frame to '" << basename << ".ppm'"
       << endl;
}

void ImGuiViewer::setWorldBounds(const box3f &worldBounds) {
  ImGui3DWidget::setWorldBounds(worldBounds);
  aoDistance = (worldBounds.upper.x - worldBounds.lower.x)/4.f;
  renderer.set("aoDistance", aoDistance);
  renderer.commit();
}

void ImGuiViewer::reshape(const vec2i &newSize)
{
  ImGui3DWidget::reshape(newSize);
  windowSize = newSize;
  frameBuffer = cpp::FrameBuffer(osp::vec2i{newSize.x, newSize.y},
                                 OSP_FB_SRGBA,
                                 OSP_FB_COLOR | OSP_FB_DEPTH | OSP_FB_ACCUM);

  frameBuffer.clear(OSP_FB_ACCUM);

  camera.set("aspect", viewPort.aspect);
  camera.commit();
  viewPort.modified = true;
}

void ImGuiViewer::keypress(char key)
{
  switch (key) {
  case ' ':
    animationPaused = !animationPaused;
    break;
  case '=':
    animationFrameDelta = max(animationFrameDelta-0.01, 0.0001); 
    break;
  case '-':
    animationFrameDelta = min(animationFrameDelta+0.01, 1.0); 
    break;
  case 'R':
    alwaysRedraw = !alwaysRedraw;
    break;
  case '!':
    saveScreenshot("ospimguiviewer");
    break;
  case 'X':
    if (viewPort.up == vec3f(1,0,0) || viewPort.up == vec3f(-1.f,0,0)) {
      viewPort.up = - viewPort.up;
    } else {
      viewPort.up = vec3f(1,0,0);
    }
    viewPort.modified = true;
    break;
  case 'Y':
    if (viewPort.up == vec3f(0,1,0) || viewPort.up == vec3f(0,-1.f,0)) {
      viewPort.up = - viewPort.up;
    } else {
      viewPort.up = vec3f(0,1,0);
    }
    viewPort.modified = true;
    break;
  case 'Z':
    if (viewPort.up == vec3f(0,0,1) || viewPort.up == vec3f(0,0,-1.f)) {
      viewPort.up = - viewPort.up;
    } else {
      viewPort.up = vec3f(0,0,1);
    }
    viewPort.modified = true;
    break;
  case 'c':
    viewPort.modified = true;//Reset accumulation
    break;
  case 'f':
    toggleFullscreen();
    break;
  case 'r':
    resetView();
    break;
  case 'p':
    printViewport();
    break;
  default:
    ImGui3DWidget::keypress(key);
  }
}

void ImGuiViewer::display()
{
  if (!frameBuffer.handle() || !renderer.handle() ) return;

  // NOTE: consume a new renderer if one has been queued by another thread
  switchRenderers();

  updateAnimation(ospcommon::getSysTime()-frameTimer);
  frameTimer = ospcommon::getSysTime();

  if (resetAccum) {
    frameBuffer.clear(OSP_FB_ACCUM);
    resetAccum = false;
  }

  ++frameID;

  if (viewPort.modified) {
    Assert2(camera.handle(),"ospray camera is null");
    camera.set("pos", viewPort.from);
    auto dir = viewPort.at - viewPort.from;
    camera.set("dir", dir);
    camera.set("up", viewPort.up);
    camera.set("aspect", viewPort.aspect);
    camera.set("fovy", viewPort.openingAngle);
    camera.commit();

    viewPort.modified = false;
    frameBuffer.clear(OSP_FB_ACCUM);
  }

  fps.startRender();
  renderer.renderFrame(frameBuffer, OSP_FB_COLOR | OSP_FB_ACCUM);
  fps.doneRender();

  // set the glut3d widget's frame buffer to the opsray frame buffer,
  // then display
  ucharFB = (uint32_t *)frameBuffer.map(OSP_FB_COLOR);
  frameBufferMode = ImGui3DWidget::FRAMEBUFFER_UCHAR;
  ImGui3DWidget::display();

  frameBuffer.unmap(ucharFB);

  // that pointer is no longer valid, so set it to null
  ucharFB = nullptr;
}

void ImGuiViewer::switchRenderers()
{
  lock_guard<mutex> lock{rendererMutex};

  if (queuedRenderer.handle()) {
    renderer = queuedRenderer;
    queuedRenderer = nullptr;
    frameBuffer.clear(OSP_FB_ACCUM);
  }
}

void ImGuiViewer::updateAnimation(double deltaSeconds)
{
  if (sceneModels.size() < 2)
    return;
  if (animationPaused)
    return;
  animationTimer += deltaSeconds;
  int framesSize = sceneModels.size();
  const int frameStart = (lockFirstAnimationFrame ? 1 : 0);
  if (lockFirstAnimationFrame)
    framesSize--;

  if (animationTimer > animationFrameDelta)
  {
    animationFrameId++;

    //set animation time to remainder off of delta 
    animationTimer -= int(animationTimer/deltaSeconds) * deltaSeconds;

    size_t dataFrameId = animationFrameId%framesSize+frameStart;
    if (lockFirstAnimationFrame)
    {
      ospcommon::affine3f xfm = ospcommon::one;
      xfm *= ospcommon::affine3f::translate(translate)
             * ospcommon::affine3f::scale(scale);
      OSPGeometry dynInst =
              ospNewInstance((OSPModel)sceneModels[dataFrameId].object(),
              (osp::affine3f&)xfm);
      ospray::cpp::Model worldModel = ospNewModel();
      ospcommon::affine3f staticXFM = ospcommon::one;
      OSPGeometry staticInst =
              ospNewInstance((OSPModel)sceneModels[0].object(),
              (osp::affine3f&)staticXFM);
      //Carson: TODO: creating new world model every frame unecessary
      worldModel.addGeometry(staticInst);
      worldModel.addGeometry(dynInst);
      worldModel.commit();
      renderer.set("model",  worldModel);
    }
    else
    {
      renderer.set("model",  sceneModels[dataFrameId]);
    }
    renderer.commit();
    resetAccumulation();
  }
}

void ImGuiViewer::buildGui()
{
  static bool show_renderer_window = false;

  ImGui::Begin("Viewer Controls: press 'g' to show/hide");
  if (ImGui::Button("Edit Renderer Parameters")) show_renderer_window ^= 1;
  if (ImGui::Button("      Auto Rotate       ")) animating ^= 1;

  ImGui::NewLine();
  ImGui::Text("ospRenderFrame() rate: %.1f FPS", fps.getFPS());
  ImGui::Text("   [avg] display rate: %.1f FPS", ImGui::GetIO().Framerate);
  ImGui::NewLine();

  if (ImGui::Button("Quit")) std::exit(0);

  ImGui::End();

  if (show_renderer_window)
  {
    ImGui::SetNextWindowSize(ImVec2(200, 350), ImGuiSetCond_FirstUseEver);
    ImGui::Begin("Renderer Parameters", &show_renderer_window);

    bool renderer_changed = false;

    static int ao = 1;
    if (ImGui::SliderInt("aoSamples", &ao, 0, 32)) {
      renderer.set("aoSamples", ao);
      renderer_changed = true;
    }

    if (ImGui::InputFloat("aoDistance", &aoDistance)) {
      renderer.set("aoDistance", aoDistance);
      renderer_changed = true;
    }

    static bool shadows = true;
    if (ImGui::Checkbox("shadows", &shadows)) {
      renderer.set("shadowsEnabled", int(shadows));
      renderer_changed = true;
    }

    static bool singleSidedLighting = true;
    if (ImGui::Checkbox("single_sided_lighting", &singleSidedLighting)) {
      renderer.set("oneSidedLighting", int(singleSidedLighting));
      renderer_changed = true;
    }

    static float epsilon = 1e-6f;
    if (ImGui::InputFloat("ray_epsilon", &epsilon)) {
      renderer.set("epsilon", epsilon);
      renderer_changed = true;
    }

    static int spp = 1;
    if (ImGui::SliderInt("spp", &spp, -4, 16)) {
      renderer.set("spp", spp);
      renderer_changed = true;
    }

    static ImVec4 bg_color = ImColor(255, 255, 255);
    if (ImGui::ColorEdit3("bg_color", (float*)&bg_color)) {
      renderer.set("bgColor", bg_color.x, bg_color.y, bg_color.z);
      renderer_changed = true;
    }

    if (renderer_changed) {
      renderer.commit();
      resetAccumulation();
    }

    ImGui::End();
  }
}

}// namepace ospray
