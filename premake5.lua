require "export-compile-commands"

workspace "PrototypeEngine"
	configurations { "Debug", "Release" }
	platforms {"arm64"}
	location "build"

project "Core"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++23"

	files {
		"**.c",
		"**.cpp"
	}

	includedirs {
		"source/include"
	}

	externalincludedirs {
		"external/*",
		"external/utils/*",
		"external/**/include"
	}

	links {
		"vulkan.1.3.236",
		"shaderc_shared.1",
		"SDL2-2.0.0",
		"SDL2_image-2.0.0",
		"tiff.6",
		"freetype.6"
	}

	libdirs {
		"external/**/lib"
	}

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
