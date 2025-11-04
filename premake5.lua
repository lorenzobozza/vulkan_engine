require "export-compile-commands"

workspace "PrototypeEngine"
	configurations { "Debug", "Release" }
	location "build"

project "Core"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++23"

	files {
		"source/**.cpp",
		"external/imgui/*.cpp",
		"external/mikktspace/*.c",
		"external/enkits/*.cpp"
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

	filter "system:windows"
		files { "external/nfd/nfd_win.cpp" }

	filter "system:macosx"
		files { "external/nfd/nfd_cocoa.m" }
		links {
			"AppKit.framework",
			"UniformTypeIdentifiers.framework"
		}

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"
