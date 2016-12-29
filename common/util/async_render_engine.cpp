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

void async_render_engine::setModels(const async_render_engine::ModelList &models,
                                    const async_render_engine::BoundsList &bounds)
{
  // TODO: implement
}

void async_render_engine::setRenderer(cpp::Renderer renderer)
{
  // TODO: implement
}

void async_render_engine::setCamera(cpp::Camera camera)
{
  // TODO: implement
}

void async_render_engine::setFbSize(const ospcommon::vec2i &size)
{
  // TODO: implement
}

void async_render_engine::markViewChanged()
{
  // TODO: implement
}

void async_render_engine::markRendererChanged()
{
  // TODO: implement
}

void async_render_engine::start()
{
  state = ExecState::RUNNING;
  backgroundThread = std::thread(&async_render_engine::run, this);
}

void async_render_engine::stop()
{
  if (state == ExecState::INVALID)
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
  // TODO: implement
  return false;
}

void *async_render_engine::mapResults()
{
  // TODO: implement
  return nullptr;
}

void async_render_engine::unmapFrame()
{
  // TODO: implement
}

void async_render_engine::run()
{
  while (state == ExecState::RUNNING) {
    // TODO: implement
  }
}

}// namespace ospray
