// ======================================================================== //
// Copyright 2016 SURVICE Engineering Company                               //
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

#include "imgui3D.h"

#include <imgui.h>
#include "imgui_impl_glfw_gl3.h"
#include <stdio.h>
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h> // for Sleep
#  define _USE_MATH_DEFINES
#  include <math.h> // M_PI
#else
#  include <sys/times.h>
#  include <unistd.h> // for usleep
#endif
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

extern "C" void glDrawPixels( GLsizei width, GLsizei height,
                              GLenum format, GLenum type,
                              const GLvoid *pixels );

namespace ospray {

  namespace imgui3D {

    bool dumpScreensDuringAnimation = false;

    static ImGui3DWidget *currentWidget = nullptr;

    // Class definitions //////////////////////////////////////////////////////

    FPSCounter::FPSCounter()
    {
      smooth_nom = 0.;
      smooth_den = 0.;
      frameStartTime = 0.;
    }
    
    void FPSCounter::startRender() 
    { 
      frameStartTime = ospcommon::getSysTime(); 
    }
    
    void FPSCounter::doneRender() {
      double seconds = ospcommon::getSysTime() - frameStartTime; 
      smooth_nom = smooth_nom * 0.8f + seconds;
      smooth_den = smooth_den * 0.8f + 1.f;
    }

    /*! write given frame buffer to file, in PPM P6 format. */
    void saveFrameBufferToFile(const char *fileName,
                               const uint32_t *pixel,
                               const uint32_t sizeX, const uint32_t sizeY)
    {
      FILE *file = fopen(fileName,"wb");
      if (!file) {
        std::cerr << "#osp:glut3D: Warning - could not create screenshot file '"
                  << fileName << "'" << std::endl;
        return;
      }
      fprintf(file,"P6\n%i %i\n255\n",sizeX,sizeY);
      unsigned char *out = (unsigned char *)alloca(3*sizeX);
      for (size_t y = 0; y < sizeY; y++) {
        const unsigned char *in =
            (const unsigned char *)&pixel[(sizeY-1-y)*sizeX];
        for (size_t x = 0; x < sizeX; x++) {
          out[3*x+0] = in[4*x+0];
          out[3*x+1] = in[4*x+1];
          out[3*x+2] = in[4*x+2];
        }
        fwrite(out, 3*sizeX, sizeof(char), file);
      }
      fprintf(file,"\n");
      fclose(file);
      std::cout << "#osp:glut3D: saved framebuffer to file "
                << fileName << std::endl;
    }

#define INVERT_RMB 
    /*! currently active window */
    ImGui3DWidget *ImGui3DWidget::activeWindow = nullptr;
    vec2i ImGui3DWidget::defaultInitSize(1024,768);

    bool animating = false;

    // InspectCenter Glut3DWidget::INSPECT_CENTER;
    /*! viewport as specified on the command line */
    ImGui3DWidget::ViewPort *viewPortFromCmdLine = nullptr;
    vec3f upVectorFromCmdLine(0,1,0);

    // ------------------------------------------------------------------
    // glut event handlers
    // ------------------------------------------------------------------

    void glut3dReshape(int32_t x, int32_t y)
    {
      if (ImGui3DWidget::activeWindow)
        ImGui3DWidget::activeWindow->reshape(vec2i(x,y));
    }

    void glut3dDisplay()
    {
      if (animating && ImGui3DWidget::activeWindow &&
          ImGui3DWidget::activeWindow->inspectCenterManipulator) {
        auto *hack =
            (InspectCenter*)ImGui3DWidget::activeWindow->inspectCenterManipulator;
        hack->rotate(-10.f * ImGui3DWidget::activeWindow->motionSpeed, 0);
      }
      if (ImGui3DWidget::activeWindow)
        ImGui3DWidget::activeWindow->display();
    }

    void glut3dKeyboard(unsigned char key, int32_t x, int32_t y)
    {
      if (ImGui3DWidget::activeWindow)
        ImGui3DWidget::activeWindow->keypress(key,vec2i(x,y));
    }
    void glut3dSpecial(int32_t key, int32_t x, int32_t y)
    {
      if (ImGui3DWidget::activeWindow)
        ImGui3DWidget::activeWindow->specialkey(key,vec2i(x,y));
    }

