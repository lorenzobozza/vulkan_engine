workspace "VulkanEngine"
	configurations {"Release", "Debug"}
	location "build"

project "Acinonyx"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++23"

	files {
		"source/**.cpp",
		"external/imgui/*.cpp",
		"external/mikktspace/*.c",
		"external/enkits/*.cpp",
		"external/bullet/*.cpp",
		"external/bullet/HACD/*.cpp",
		"external/miniz/*.c"
	}

	includedirs {
		"source/include",
		"source/**/include"
	}

	externalincludedirs {
		"external/*",
		"external/utils/*",
		"external/**/include",
		"external/bullet",
		"external/miniz"
	}

	links {
		"SDL3",
        "shaderc_shared",
		"freetype"
	}

	libdirs {
		"external/**/lib",
	}

	filter "system:macosx or linux"
		links {"vulkan"}
        libdirs {"/usr/local/lib","/opt/homebrew/lib"}
        runpathdirs {"/usr/local/lib"}

	filter "system:windows"
		links {"vulkan-1"}
        libdirs {"$(VULKAN_SDK)/Lib"}
		debugdir "$(TargetDir)"
		architecture "x86_64"

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"
		optimize "Off"

	filter "configurations:Release"
		defines { "NDEBUG" }
		symbols "Off"
		optimize "On"
