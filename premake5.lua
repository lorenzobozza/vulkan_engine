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
		"external/bullet/HACD/*.cpp"
	}

	includedirs {
		"source/include",
		"source/**/include"
	}

	externalincludedirs {
		"external/*",
		"external/utils/*",
		"external/**/include",
		"external/bullet"
	}

	links {
		"vulkan.1.4.328",
		"shaderc_shared.1",
		"SDL3",
		"freetype.6"
	}

	libdirs {
		"external/**/lib"
	}

	filter "system:windows"
		debugdir "$(TargetDir)"
		architecture "x86_64"

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