    void glut3dIdle()
    {
      if (ImGui3DWidget::activeWindow)
        ImGui3DWidget::activeWindow->idle();
    }
    void glut3dMotionFunc(int32_t x, int32_t y)
    {
      if (ImGui3DWidget::activeWindow)
        ImGui3DWidget::activeWindow->motion(vec2i(x,y));
    }

    void glut3dMouseFunc(int32_t whichButton, int32_t released,
                         int32_t x, int32_t y)
    {
      if (ImGui3DWidget::activeWindow)
        ImGui3DWidget::activeWindow->mouseButton(whichButton,released,vec2i(x,y));
    }


    // ------------------------------------------------------------------
    // implementation of glut3d::viewPorts
    // ------------------------------------------------------------------
    ImGui3DWidget::ViewPort::ViewPort() :
      modified(true),
      from(0,0,-1),
      at(0,0,0),
      up(upVectorFromCmdLine),
      openingAngle(60.f),
      aspect(1.f)
    {
      frame = AffineSpace3fa::translate(from) * AffineSpace3fa(ospcommon::one);
    }

    void ImGui3DWidget::ViewPort::snapUp()
    {
      if (fabsf(dot(up,frame.l.vz)) < 1e-3f)
        return;
      frame.l.vx = normalize(cross(frame.l.vy,up));
      frame.l.vz = normalize(cross(frame.l.vx,frame.l.vy));
      frame.l.vy = normalize(cross(frame.l.vz,frame.l.vx));
    }

    // ------------------------------------------------------------------
    // implementation of glut3d widget
    // ------------------------------------------------------------------
    void ImGui3DWidget::mouseButton(int32_t whichButton,
                                   bool released,
                                   const vec2i &pos)
    {

      if (pos != currMousePos)
        motion(pos);
      lastButtonState = currButtonState;

      if (released)
        currButtonState = currButtonState & ~(1<<whichButton);
      else
        currButtonState = currButtonState |  (1<<whichButton);
#if 0
      currModifiers = glutGetModifiers();
#endif

      manipulator->button(this, pos);
    }

    void ImGui3DWidget::motion(const vec2i &pos)
    {
      currMousePos = pos;
      if (currButtonState != lastButtonState) {
        // some button got pressed; reset 'old' pos to new pos.
        lastMousePos = currMousePos;
        lastButtonState = currButtonState;
      }

      manipulator->motion(this);
      lastMousePos = currMousePos;
    }

    ImGui3DWidget::ImGui3DWidget(FrameBufferMode frameBufferMode,
                                 ManipulatorMode initialManipulator,
                                 int allowedManipulators) :
      lastMousePos(-1,-1),
      currMousePos(-1,-1),
      lastButtonState(0),
      currButtonState(0),
      currModifiers(0),
      windowID(-1),
      windowSize(-1,-1),
      motionSpeed(.003f),
      rotateSpeed(.003f),
      frameBufferMode(frameBufferMode),
      ucharFB(nullptr)
    {
      worldBounds.lower = vec3f(-1);
      worldBounds.upper = vec3f(+1);

      if (allowedManipulators & INSPECT_CENTER_MODE) {
        inspectCenterManipulator = new InspectCenter(this);
      }
      if (allowedManipulators & MOVE_MODE) {
        moveModeManipulator = new MoveMode(this);
      }
      switch(initialManipulator) {
      case MOVE_MODE:
        manipulator = moveModeManipulator;
        break;
      case INSPECT_CENTER_MODE:
        manipulator = inspectCenterManipulator;
        break;
      }
      Assert2(manipulator != nullptr,"invalid initial manipulator mode");

      if (viewPortFromCmdLine) {
        viewPort = *viewPortFromCmdLine;

        if (length(viewPort.up) < 1e-3f)
          viewPort.up = vec3f(0,0,1.f);

        this->worldBounds = worldBounds;
        computeFrame();
      }
    }

    void ImGui3DWidget::computeFrame()
    {
      viewPort.frame.l.vy = normalize(viewPort.at - viewPort.from);
      viewPort.frame.l.vx = normalize(cross(viewPort.frame.l.vy,viewPort.up));
      viewPort.frame.l.vz =
          normalize(cross(viewPort.frame.l.vx,viewPort.frame.l.vy));
      viewPort.frame.p    = viewPort.from;
      viewPort.snapUp();
      viewPort.modified = true;
    }

