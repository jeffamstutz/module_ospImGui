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

#include "async_render_engine.h"

namespace ospray {

async_render_engine::~async_render_engine()
{
  stop();
}

void async_render_engine::setRenderer(cpp::Renderer renderer)
{
  this->renderer = renderer;
}

void async_render_engine::setCamera(cpp::Camera camera)
{
  this->camera = camera;
}

void async_render_engine::setFbSize(const ospcommon::vec2i &size)
{
  fbSize = size;
}

void async_render_engine::markViewChanged()
{
  viewChanged = true;
}

void async_render_engine::markRendererChanged()
{
  rendererChanged = true;
}

void async_render_engine::start()
{
  validate();

  if (state == ExecState::INVALID)
    throw std::runtime_error("Can't start the engine in an invalid state!");

  state = ExecState::RUNNING;
  backgroundThread = std::thread(&async_render_engine::run, this);
}

void async_render_engine::stop()
{
  if (state != ExecState::RUNNING)
    return;

  state = ExecState::STOPPED;
  if (backgroundThread.joinable())
    backgroundThread.join();
}

ExecState async_render_engine::runningState()
{
  return state;
}

bool async_render_engine::hasNewFrame()
{
  return newPixels;
}

uint32_t *async_render_engine::mapResults()
{
  fbMutex.lock();
  newPixels = false;
  return pixelBuffer[mappedPB].data();
}

void async_render_engine::unmapFrame()
{
  fbMutex.unlock();
}

void async_render_engine::validate()
{
  if (state == ExecState::INVALID)
  {
    renderer.update();
    state = (camera.handle() && renderer.ref().handle()) ? ExecState::STOPPED :
                                                           ExecState::INVALID;
  }
}

bool async_render_engine::checkForFbResize()
{
  bool changed = fbSize.update();

  if (changed) {
    auto &size  = fbSize.ref();
    frameBuffer = cpp::FrameBuffer(osp::vec2i{size.x, size.y}, OSP_FB_SRGBA,
                                   OSP_FB_COLOR | OSP_FB_DEPTH | OSP_FB_ACCUM);

    nPixels = size.x * size.y;
    pixelBuffer[0].resize(nPixels);
    pixelBuffer[1].resize(nPixels);
  }

  return changed;
}

bool async_render_engine::updateProperties()
{
  bool changes = renderer.update();

  changes |= viewChanged | rendererChanged;

  if (viewChanged)     camera.commit();
  if (rendererChanged) renderer.ref().commit();

  viewChanged     = false;
  rendererChanged = false;

  return changes;
}

void async_render_engine::run()
{
  while (state == ExecState::RUNNING) {
    bool resetAccum = false;
    resetAccum |= checkForFbResize();
    resetAccum |= updateProperties();

    if (resetAccum)
      frameBuffer.clear(OSP_FB_ACCUM);

    renderer.ref().renderFrame(frameBuffer, OSP_FB_COLOR | OSP_FB_ACCUM);

    auto *srcPB = (uint32_t*)frameBuffer.map(OSP_FB_COLOR);
    auto *dstPB = (uint32_t*)pixelBuffer[currentPB].data();

    memcpy(dstPB, srcPB, nPixels*sizeof(uint32_t));

    frameBuffer.unmap(srcPB);

    if (fbMutex.try_lock())
    {
      std::swap(currentPB, mappedPB);
      newPixels = true;
      fbMutex.unlock();
    }
  }
}

}// namespace ospray
