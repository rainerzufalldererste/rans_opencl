ProjectName = "rans_opencl"
project(ProjectName)

  --Settings
  kind "ConsoleApp"
  language "C++"
  flags { "StaticRuntime" }

  filter { "system:windows" }
    buildoptions { '/Gm-' }
    buildoptions { '/MP' }

    ignoredefaultlibraries { "msvcrt" }
  filter { "system:linux" }
    cppdialect "C++11"
  filter { }
  
  filter { "configurations:Release" }
    flags { "LinkTimeOptimization" }
  
  filter { }
  
  defines { "_CRT_SECURE_NO_WARNINGS", "SSE2", "GLEW_STATIC" }
  
  objdir "intermediate/obj"

  files { "src/**.cpp", "src/**.c", "src/**.cc", "src/**.h", "src/**.hh", "src/**.hpp", "src/**.inl", "src/**rc" }
  files { "project.lua" }
  
  includedirs { "3rdParty/OpenCL/include" }

  filter { "configurations:Debug", "system:Windows" }
    ignoredefaultlibraries { "libcmt" }
  filter { }

  filter { "system:linux" }
    libdirs { "../htCodec/3rdParty/OpenCL/lib/x64/", "../htCodec/3rdParty/zstd/lib/" }
    links { "pthread", "opencl" }

  filter { "system:windows" }
    links { "3rdParty/OpenCL/lib/$(Platform)/OpenCL.lib" }

  filter {}
  
  targetname(ProjectName)
  targetdir "builds/bin"
  debugdir "builds/bin"
  
filter {}
configuration {}

warnings "Extra"

filter {"configurations:Release"}
  targetname "%{prj.name}"
filter {"configurations:Debug"}
  targetname "%{prj.name}D"

filter {}
configuration {}
flags { "NoMinimalRebuild", "NoPCH" }
exceptionhandling "Off"
rtti "Off"
floatingpoint "Fast"

filter { "configurations:Debug*" }
	defines { "_DEBUG" }
	optimize "Off"
	symbols "On"

filter { "configurations:Release" }
	defines { "NDEBUG" }
	optimize "Speed"
	flags { "NoFramePointer", "NoBufferSecurityCheck" }
	symbols "On"

filter { "system:windows", "configurations:Release", "action:vs2012" }
	buildoptions { "/d2Zi+" }

filter { "system:windows", "configurations:Release", "action:vs2013" }
	buildoptions { "/Zo" }

filter { "system:windows", "configurations:Release" }
	flags { "NoIncrementalLink" }

filter {}
  flags { "NoFramePointer", "NoBufferSecurityCheck", "NoIncrementalLink" }

editandcontinue "Off"