    void ImGui3DWidget::setZUp(const vec3f &up)
    {
      viewPort.up = up;
      if (up != vec3f(0.f))
        viewPort.snapUp();
    }

    void ImGui3DWidget::idle()
    {
#ifdef _WIN32
      Sleep(1);
#else
      usleep(1000);
#endif
    }

    void ImGui3DWidget::reshape(const vec2i &newSize)
    {
      windowSize = newSize;
      viewPort.aspect = newSize.x/float(newSize.y);
    }

    void ImGui3DWidget::activate()
    {
      activeWindow = this;
#if 0
      glutSetWindow(windowID);
#endif
    }

    void ImGui3DWidget::display()
    {
      if (frameBufferMode == ImGui3DWidget::FRAMEBUFFER_UCHAR && ucharFB) {
        glDrawPixels(windowSize.x, windowSize.y,
                     GL_RGBA, GL_UNSIGNED_BYTE, ucharFB);
#ifndef _WIN32
        if (animating && dumpScreensDuringAnimation) {
          char tmpFileName[] = "/tmp/ospray_scene_dump_file.XXXXXXXXXX";
          static const char *dumpFileRoot;
          if (!dumpFileRoot) 
            dumpFileRoot = getenv("OSPRAY_SCREEN_DUMP_ROOT");
          if (!dumpFileRoot) {
            auto rc = mkstemp(tmpFileName);
            (void)rc;
            dumpFileRoot = tmpFileName;
          }

          char fileName[100000];
          sprintf(fileName,"%s_%08ld.ppm",dumpFileRoot,times(nullptr));
          saveFrameBufferToFile(fileName,ucharFB,windowSize.x,windowSize.y);
        }
#endif
      } else if (frameBufferMode == ImGui3DWidget::FRAMEBUFFER_FLOAT && floatFB) {
        glDrawPixels(windowSize.x, windowSize.y, GL_RGBA, GL_FLOAT, floatFB);
      } else {
        glClearColor(0.f,0.f,0.f,1.f);
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
      }
    }

    void ImGui3DWidget::buildGui()
    {
      bool show_test_window = true;
      bool show_another_window = false;

      ImVec4 clear_color = ImColor(114, 144, 154);

      // 1. Show a simple window
      // Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears
      //      in a window automatically called "Debug"
      {
        static float f = 0.0f;
        ImGui::Text("Hello, world!");
        ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
        ImGui::ColorEdit3("clear color", (float*)&clear_color);
        if (ImGui::Button("Test Window")) show_test_window ^= 1;
        if (ImGui::Button("Another Window")) show_another_window ^= 1;
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                    1000.0f / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate);
      }

      // 2. Show another window, this time using an explicit Begin/End pair
      if (show_another_window)
      {
        ImGui::SetNextWindowSize(ImVec2(200,100), ImGuiSetCond_FirstUseEver);
        ImGui::Begin("Another Window", &show_another_window);
        ImGui::Text("Hello");
        ImGui::End();
      }
    }

    void ImGui3DWidget::setViewPort(const vec3f from,
                                   const vec3f at,
                                   const vec3f up)
    {
      const vec3f dir = at - from;
      viewPort.at    = at;
      viewPort.from  = from;
      viewPort.up    = up;

      this->worldBounds = worldBounds;
      viewPort.frame.l.vy = normalize(dir);
      viewPort.frame.l.vx = normalize(cross(viewPort.frame.l.vy,up));
      viewPort.frame.l.vz =
          normalize(cross(viewPort.frame.l.vx,viewPort.frame.l.vy));
      viewPort.frame.p    = from;
      viewPort.snapUp();
      viewPort.modified = true;
    }

