
workspace "osc_ws"
    configurations { "Debug", "Release" }


project "terrain"
    kind "ConsoleApp"
    language "C"
    architecture "x86_64"
    toolset "msc"
    location "build/terrain"
    targetdir "bin/%{cfg.buildcfg}"
    files { "src/terrain/*.c", "src/terrain/*.h" }
    includedirs { "include" }
    libdirs { "lib" }
    links { 
        "libjack64",
        "libjacknet64",
        "libjackserver64",
    }
    filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"

project "phs"
    kind "ConsoleApp"
    language "C"
    architecture "x86_64"
    toolset "msc"
    location "build/phs"
    targetdir "bin/%{cfg.buildcfg}"
    files { "src/phs/*.c", "src/phs/*.h" }
    includedirs { "include" }
    libdirs { "lib" }
    links { 
        "libjack64",
        "libjacknet64",
        "libjackserver64",
    }
    filter "configurations:Debug"
      defines { "DEBUG" }
      symbols "On"

   filter "configurations:Release"
      defines { "NDEBUG" }
      optimize "On"