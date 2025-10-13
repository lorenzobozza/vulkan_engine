require "export-compile-commands"

workspace "PrototypeEngine"
	configurations { "Debug", "Release" }
	platforms {"arm64"}
	location "build"

project "Core"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++17"

	defines { "SDL_DISABLE_IMMINTRIN_H" }

	files {
		"**.c",
		"**.cpp"
	}

	includedirs {
		"source/include",
		"external/*",
		"external/utils/*",
		"external/**/include"
	}

	links {
		"vulkan.1.3.236",
		"SDL2-2.0.0",
		"SDL2_image-2.0.0",
		"tiff-4.5.0",
		"freetype.6"
	}

	libdirs {
		"external/**/lib/**"
	}

	filter "configurations:Debug"
		defines { "DEBUG" }
		symbols "On"

	filter "configurations:Release"
		defines { "NDEBUG" }
		optimize "On"

	filter "action:xcode4"
		xcodebuildsettings {
			["OTHER_LDFLAGS"] = "-llibvulkan.1.3.236.dylib -llibSDL2-2.0.0.dylib -llibSDL2_image-2.0.0.dylib -llibtiff-4.5.0.dylib -llibfreetype.6.dylib"
		}