    void ImGui3DWidget::setWorldBounds(const box3f &worldBounds)
    {
      vec3f center = ospcommon::center(worldBounds);
      vec3f diag   = worldBounds.size();
      diag         = max(diag,vec3f(0.3f*length(diag)));
      vec3f from   = center - .75f*vec3f(-.6*diag.x,-1.2f*diag.y,.8f*diag.z);
      vec3f dir    = center - from;
      vec3f up     = viewPort.up;

      if (!viewPortFromCmdLine) {
        viewPort.at    = center;
        viewPort.from  = from;
        viewPort.up    = up;

        if (length(up) < 1e-3f)
          up = vec3f(0,0,1.f);

        this->worldBounds = worldBounds;
        viewPort.frame.l.vy = normalize(dir);
        viewPort.frame.l.vx = normalize(cross(viewPort.frame.l.vy,up));
        viewPort.frame.l.vz =
            normalize(cross(viewPort.frame.l.vx,viewPort.frame.l.vy));
        viewPort.frame.p    = from;
        viewPort.snapUp();
        viewPort.modified = true;
      }
      motionSpeed = length(diag) * .001f;
    }

    void ImGui3DWidget::setTitle(const char *title)
    {
      Assert2(windowID >= 0,
              "must call Glut3DWidget::create() before calling setTitle()");
#if 0
      glutSetWindow(windowID);
      glutSetWindowTitle(title);
#endif
    }

    void ImGui3DWidget::create(const char *title,
                               const vec2i &size,
                               bool fullScreen)
    {
      // Setup window
      auto error_callback = [](int error, const char* description) {
        fprintf(stderr, "Error %d: %s\n", error, description);
      };

      glfwSetErrorCallback(error_callback);

      if (!glfwInit())
        throw std::runtime_error("Could not initialize glfw!");

      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
      glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#if 1
      glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
#else
      glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
#if __APPLE__
      glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

      window = glfwCreateWindow(1280, 720, "ospImGui App", nullptr, nullptr);

      glfwMakeContextCurrent(window);
      gl3wInit();

      // NOTE(jda) - move key handler registration into this class
      ImGui_ImplGlfwGL3_Init(window, true);

      currentWidget = this;
    }

    void run()
    {
      if (!currentWidget)
        throw std::runtime_error("ImGuiViewer window not created/set!");

      auto *window = currentWidget->window;

      int display_w = 0, display_h = 0;

      // Main loop
      while (!glfwWindowShouldClose(window))
      {
        glfwPollEvents();
        ImGui_ImplGlfwGL3_NewFrame();

        currentWidget->buildGui();

        int new_w = 0, new_h = 0;
        glfwGetFramebufferSize(window, &new_w, &new_h);

        if (new_w != display_w || new_h != display_h)
        {
          display_w = new_w;
          display_h = new_h;
          currentWidget->reshape(vec2i(display_w, display_h));
        }

        glViewport(0, 0, new_w, new_h);
        glClear(GL_COLOR_BUFFER_BIT);

        // Render OSPRay frame
        currentWidget->display();

        // Render GUI
        ImGui::Render();

        glfwSwapBuffers(window);
      }

      // Cleanup
      ImGui_ImplGlfwGL3_Shutdown();
      glfwTerminate();
    }

