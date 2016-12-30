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

namespace ospray {

  /*! helper class that allows for easily computing (smoothed) frame rate */
  struct FPSCounter {
    FPSCounter();
    void startRender();
    void doneRender();
    double getFPS() const { return smooth_den / smooth_nom; }

  private:
    double smooth_nom;
    double smooth_den;
    double frameStartTime;
  };

}// namespace ospray
