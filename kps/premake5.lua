
workspace "kps_ws"
    configurations { "Debug", "Release" }


project "excite"
    kind "ConsoleApp"
    language "C++"
    architecture "x86_64"
    toolset "msc"
    location "build/excite"
    targetdir "bin/%{cfg.buildcfg}"
    files { "src/excite/*.c", "src/excite/*.h", "src/excite/*.cpp"}
    includedirs { "include" }
    libdirs { "lib" }
    links { 
        "libjack64",
        "libjacknet64",
        "libjackserver64",
        "opengl32",
        "imgui",
        "glfw3"
    }
    filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

project "kps"
    kind "ConsoleApp"
    language "C++"
    architecture "x86_64"
    toolset "msc"
    location "build/kps"
    targetdir "bin/%{cfg.buildcfg}"
    files { "src/kps/*.c", "src/kps/*.h", "src/kps/*.cpp"}
    includedirs { "include" }
    libdirs { "lib" }
    links { 
        "libjack64",
        "libjacknet64",
        "libjackserver64",
        "opengl32",
        "imgui",
        "glfw3"
    }
    filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"