    void init(int32_t *ac, const char **av)
    {
      for(int i = 1; i < *ac;i++)
      {
        std::string arg(av[i]);
        if (arg == "-win") {
          std::string arg2(av[i+1]);
          size_t pos = arg2.find("x");
          if (pos != std::string::npos) {
            arg2.replace(pos, 1, " ");
            std::stringstream ss(arg2);
            ss >> ImGui3DWidget::defaultInitSize.x
               >> ImGui3DWidget::defaultInitSize.y;
            removeArgs(*ac,(char **&)av,i,2); --i;
          } else {
            ImGui3DWidget::defaultInitSize.x = atoi(av[i+1]);
            ImGui3DWidget::defaultInitSize.y = atoi(av[i+1]);
            removeArgs(*ac,(char **&)av,i,3); --i;
          }
          continue;
        } if (arg == "--1k" || arg == "-1k") {
          ImGui3DWidget::defaultInitSize.x =
              ImGui3DWidget::defaultInitSize.y = 1024;
          removeArgs(*ac,(char **&)av,i,1); --i;
          continue;
        } if (arg == "--size") {
          ImGui3DWidget::defaultInitSize.x = atoi(av[i+1]);
          ImGui3DWidget::defaultInitSize.y = atoi(av[i+2]);
          removeArgs(*ac,(char **&)av,i,3); --i;
          continue;
        } if (arg == "-v" || arg == "--view") {
          std::ifstream fin(av[i+1]);
          if (!fin.is_open())
          {
            throw std::runtime_error("Failed to open \"" +
                                     std::string(av[i+1]) +
                                     "\" for reading");
          }

          if (!viewPortFromCmdLine)
            viewPortFromCmdLine = new ImGui3DWidget::ViewPort;

          auto& fx = viewPortFromCmdLine->from.x;
          auto& fy = viewPortFromCmdLine->from.y;
          auto& fz = viewPortFromCmdLine->from.z;

          auto& ax = viewPortFromCmdLine->at.x;
          auto& ay = viewPortFromCmdLine->at.y;
          auto& az = viewPortFromCmdLine->at.z;

          auto& ux = upVectorFromCmdLine.x;
          auto& uy = upVectorFromCmdLine.y;
          auto& uz = upVectorFromCmdLine.z;

          auto& fov = viewPortFromCmdLine->openingAngle;

          auto token = std::string("");
          while (fin >> token)
          {
            if (token == "-vp")
              fin >> fx >> fy >> fz;
            else if (token == "-vu")
              fin >> ux >> uy >> uz;
            else if (token == "-vi")
              fin >> ax >> ay >> az;
            else if (token == "-fv")
              fin >> fov;
            else
            {
              throw std::runtime_error("Unrecognized token:  \"" + token +
                                       '\"');
            }
          }

          assert(i+1 < *ac);
          removeArgs(*ac,(char **&)av, i, 2); --i;
          continue;
        } if (arg == "-vu") {
          if (!viewPortFromCmdLine)
            viewPortFromCmdLine = new ImGui3DWidget::ViewPort;
          upVectorFromCmdLine.x = atof(av[i+1]);
          upVectorFromCmdLine.y = atof(av[i+2]);
          upVectorFromCmdLine.z = atof(av[i+3]);
          if (viewPortFromCmdLine)
            viewPortFromCmdLine->up = upVectorFromCmdLine;
          assert(i+3 < *ac);
          removeArgs(*ac,(char **&)av,i,4); --i;
          continue;
        } if (arg == "-vp") {
          if (!viewPortFromCmdLine)
            viewPortFromCmdLine = new ImGui3DWidget::ViewPort;
          viewPortFromCmdLine->from.x = atof(av[i+1]);
          viewPortFromCmdLine->from.y = atof(av[i+2]);
          viewPortFromCmdLine->from.z = atof(av[i+3]);
          assert(i+3 < *ac);
          removeArgs(*ac,(char **&)av,i,4); --i;
          continue;
        } if (arg == "-vi") {
          if (!viewPortFromCmdLine)
            viewPortFromCmdLine = new ImGui3DWidget::ViewPort;
          viewPortFromCmdLine->at.x = atof(av[i+1]);
          viewPortFromCmdLine->at.y = atof(av[i+2]);
          viewPortFromCmdLine->at.z = atof(av[i+3]);
          assert(i+3 < *ac);
          removeArgs(*ac,(char **&)av,i,4); --i;
          continue;
        }
      }
    }

    // ------------------------------------------------------------------
    // base manipulator
    // ------------------------------------------------------------------
    void Manipulator::motion(ImGui3DWidget *widget)
    {
#if 0
      if ((widget->currButtonState == (1<<GLUT_RIGHT_BUTTON))
          ||
          ((widget->currButtonState == (1<<GLUT_LEFT_BUTTON)) 
           && 
           (widget->currModifiers & GLUT_ACTIVE_ALT))
          ) {
        dragRight(widget,widget->currMousePos,widget->lastMousePos);
      } else if ((widget->currButtonState == (1<<GLUT_MIDDLE_BUTTON)) 
                 ||
                 ((widget->currButtonState == (1<<GLUT_LEFT_BUTTON)) 
                  && 
                  (widget->currModifiers & GLUT_ACTIVE_CTRL))
                 ) {
        dragMiddle(widget,widget->currMousePos,widget->lastMousePos);
      } else if (widget->currButtonState == (1<<GLUT_LEFT_BUTTON)) {
        dragLeft(widget,widget->currMousePos,widget->lastMousePos);
      } 
#endif
    }

    void Manipulator::button(ImGui3DWidget *widget, const vec2i &pos)
    {
    }

