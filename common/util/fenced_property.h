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

#include <mutex>
#include <utility>

template <typename T>
class fenced_property
{
public:

  fenced_property()  = default;
  ~fenced_property() = default;

  template <typename OtherType>
  fenced_property(const OtherType &ot);

  template <typename OtherType>
  fenced_property& operator=(const OtherType &ot);

  fenced_property<T>& operator=(const fenced_property<T>& fp);

  T &ref();
  T  get();

  bool update();

private:

  bool newValue;
  T queuedValue;
  T currentValue;

  std::mutex mutex;
};

// Inlined fenced_property Members ////////////////////////////////////////////

template <typename T>
template <typename OtherType>
inline fenced_property<T>::fenced_property(const OtherType &ot)
{
  currentValue = ot;
}

template <typename T>
template <typename OtherType>
inline fenced_property<T> &fenced_property<T>::operator=(const OtherType &ot)
{
  std::lock_guard<std::mutex> lock{mutex};
  queuedValue = ot;
  newValue    = true;
}

template <typename T>
inline fenced_property<T> &
fenced_property<T>::operator=(const fenced_property<T> &fp)
{
  std::lock_guard<std::mutex> lock{mutex};
  queuedValue = fp.ref();
  newValue    = true;
}

template<typename T>
inline T &fenced_property<T>::ref()
{
  return currentValue;
}

template<typename T>
inline T fenced_property<T>::get()
{
  return currentValue;
}

template<typename T>
inline bool fenced_property<T>::update()
{
  bool didUpdate = false;
  if (newValue) {
    std::lock_guard<std::mutex> lock{mutex};
    currentValue = std::move(queuedValue);
    newValue     = false;
    didUpdate    = true;
  }

  return didUpdate;
}
