#pragma once

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "DXRay/DXRay.h"

using namespace Microsoft::WRL;

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

#if defined(_DEBUG)
#define THROW_IF_FAILED(hr)                                                                                            \
    if (FAILED(hr))                                                                                                    \
    {                                                                                                                  \
        __debugbreak();                                                                                                \
    }
#else
#define THROW_IF_FAILED(hr) hr;
#endif

#include "SimpleTimer.h"
#include "FileRead.h"
#include "Camera.h"