    // ------------------------------------------------------------------
    // INSPECT_CENTER manipulator
    // ------------------------------------------------------------------
    InspectCenter::InspectCenter(ImGui3DWidget *widget)
      : Manipulator(widget)
      , pivot(ospcommon::center(widget->worldBounds))
    {}

    void InspectCenter::keypress(ImGui3DWidget *widget,
                                 int32_t key)
    {
      switch(key) {
      case 'a': {
        rotate(+10.f*widget->rotateSpeed,0);
      } return;
      case 'd': {
        rotate(-10.f*widget->rotateSpeed,0);
      } return;
      case 'w': {
        rotate(0,+10.f*widget->rotateSpeed);
      } return;
      case 's': {
        rotate(0,-10.f*widget->rotateSpeed);
      } return;
      }

      Manipulator::keypress(widget,key);
    }

    void InspectCenter::button(ImGui3DWidget *widget, const vec2i &pos)
    {
    }

    void InspectCenter::rotate(float du, float dv)
    {
      ImGui3DWidget::ViewPort &cam = widget->viewPort;
      const vec3f pivot = widget->viewPort.at;//center(widget->worldBounds);
      AffineSpace3fa xfm
        = AffineSpace3fa::translate(pivot)
        * AffineSpace3fa::rotate(cam.frame.l.vx,-dv)
        * AffineSpace3fa::rotate(cam.frame.l.vz,-du)
        * AffineSpace3fa::translate(-pivot);
      cam.frame = xfm * cam.frame;
      cam.from  = xfmPoint(xfm,cam.from);
      cam.at    = xfmPoint(xfm,cam.at);
      cam.snapUp();
      cam.modified = true;
    }

    void InspectCenter::specialkey(ImGui3DWidget *widget,
                                   int32_t key)
    {
#if 0
      switch(key) {
      case GLUT_KEY_LEFT: {
        rotate(+10.f*widget->rotateSpeed,0);
      } return;
      case GLUT_KEY_RIGHT: {
        rotate(-10.f*widget->rotateSpeed,0);
      } return;
      case GLUT_KEY_UP: {
        rotate(0,+10.f*widget->rotateSpeed);
      } return;
      case GLUT_KEY_DOWN: {
        rotate(0,-10.f*widget->rotateSpeed);
      } return;
      }
#endif
      Manipulator::specialkey(widget,key);
    }

    /*! INSPECT_CENTER::RightButton: move lookfrom/viewPort positoin
      forward/backward on right mouse button */
    void InspectCenter::dragRight(ImGui3DWidget *widget,
                                  const vec2i &to, const vec2i &from)
    {
      ImGui3DWidget::ViewPort &cam = widget->viewPort;
      float fwd = 
#ifdef INVERT_RMB
#else
        - 
#endif
        (to.y - from.y) * 4 * widget->motionSpeed;
      // * length(widget->worldBounds.size());
      float oldDist = length(cam.at - cam.from);
      float newDist = oldDist - fwd;
      if (newDist < 1e-3f)
        return;
      cam.from = cam.at - newDist * cam.frame.l.vy;
      cam.frame.p = cam.from;
      cam.modified = true;
    }

    /*! INSPECT_CENTER::MiddleButton: move lookat/center of interest
      forward/backward on middle mouse button */
    void InspectCenter::dragMiddle(ImGui3DWidget *widget,
                                   const vec2i &to, const vec2i &from)
    {
      ImGui3DWidget::ViewPort &cam = widget->viewPort;
      float du = (to.x - from.x);
      float dv = (to.y - from.y);

      AffineSpace3fa xfm =
          AffineSpace3fa::translate(widget->motionSpeed * dv * cam.frame.l.vz )
        * AffineSpace3fa::translate(-1.0f * widget->motionSpeed
                                    * du * cam.frame.l.vx);

      cam.frame = xfm * cam.frame;
      cam.from = xfmPoint(xfm, cam.from);
      cam.at = xfmPoint(xfm, cam.at);
      cam.modified = true;
    }

