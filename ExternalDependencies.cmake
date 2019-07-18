cmake_minimum_required(VERSION 3.6)

# Include logi scene graph
if (NOT TARGET logi_scene_graph)
    add_subdirectory(libs/LogiSceneGraph)
else ()
    message(STATUS "[LogiPathTracer] Target logi_scene_graph is already defined. Using existing target.")
endif ()

# Include logi scene graph
if (NOT TARGET logi)
    add_subdirectory(libs/Logi)
else ()
    message(STATUS "[LogiPathTracer] Target logi_scene_graph is already defined. Using existing target.")
endif ()

if (NOT TARGET cpp-glfw)
    add_subdirectory(libs/CppGLFW)
else ()
    message(STATUS "[LogiPathTracer] Target logi_scene_graph is already defined. Using existing target.")
endif ()