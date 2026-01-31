workspace "VulkanEngine"
	configurations { "Debug", "Release" }
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
		"freetype.6"
	}

	libdirs {
		"external/**/lib",
		"$(VULKAN_SDK)/Lib"
	}

	filter "system:macos"
		links {"vulkan.1", "shaderc_shared.1"}

	filter "system:windows"
		links {"vulkan-1", "shaderc_shared"}
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