    void InspectCenter::dragLeft(ImGui3DWidget *widget,
                                 const vec2i &to, const vec2i &from)
    {
      ImGui3DWidget::ViewPort &cam = widget->viewPort;
      float du = (to.x - from.x) * widget->rotateSpeed;
      float dv = (to.y - from.y) * widget->rotateSpeed;

      const vec3f pivot = cam.at;
      AffineSpace3fa xfm
        = AffineSpace3fa::translate(pivot)
        * AffineSpace3fa::rotate(cam.frame.l.vx,-dv)
        * AffineSpace3fa::rotate(cam.frame.l.vz,-du)
        * AffineSpace3fa::translate(-pivot);
      cam.frame = xfm * cam.frame;
      cam.from  = xfmPoint(xfm,cam.from);
      cam.at    = xfmPoint(xfm,cam.at);
      cam.snapUp();
      cam.modified = true;
    }



    // ------------------------------------------------------------------
    // MOVE_MOVE manipulator - TODO.
    // ------------------------------------------------------------------

    /*! \brief key press events for move mode

      Right now, recognizes the following move modes:

      - w : move forward
      - s : move backward
      - a : pan left
      - d : pan right

    */
    void MoveMode::keypress(ImGui3DWidget *widget,
                            int32_t key)
    {
      ImGui3DWidget::ViewPort &cam = widget->viewPort;
      switch(key) {
      case 'w': {
        float fwd = 8 * widget->motionSpeed;
        cam.from = cam.from + fwd * cam.frame.l.vy;
        cam.at   = cam.at   + fwd * cam.frame.l.vy;
        cam.frame.p = cam.from;
        cam.modified = true;
      } return;
      case 's': {
        float fwd = 8 * widget->motionSpeed;
        cam.from = cam.from - fwd * cam.frame.l.vy;
        cam.at   = cam.at   - fwd * cam.frame.l.vy;
        cam.frame.p = cam.from;
        cam.modified = true;
      } return;
      case 'd': {
        float fwd = 8 * widget->motionSpeed;
        cam.from = cam.from + fwd * cam.frame.l.vx;
        cam.at   = cam.at   + fwd * cam.frame.l.vx;
        cam.frame.p = cam.from;
        cam.modified = true;
      } return;
      case 'a': {
        float fwd = 8 * widget->motionSpeed;
        cam.from = cam.from - fwd * cam.frame.l.vx;
        cam.at   = cam.at   - fwd * cam.frame.l.vx;
        cam.frame.p = cam.from;
        cam.modified = true;
      } return;
      }
      Manipulator::keypress(widget,key);
    }

    void MoveMode::dragRight(ImGui3DWidget *widget,
                             const vec2i &to, const vec2i &from)
    {
      ImGui3DWidget::ViewPort &cam = widget->viewPort;
      float fwd = 
#ifdef INVERT_RMB
#else
        - 
#endif
        (to.y - from.y) * 4 * widget->motionSpeed;
      cam.from = cam.from + fwd * cam.frame.l.vy;
      cam.at   = cam.at   + fwd * cam.frame.l.vy;
      cam.frame.p = cam.from;
      cam.modified = true;
    }

    /*! todo */
    void MoveMode::dragMiddle(ImGui3DWidget *widget,
                              const vec2i &to, const vec2i &from)
    {
      ImGui3DWidget::ViewPort &cam = widget->viewPort;
      float du = (to.x - from.x);
      float dv = (to.y - from.y);

      AffineSpace3fa xfm = AffineSpace3fa::translate( widget->motionSpeed * dv * cam.frame.l.vz )
        * AffineSpace3fa::translate( -1.0f * widget->motionSpeed * du * cam.frame.l.vx );

      cam.frame = xfm * cam.frame;
      cam.from = xfmPoint(xfm, cam.from);
      cam.at = xfmPoint(xfm, cam.at);
      cam.modified = true;
    }

    void MoveMode::dragLeft(ImGui3DWidget *widget,
                            const vec2i &to, const vec2i &from)
    {
      ImGui3DWidget::ViewPort &cam = widget->viewPort;
      float du = (to.x - from.x) * widget->rotateSpeed;
      float dv = (to.y - from.y) * widget->rotateSpeed;

      const vec3f pivot = cam.from; //center(widget->worldBounds);
      AffineSpace3fa xfm
        = AffineSpace3fa::translate(pivot)
        * AffineSpace3fa::rotate(cam.frame.l.vx,-dv)
        * AffineSpace3fa::rotate(cam.frame.l.vz,-du)
        * AffineSpace3fa::translate(-pivot);
      cam.frame = xfm * cam.frame;
      cam.from  = xfmPoint(xfm,cam.from);
      cam.at    = xfmPoint(xfm,cam.at);
      cam.snapUp();
      cam.modified = true;
    }

    void ImGui3DWidget::specialkey(int32_t key, const vec2i &where)
    {
      if (manipulator) manipulator->specialkey(this,key);
    }
    void ImGui3DWidget::keypress(char key, const vec2i &where)
    {
      if (key == '!') {
        if (animating) {
          dumpScreensDuringAnimation = !dumpScreensDuringAnimation;
        } else {
          char tmpFileName[] = "/tmp/ospray_screen_dump_file.XXXXXXXX";
          static const char *dumpFileRoot;
          if (!dumpFileRoot) 
            dumpFileRoot = getenv("OSPRAY_SCREEN_DUMP_ROOT");
#ifndef _WIN32
          if (!dumpFileRoot) {
            auto rc = mkstemp(tmpFileName);
            (void)rc;
            dumpFileRoot = tmpFileName;
          }
#endif
          char fileName[100000];
          static int frameDumpSequenceID = 0;
          sprintf(fileName,"%s_%05d.ppm",dumpFileRoot,frameDumpSequenceID++);
          if (ucharFB)
            saveFrameBufferToFile(fileName,ucharFB,windowSize.x,windowSize.y);
          return;
        }
      } 

      if (key == 'C') {
        PRINT(viewPort);
        return;
      }
      if (key == 'I' && inspectCenterManipulator) {
        // 'i'nspect mode
        manipulator = inspectCenterManipulator;
        return;
      }
      if ((key == 'M' || key == 'F') && moveModeManipulator) {
        manipulator = moveModeManipulator;
        return;
      }
      if (key == 'F' && moveModeManipulator) {
        // 'f'ly mode
        manipulator = moveModeManipulator;
        return;
      }
      if (key == 'A' && inspectCenterManipulator) {
        animating = !animating;
        return;
      }
      if (key == '+') { 
        motionSpeed *= 1.5f; 
        std::cout << "glut3d: new motion speed " << motionSpeed << std:: endl;
        return; 
      }
      if (key == '-') { 
        motionSpeed /= 1.5f; 
        std::cout << "glut3d: new motion speed " << motionSpeed << std:: endl;
        return; 
      }
      if (manipulator) manipulator->keypress(this,key);
    }


    void Manipulator::keypress(ImGui3DWidget *widget, const int32_t key)
    {
      switch(key) {
      case 27 /*ESC*/:
      case 'q':
      case 'Q':
        std::exit(0);
      }
    }

    void Manipulator::specialkey(ImGui3DWidget *widget, const int32_t key)
    {
    }

    std::ostream &operator<<(std::ostream &o, const ImGui3DWidget::ViewPort &cam)
    {
      o << "// "
        << " -vp " << cam.from.x << " " << cam.from.y << " " << cam.from.z
        << " -vi " << cam.at.x << " " << cam.at.y << " " << cam.at.z
        << " -vu " << cam.up.x << " " << cam.up.y << " " << cam.up.z
        << std::endl;
      o << "<viewPort>" << std::endl;
      o << "  <from>" << cam.from.x << " " << cam.from.y << " " << cam.from.z << "</from>" << std::endl;
      o << "  <at>" << cam.at.x << " " << cam.at.y << " " << cam.at.z << "</at>" << std::endl;
      o << "  <up>" << cam.up.x << " " << cam.up.y << " " << cam.up.z << "</up>" << std::endl;
      o << "  <aspect>" << cam.aspect << "</aspect>" << std::endl;
      o << "  <frame.dx>" << cam.frame.l.vx << "</frame.dx>" << std::endl;
      o << "  <frame.dy>" << cam.frame.l.vy << "</frame.dy>" << std::endl;
      o << "  <frame.dz>" << cam.frame.l.vz << "</frame.dz>" << std::endl;
      o << "  <frame.p>" << cam.frame.p << "</frame.p>" << std::endl;
      o << "</viewPort>";
      return o;
    }
  }
}
