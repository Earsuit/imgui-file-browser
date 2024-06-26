#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "ImFileDialog.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "misc/cpp/imgui_stdlib.h"

#include "stb_image.h"

#include "magic_enum/magic_enum_all.hpp"

#include <cmath>
#include <array>
#include <fstream>
#include <algorithm>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <lmcons.h>
#pragma comment(lib, "Shell32.lib")
#elif defined(__linux__)
#include <gio/gio.h>
#include <unistd.h>
#include <pwd.h>
#elif defined(__APPLE__)
#include <AppKit/AppKit.h>
#include <unistd.h>
#include <pwd.h>
#endif

#ifdef USE_GETTEXT
#include <libintl.h>
#define __(x) gettext(x)
#else
#define __(x) x
#endif

namespace ifd {
	constexpr auto DEFAULT_ICON_SIZE = 32;
	constexpr auto PI = 3.141592f;
	constexpr auto EXTRA_SIZE_FOR_ICON = 3.0f;
	constexpr auto EXTRA_SIZE_FOR_ELEMENT = 10.0f;
	constexpr auto ELEMENT_MAX_SIZE = 24.0f;
	constexpr auto IMGUI_LIGHT_THEME_WINDOW_BG = ImVec4(0.94f, 0.94f, 0.94f, 1.00f);
	constexpr auto DEFAULT_ICON_CHANNELS = 4;
	constexpr auto RGB_MASK = 0xFFFFFFU;
	constexpr auto ALPHA_MASK = 0xFF000000U;
	constexpr auto MOUSE_WHEEL_NOT_SCROLLING = 0.0f;
	constexpr auto ZOOM_LEVEL_RENDER_PREVIEW = 5.0f;
	constexpr auto ZOOM_LEVEL_LIST_VIEW = 1.0f;
	constexpr auto APPLE_ICON_DEFAULT_SCALE_LEVEL = 8;
	constexpr auto NUM_SYSTEM_ICON_PATH = 3;
	constexpr auto USER_ICON_PATH = "~/.icons";
	constexpr auto GLOBAL_ICON_PATH = "/usr/share/icons";

	enum class SizeUnit : uint8_t {
		B = 0,
		KiB,
		MiB,
		GiB,
		TiB
	};

#ifdef __linux__
	std::string getIconTheme() 
	{
		const auto settings = g_settings_new("org.gnome.desktop.interface");
		if (!settings) {
			fprintf(stderr, "Error creating GSettings object\n");
			return {};
		}
		const auto iconTheme = g_settings_get_string(settings, "icon-theme");
		g_object_unref(settings);
		return iconTheme;
	}
#endif

	void AlignForWidth(float width, float alignment = 0.5f)
	{
		ImGuiStyle& style = ImGui::GetStyle();
		float avail = ImGui::GetContentRegionAvail().x;
		float offset = (avail - width) * alignment;
		if (offset > 0.0f) {
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
		}
	}

	// we can't use std::common_type_t<Y...> in MSVC, it has a recursive limit 
	// https://learn.microsoft.com/en-us/cpp/error-messages/compiler-errors-1/fatal-error-c1202?view=msvc-170
	template <typename T, typename... Y>
	constexpr auto make_array(Y&&... values)
	{
		return std::array<T, sizeof...(Y)>{(static_cast<T>(values))...};
	}

	constexpr auto DEFAULT_FILE_ICON = make_array<uint32_t>(
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x4c000000, 0xf5000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xdd000000, 0x2d000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0xd1000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6a000000, 0xa1000000, 0xff000000, 0xff000000, 0x2e000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x54000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x46000000, 0xf5000000, 0xe0000000, 0xff000000, 0x30000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6e000000, 0xf8000000, 0x01000000, 0xc3000000, 0xff000000, 0x30000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00000000, 0x00000000, 0xd2000000, 0xff000000, 0x30000000, 0x00000000, 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x13000000, 0x00000000, 0x00000000, 0xd2000000, 0xff000000, 0x30000000, 0x00000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x73000000, 0xff000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xbe000000, 0xff000000, 0x30000000, 0x00000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x65000000, 0xff000000, 0x34000000, 0x10000000, 0x10000000, 0x03000000, 0x0a000000, 0xdb000000, 0xff000000, 0x2f000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0f000000, 0xd9000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xed000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x06000000, 0x5e000000, 0x6c000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x60000000, 0x9e000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x52000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6b000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6b000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0x54000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x54000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0xff000000, 0xd2000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0x6b000000, 0xd2000000, 0xff000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x4c000000, 0xf5000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xf5000000, 0x4b000000, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff
	);

	constexpr auto DEFAULT_FOLDER_ICON = make_array<uint32_t>(
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00000000, 0x00000000, 0x45000000, 0x8a000000, 0x99000000, 0x97000000, 0x97000000, 0x97000000, 0x97000000, 0x97000000, 0x98000000, 0x81000000, 0x35000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x00000000, 0x9e000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0x80000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0x76000000, 0xff000000, 0xff000000, 0xf6000000, 0xe2000000, 0xe2000000, 0xe2000000, 0xe2000000, 0xe2000000, 0xe2000000, 0xe2000000, 0xff000000, 0xff000000, 0xff000000, 0x80000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0xe7000000, 0xff000000, 0xbe000000, 0x11000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x1e000000, 0xd1000000, 0xff000000, 0xff000000, 0x75000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0xfa000000, 0xff000000, 0x5a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x06000000, 0xe0000000, 0xff000000, 0xff000000, 0x68000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
		0xf4000000, 0xff000000, 0x67000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x11000000, 0xe4000000, 0xff000000, 0xff000000, 0xad000000, 0x94000000, 0x94000000, 0x94000000, 0x94000000, 0x94000000, 0x94000000, 0x94000000, 0x94000000, 0x94000000, 0x96000000, 0x8b000000, 0x4f000000, 0x00000000, 0x00000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x17000000, 0xe8000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xaf000000, 0x00000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x0e000000, 0x88000000, 0xc3000000, 0xcd000000, 0xcc000000, 0xcc000000, 0xcc000000, 0xcc000000, 0xcc000000, 0xcc000000, 0xcc000000, 0xcb000000, 0xcc000000, 0xe2000000, 0xff000000, 0xff000000, 0x81000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0xb6000000, 0xff000000, 0xec000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x5b000000, 0xff000000, 0xf9000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x68000000, 0xff000000, 0xf4000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
		0xf3000000, 0xff000000, 0x6a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x6a000000, 0xff000000, 0xf3000000,
		0xf4000000, 0xff000000, 0x68000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x68000000, 0xff000000, 0xf4000000,
		0xfa000000, 0xff000000, 0x5a000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x5a000000, 0xff000000, 0xf9000000,
		0xea000000, 0xff000000, 0xb5000000, 0x05000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x05000000, 0xb5000000, 0xff000000, 0xea000000,
		0x7e000000, 0xff000000, 0xff000000, 0xeb000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xd6000000, 0xeb000000, 0xff000000, 0xff000000, 0x7f000000,
		0x00000000, 0xac000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xff000000, 0xac000000, 0x00000000,
		0x00000000, 0x00000000, 0x53000000, 0x8f000000, 0x9a000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x99000000, 0x9a000000, 0x8f000000, 0x53000000, 0x00000000, 0x00000000,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff,
		0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff
	);

	float computeIconSize(float fontSize)
	{
		return fontSize + EXTRA_SIZE_FOR_ICON;
	}

	float computeGuiElementSize(float fontSize)
	{
		return std::max<float>(fontSize + EXTRA_SIZE_FOR_ELEMENT, ELEMENT_MAX_SIZE);
	}

	/* UI CONTROLS */
	bool folderNode(const char* label, ImTextureID icon, bool& clicked)
	{
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;

		clicked = false;

		ImU32 id = window->GetID(label);
		int opened = window->StateStorage.GetInt(id, 0);
		ImVec2 pos = window->DC.CursorPos;
		const bool is_mouse_x_over_arrow = (g.IO.MousePos.x >= pos.x && g.IO.MousePos.x < pos.x + g.FontSize);

		if (ImGui::InvisibleButton(label, ImVec2(-FLT_MIN, g.FontSize + g.Style.FramePadding.y * 2)))
		{
			if (is_mouse_x_over_arrow) {
				int* p_opened = window->StateStorage.GetIntRef(id, 0);
				opened = *p_opened = !*p_opened;
			} else {
				clicked = true;
			}
		}

		bool hovered = ImGui::IsItemHovered();
		bool active = ImGui::IsItemActive();
		bool doubleClick = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);

		if (doubleClick && hovered) {
			int* p_opened = window->StateStorage.GetIntRef(id, 0);
			opened = *p_opened = !*p_opened;
			clicked = false;
		}

		if (hovered || active) {
			window->DrawList->AddRectFilled(g.LastItemData.Rect.Min, 
											g.LastItemData.Rect.Max, 
											ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[active ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered]));
		}
					
		// Icon, text
		float icon_posX = pos.x + g.FontSize + g.Style.FramePadding.y;
		float text_posX = icon_posX + g.Style.FramePadding.y + computeIconSize(ImGui::GetFont()->FontSize);
		ImGui::RenderArrow(window->DrawList, ImVec2(pos.x, pos.y+g.Style.FramePadding.y), ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[((hovered && is_mouse_x_over_arrow) || opened) ? ImGuiCol_Text : ImGuiCol_TextDisabled]), opened ? ImGuiDir_Down : ImGuiDir_Right);
		window->DrawList->AddImage(icon, ImVec2(icon_posX, pos.y), ImVec2(icon_posX + computeIconSize(ImGui::GetFont()->FontSize), pos.y + computeIconSize(ImGui::GetFont()->FontSize)));
		ImGui::RenderText(ImVec2(text_posX, pos.y + g.Style.FramePadding.y), label);
		
		if (opened) {
			ImGui::TreePush(label);
		}
			
		return opened != 0;
	}

	bool fileNode(const char* label, ImTextureID icon) {
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;

		//ImU32 id = window->GetID(label);
		ImVec2 pos = window->DC.CursorPos;
		bool ret = ImGui::InvisibleButton(label, ImVec2(-FLT_MIN, g.FontSize + g.Style.FramePadding.y * 2));

		bool hovered = ImGui::IsItemHovered();
		bool active = ImGui::IsItemActive();

		if (hovered || active) {
			window->DrawList->AddRectFilled(g.LastItemData.Rect.Min, 
											g.LastItemData.Rect.Max, 
											ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[active ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered]));
		}
			
		// Icon, text
		window->DrawList->AddImage(icon, ImVec2(pos.x, pos.y), ImVec2(pos.x + computeIconSize(ImGui::GetFont()->FontSize), pos.y + computeIconSize(ImGui::GetFont()->FontSize)));
		ImGui::RenderText(ImVec2(pos.x + g.Style.FramePadding.y + computeIconSize(ImGui::GetFont()->FontSize), pos.y + g.Style.FramePadding.y), label);
		
		return ret;
	}

	bool pathBox(const char* label, std::filesystem::path& path, std::string& pathBuffer, ImVec2 size_arg) {
		ImGuiWindow* window = ImGui::GetCurrentWindow();

		if (window->SkipItems) {
			return false;
		}

		bool ret = false;
		const ImGuiID id = window->GetID(label);
		int* state = window->StateStorage.GetIntRef(id, 0);
		
		ImGui::SameLine();

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		ImVec2 pos = window->DC.CursorPos;
		ImVec2 uiPos = ImGui::GetCursorPos();
		ImVec2 size = ImGui::CalcItemSize(size_arg, 200, computeGuiElementSize(GImGui->FontSize));
		const ImRect bb(pos, pos + size);
		
		// buttons
		if (!(*state & 0b001)) {
			ImGui::PushClipRect(bb.Min, bb.Max, false);

			// background
			bool hovered = g.IO.MousePos.x >= bb.Min.x && g.IO.MousePos.x <= bb.Max.x &&
				g.IO.MousePos.y >= bb.Min.y && g.IO.MousePos.y <= bb.Max.y;
			bool clicked = hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left);
			bool anyOtherHC = false; // are any other items hovered or clicked?
			window->DrawList->AddRectFilled(pos, pos + size, ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[(*state & 0b10) ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg]));

			// fetch the buttons (so that we can throw some away if needed)
			std::vector<std::string> btnList;
			float totalWidth = 0.0f;
			for (auto comp : path) {
				std::string section = comp.u8string();

				if (section.size() == 1 && (section[0] == '\\' || section[0] == '/')) {
					continue;
				}
					
				totalWidth += ImGui::CalcTextSize(section.c_str()).x + style.FramePadding.x * 2.0f + computeGuiElementSize(GImGui->FontSize);
				btnList.push_back(section);
			}
			totalWidth -= computeGuiElementSize(GImGui->FontSize);

			// UI buttons
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
			bool isFirstElement = true;
			for (size_t i = 0; i < btnList.size(); i++) {
				if (totalWidth > size.x - 30 && i != btnList.size() - 1) { // trim some buttons if there's not enough space
					float elSize = ImGui::CalcTextSize(btnList[i].c_str()).x + style.FramePadding.x * 2.0f + computeGuiElementSize(GImGui->FontSize);
					totalWidth -= elSize;
					continue;
				}

				ImGui::PushID(static_cast<int>(i));

				if (!isFirstElement) {
					ImGui::ArrowButtonEx("##dir_dropdown", ImGuiDir_Right, ImVec2(computeGuiElementSize(GImGui->FontSize), computeGuiElementSize(GImGui->FontSize)));
					anyOtherHC |= ImGui::IsItemHovered() | ImGui::IsItemClicked();
					ImGui::SameLine();
				}

				if (ImGui::Button(btnList[i].c_str(), ImVec2(0, computeGuiElementSize(GImGui->FontSize)))) {
#ifdef _WIN32
					std::string newPath = "";
#else
					std::string newPath = "/";
#endif
					for (size_t j = 0; j <= i; j++) {
						newPath += btnList[j];
#ifdef _WIN32
						if (j != i) {
							newPath += "\\";
						}
#else
						if (j != i) {
							newPath += "/";
						}
#endif
					}
					path = std::filesystem::u8path(newPath);
					ret = true;
				}

				anyOtherHC |= ImGui::IsItemHovered() | ImGui::IsItemClicked();
				ImGui::SameLine();
				ImGui::PopID();

				isFirstElement = false;
			}
			ImGui::PopStyleVar(2);


			// click state
			if (!anyOtherHC && clicked) {
				pathBuffer = path.u8string();
				*state |= 0b001;
				*state &= 0b011; // remove SetKeyboardFocus flag
			} else {
				*state &= 0b110;
			}

			// hover state
			if (!anyOtherHC && hovered && !clicked) {
				*state |= 0b010;
			} else {
				*state &= 0b101;
			}

			ImGui::PopClipRect();

			// allocate space
			ImGui::SetCursorPos(uiPos);
			ImGui::ItemSize(size);
		}
		// input box
		else {
			bool skipActiveCheck = false;

			if (!(*state & 0b100)) {
				skipActiveCheck = true;
				ImGui::SetKeyboardFocusHere();

				if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
					*state |= 0b100;
				}
			}

			if (ImGui::InputTextWithHint("##pathbox_input", "", &pathBuffer, ImGuiInputTextFlags_EnterReturnsTrue)) {
				if (std::filesystem::exists(pathBuffer)) {
					path = std::filesystem::u8path(pathBuffer);
				}

				ret = true;
			}

			if (!skipActiveCheck && !ImGui::IsItemActive()) {
				*state &= 0b010;
			}
		}

		return ret;
	}

	bool favoriteButton(const char* label, bool isFavorite)
	{
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;

		ImVec2 pos = window->DC.CursorPos;
		bool ret = ImGui::InvisibleButton(label, ImVec2(computeGuiElementSize(GImGui->FontSize), computeGuiElementSize(GImGui->FontSize)));
		
		bool hovered = ImGui::IsItemHovered();
		bool active = ImGui::IsItemActive();

		float size = g.LastItemData.Rect.Max.x - g.LastItemData.Rect.Min.x;

		int numPoints = 5;
		float innerRadius = size / 4;
		float outerRadius = size / 2;
		float angle = PI / numPoints;
		ImVec2 center = ImVec2(pos.x + size / 2, pos.y + size / 2);

		// fill
		if (isFavorite || hovered || active) {
			ImU32 fillColor = 0xff00ffff;// ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]);
			
			if (hovered || active) {
				fillColor = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[active ? ImGuiCol_HeaderActive : ImGuiCol_HeaderHovered]);
			}
				
			// since there is no PathFillConcave, fill first the inner part, then the triangles
			// inner
			window->DrawList->PathClear();
			for (int i = 1; i < numPoints * 2; i += 2) {
				window->DrawList->PathLineTo(ImVec2(center.x + innerRadius * sin(i * angle), center.y - innerRadius * cos(i * angle)));
			}
			window->DrawList->PathFillConvex(fillColor);

			// triangles
			for (int i = 0; i < numPoints; i++) {
				window->DrawList->PathClear();

				int pIndex = i * 2;
				window->DrawList->PathLineTo(ImVec2(center.x + outerRadius * sin(pIndex * angle), center.y - outerRadius * cos(pIndex * angle)));
				window->DrawList->PathLineTo(ImVec2(center.x + innerRadius * sin((pIndex + 1) * angle), center.y - innerRadius * cos((pIndex + 1) * angle)));
				window->DrawList->PathLineTo(ImVec2(center.x + innerRadius * sin((pIndex - 1) * angle), center.y - innerRadius * cos((pIndex - 1) * angle)));

				window->DrawList->PathFillConvex(fillColor);
			}
		}

		// outline
		window->DrawList->PathClear();
		for (int i = 0; i < numPoints * 2; i++) {
			float radius = i & 1 ? innerRadius : outerRadius;
			window->DrawList->PathLineTo(ImVec2(center.x + radius * sin(i * angle), center.y - radius * cos(i * angle)));
		}
		window->DrawList->PathStroke(ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]), true, 2.0f);

		return ret;
	}

	bool fileIcon(const char* label, bool isSelected, ImTextureID icon, ImVec2 size, bool hasPreview, int previewWidth, int previewHeight)
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImGuiContext& g = *GImGui;
		ImGuiWindow* window = g.CurrentWindow;

		float windowSpace = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
		ImVec2 pos = window->DC.CursorPos;
		bool ret = false;

		if (ImGui::InvisibleButton(label, size)) {
			ret = true;
		}

		bool hovered = ImGui::IsItemHovered();
		bool active = ImGui::IsItemActive();
		bool doubleClick = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
		if (doubleClick && hovered) {
			ret = true;
		}

		float iconSize = size.y - g.FontSize * 2;
		float iconPosX = pos.x + (size.x - iconSize) / 2.0f;
		ImVec2 textSize = ImGui::CalcTextSize(label, 0, true, size.x);

		
		if (hovered || active || isSelected) {
			window->DrawList->AddRectFilled(g.LastItemData.Rect.Min, 
											g.LastItemData.Rect.Max, 
											ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[active ? ImGuiCol_HeaderActive : (isSelected ? ImGuiCol_Header : ImGuiCol_HeaderHovered)]));
		}

		if (hasPreview) {
			ImVec2 availSize = ImVec2(size.x, iconSize);

			float scale = std::min<float>(availSize.x / previewWidth, availSize.y / previewHeight);
			availSize.x = previewWidth * scale;
			availSize.y = previewHeight * scale;

			float previewPosX = pos.x + (size.x - availSize.x) / 2.0f;
			float previewPosY = pos.y + (iconSize - availSize.y) / 2.0f;

			window->DrawList->AddImage(icon, ImVec2(previewPosX, previewPosY), ImVec2(previewPosX + availSize.x, previewPosY + availSize.y));
		} else {
			window->DrawList->AddImage(icon, ImVec2(iconPosX, pos.y), ImVec2(iconPosX + iconSize, pos.y + iconSize));
		}

		window->DrawList->AddText(g.Font, 
								  g.FontSize, 
								  ImVec2(pos.x + (size.x-textSize.x) / 2.0f, pos.y + iconSize), 
								  ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]), 
								  label, 
								  0, 
								  size.x);

		float lastButtomPos = ImGui::GetItemRectMax().x;
		float thisButtonPos = lastButtomPos + style.ItemSpacing.x + size.x; // Expected position if next button was on same line
		if (thisButtonPos < windowSpace) {
			ImGui::SameLine();
		}

		return ret;
	}

	FileDialog::SmartSize::SmartSize(size_t s):
		sizeInByte{s},
		size{static_cast<float>(sizeInByte)},
		unit{magic_enum::enum_name(SizeUnit::B)} 
	{
		if (size >= 1024) {
			if (auto u = magic_enum::enum_cast<SizeUnit>(static_cast<uint8_t>(std::log(size) / std::log(1024.0f))); u) {
				size = sizeInByte / std::pow(1024.0f, static_cast<float>(magic_enum::enum_integer(*u)));  // seems MSVC can't deduce std::pow to return float automatically
				unit = magic_enum::enum_name(*u);
			}
		}
	}

	FileDialog::FileData::FileData(const std::filesystem::path& p) 
	{
		std::error_code ec;
		path = p;
		isDirectory = std::filesystem::is_directory(path, ec);
		size = SmartSize{std::filesystem::file_size(path, ec)};

		struct stat attr;
		stat(path.u8string().c_str(), &attr);
		dateModified = attr.st_ctime;

		hasIconPreview = false;
		iconPreview = nullptr;
		iconPreviewData = nullptr;
		iconPreviewHeight = 0;
		iconPreviewWidth = 0;
	}

	FileDialog::FileDialog():
		m_isMultiselect{false},
		m_isOpen{false},
		m_type{DialogType::openFile},
		m_calledOpenPopup{false},
		m_zoom{MIN_ZOOM_LEVEL},
		m_selectedFileItem{-1},
		m_filterSelection{0},
		m_previewLoaderRunning{false},
		m_sortColumn{0},
		m_sortDirection{ImGuiSortDirection_Ascending}
	{
		m_setDirectory(std::filesystem::current_path(), false);

		// favorites are available on every OS
		auto quickAccess = std::make_unique<FileTreeNode>("Quick Access");
		quickAccess->read = true;

#ifdef _WIN32
		wchar_t username[UNLEN + 1] = { 0 };
		DWORD username_len = UNLEN + 1;
		GetUserNameW(username, &username_len);

		std::wstring userPath = L"C:\\Users\\" + std::wstring(username) + L"\\";

		// Quick Access / Bookmarks
		quickAccess->children.emplace_back(std::make_unique<FileTreeNode>(userPath + L"Desktop"));
		quickAccess->children.emplace_back(std::make_unique<FileTreeNode>(userPath + L"Documents"));
		quickAccess->children.emplace_back(std::make_unique<FileTreeNode>(userPath + L"Downloads"));
		quickAccess->children.emplace_back(std::make_unique<FileTreeNode>(userPath + L"Pictures"));
		m_treeCache.emplace_back(std::move(quickAccess));

		// OneDrive
		auto oneDrive = std::make_unique<FileTreeNode>(_wgetenv(L"OneDriveConsumer"));
		m_treeCache.emplace_back(std::move(oneDrive));

		// This PC
		auto thisPC = std::make_unique<FileTreeNode>("This PC");
		thisPC->read = true;

		if (std::filesystem::exists(userPath + L"3D Objects")) {
			thisPC->children.emplace_back(std::make_unique<FileTreeNode>(userPath + L"3D Objects"));
		}

		thisPC->children.emplace_back(std::make_unique<FileTreeNode>(userPath + L"Desktop"));
		thisPC->children.emplace_back(std::make_unique<FileTreeNode>(userPath + L"Documents"));
		thisPC->children.emplace_back(std::make_unique<FileTreeNode>(userPath + L"Downloads"));
		thisPC->children.emplace_back(std::make_unique<FileTreeNode>(userPath + L"Music"));
		thisPC->children.emplace_back(std::make_unique<FileTreeNode>(userPath + L"Pictures"));
		thisPC->children.emplace_back(std::make_unique<FileTreeNode>(userPath + L"Videos"));
		DWORD d = GetLogicalDrives();

		for (int i = 0; i < 26; i++) {
			if (d & (1 << i)) {
				thisPC->children.emplace_back(std::make_unique<FileTreeNode>(std::string(1, 'A' + i) + ":"));
			}
		}
		
		m_treeCache.emplace_back(std::move(thisPC));
#else
		// Quick Access
		struct passwd *pw;
		uid_t uid;
		uid = geteuid();
		pw = getpwuid(uid);
		if (pw) {

#ifdef __APPLE__
			std::string homePath = "/Users/" + std::string(pw->pw_name);
#else
			std::string homePath = "/home/" + std::string(pw->pw_name);
#endif
			
			if (std::filesystem::exists(homePath)) {
				quickAccess->children.emplace_back(std::make_unique<FileTreeNode>(homePath));
			}
				
			if (std::filesystem::exists(homePath + "/Desktop")) {
				quickAccess->children.emplace_back(std::make_unique<FileTreeNode>(homePath + "/Desktop"));
			}
				
			if (std::filesystem::exists(homePath + "/Documents")) {
				quickAccess->children.emplace_back(std::make_unique<FileTreeNode>(homePath + "/Documents"));
			}
				
			if (std::filesystem::exists(homePath + "/Downloads")) {
				quickAccess->children.emplace_back(std::make_unique<FileTreeNode>(homePath + "/Downloads"));
			}
				
			if (std::filesystem::exists(homePath + "/Pictures")) {
				quickAccess->children.emplace_back(std::make_unique<FileTreeNode>(homePath + "/Pictures"));
			}
		}

		m_treeCache.emplace_back(std::move(quickAccess));

		// This PC
		auto thisPC = std::make_unique<FileTreeNode>("This PC");
		thisPC->read = true;
		for (const auto& entry : std::filesystem::directory_iterator("/")) {
			if (std::filesystem::is_directory(entry)) {
				thisPC->children.emplace_back(std::make_unique<FileTreeNode>(entry.path().u8string()));
			}
		}
		m_treeCache.emplace_back(std::move(thisPC));
#endif
	}

	FileDialog::~FileDialog() {
		m_clearIconPreview();
		m_clearIcons();
	}

	bool FileDialog::save(const std::string& key, const std::string& title, const std::string& filter, const std::string& startingDir)
	{
		if (!m_currentKey.empty())
			return false;

		m_currentKey = key;
		m_currentTitle = title + "###" + key;
		m_isOpen = true;
		m_calledOpenPopup = false;
		m_result.clear();
		m_inputTextbox = "";
		m_selections.clear();
		m_selectedFileItem = -1;
		m_isMultiselect = false;
		m_type = DialogType::saveFile;

		m_parseFilter(filter);
		if (!startingDir.empty()) {
			m_setDirectory(std::filesystem::u8path(startingDir), false);
		} else {
			m_setDirectory(m_currentDirectory, false); // refresh contents
		}

		return true;
	}

	bool FileDialog::open(const std::string& key, const std::string& title, const std::string& filter, bool isMultiselect, const std::string& startingDir)
	{
		if (!m_currentKey.empty()) {
			return false;
		}

		m_currentKey = key;
		m_currentTitle = title + "###" + key;
		m_isOpen = true;
		m_calledOpenPopup = false;
		m_result.clear();
		m_inputTextbox = "";
		m_selections.clear();
		m_selectedFileItem = -1;
		m_isMultiselect = isMultiselect;
		m_type = filter.empty() ? DialogType::openDirectory : DialogType::openFile;

		m_parseFilter(filter);
		if (!startingDir.empty()) {
			m_setDirectory(std::filesystem::u8path(startingDir), false);
		} else {
			m_setDirectory(m_currentDirectory, false); // refresh contents
		}

		return true;
	}

	bool FileDialog::isDone(const std::string& key)
	{
		bool isMe = m_currentKey == key;

		if (isMe && m_isOpen) {
			if (!m_calledOpenPopup) {
				ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
				ImGui::OpenPopup(m_currentTitle.c_str());
				m_calledOpenPopup = true;
			}

			if (ImGui::BeginPopupModal(m_currentTitle.c_str(), &m_isOpen, ImGuiWindowFlags_NoScrollbar)) {
				m_renderFileDialog();
				ImGui::EndPopup();
			} else {
				m_isOpen = false;
			}
		}

		return isMe && !m_isOpen;
	}
	void FileDialog::close()
	{
		m_currentKey.clear();
		m_backHistory = std::stack<std::filesystem::path>();
		m_forwardHistory = std::stack<std::filesystem::path>();
		confirmationPopup = false;

		for (auto& node : m_treeCache) {
			for (auto& child : node->children) {
				child->children.clear();
				child->read = false;
			}
		}

		// free icon textures
		m_clearIconPreview();
		m_clearIcons();
	}

	void FileDialog::removeFavorite(const std::string& path)
	{
		auto itr = std::find(m_favorites.begin(), m_favorites.end(), m_currentDirectory.u8string());

		if (itr != m_favorites.end())
			m_favorites.erase(itr);

		// remove from sidebar
		for (auto& p : m_treeCache) {
			if (p->path == "Quick Access") {
				for (size_t i = 0; i < p->children.size(); i++)
					if (p->children[i]->path == path) {
						p->children.erase(p->children.begin() + i);
						break;
					}
				break;
			}
		}
	}

	void FileDialog::addFavorite(const std::string& path)
	{
		if (std::count(m_favorites.begin(), m_favorites.end(), path) > 0)
			return;

		if (!std::filesystem::exists(std::filesystem::u8path(path)))
			return;

		m_favorites.push_back(path);
		
		// add to sidebar
		for (auto& p : m_treeCache) {
			if (p->path == "Quick Access") {
				p->children.emplace_back(std::make_unique<FileTreeNode>(path));
				break;
			}
		}
	}
	
	void FileDialog::m_select(const std::filesystem::path& path, bool isCtrlDown)
	{
		bool multiselect = isCtrlDown && m_isMultiselect;

		if (!multiselect) {
			m_selections.clear();
			m_selections.push_back(path);
		} else {
			auto it = std::find(m_selections.begin(), m_selections.end(), path);
			if (it != m_selections.end()) {
				m_selections.erase(it);
			} else {
				m_selections.push_back(path);
			}
		}

		if (m_selections.size() == 1) {
			m_inputTextbox = m_selections[0].filename().u8string();
			if (m_inputTextbox.size() == 0) {
				m_inputTextbox = m_selections[0].u8string(); // drive
			}
		} else {
			std::string textboxVal = "";
			for (const auto& sel : m_selections) {
				std::string filename = sel.filename().u8string();
				if (filename.size() == 0) {
					filename = sel.u8string();
				}

				textboxVal += "\"" + filename + "\", ";
			}

			m_inputTextbox = textboxVal.substr(0, textboxVal.size() - 2).c_str();
		}
	}

	bool FileDialog::m_finalize(const std::string& filename)
	{
		auto path = std::filesystem::u8path(filename);
		bool hasResult = (!filename.empty() && m_type != DialogType::openDirectory) || m_type == DialogType::openDirectory;
		
		if (hasResult) {
			if (m_type == DialogType::saveFile) {
				// add the extension
				if (m_filterSelection < m_filterExtensions.size() && m_filterExtensions[m_filterSelection].size() > 0) {
					if (!path.has_extension()) {
						std::string extAdd = m_filterExtensions[m_filterSelection][0];
						path.replace_extension(extAdd);
						m_inputTextbox = path.u8string();
					}
				}

				if (std::filesystem::exists(m_currentDirectory / path) &&
					!confirmationPopup) {
					// ask to confirm if overwrite 
					// shouldn't call OpenPopup here because m_finalize may be called in a ID stack 
					// level different to where we call BeginPopupModal
					confirmationPopup = true;
					return false;
				}
			}

			if (!m_isMultiselect || m_selections.size() <= 1) {
				if (path.is_absolute()) {
					m_result.push_back(path);
				} else {
					m_result.push_back(m_currentDirectory / path);
				}

				if (m_type == DialogType::openDirectory || m_type == DialogType::openFile) {
					if (!std::filesystem::exists(m_result.back())) {
						m_result.clear();
						return false;
					}
				}
			}
			else {
				for (const auto& sel : m_selections) {
					if (sel.is_absolute()) {
						m_result.push_back(sel);
					} else {
						m_result.push_back(m_currentDirectory / sel);
					}

					if (m_type == DialogType::openDirectory || m_type == DialogType::openFile) {
						if (!std::filesystem::exists(m_result.back())) {
							m_result.clear();
							return false;
						}
					}
				}
			}
		}

		m_isOpen = false;

		return true;
	}
	void FileDialog::m_parseFilter(const std::string& filter)
	{
		m_filter = "";
		m_filterExtensions.clear();
		m_filterSelection = 0;

		if (filter.empty()) {
			return;
		}

		std::vector<std::string> exts;

		size_t lastSplit = 0, lastExt = 0;
		bool inExtList = false;
		for (size_t i = 0; i < filter.size(); i++) {
			if (filter[i] == ',') {
				if (!inExtList) {
					lastSplit = i + 1;
				} else {
					exts.push_back(filter.substr(lastExt, i - lastExt));
					lastExt = i + 1;
				}
			}
			else if (filter[i] == '{') {
				std::string filterName = filter.substr(lastSplit, i - lastSplit);
				if (filterName == ".*") {
					m_filter += std::string(std::string(__("All Files (*.*)\0")).c_str(), 16);
					m_filterExtensions.push_back(std::vector<std::string>());
				} else {
					m_filter += std::string((filterName + "\0").c_str(), filterName.size() + 1);
				}

				inExtList = true;
				lastExt = i + 1;
			} else if (filter[i] == '}') {
				exts.push_back(filter.substr(lastExt, i - lastExt));
				m_filterExtensions.push_back(exts);
				exts.clear();

				inExtList = false;
			}
		}

		if (lastSplit != 0) {
			std::string filterName = filter.substr(lastSplit);
			if (filterName == ".*") {
				m_filter += std::string(std::string(__("All Files (*.*)\0")).c_str(), 16);
				m_filterExtensions.push_back(std::vector<std::string>());
			}
			else {
				m_filter += std::string((filterName + "\0").c_str(), filterName.size() + 1);
			}
		}
	}

#ifdef __linux__
	std::filesystem::path FileDialog::m_locateIcon(const std::string& iconName, int size)
	{
		const auto home = g_get_home_dir();
		const auto theme = getIconTheme();
		if (theme.empty()) {
			fprintf(stderr, "Error getting icon theme\n");
			return {};
		}

		if (!m_iconPathCache.contains(theme)) {
			m_iconPathCache.emplace(std::make_pair(theme, std::unordered_map<std::string, std::filesystem::path>{}));
		} else if (m_iconPathCache[theme].contains(iconName)) {
			return m_iconPathCache[theme][iconName];
		}

		std::string sizeString = std::to_string(size);
		std::filesystem::path dimension = sizeString + "x" + sizeString;
		std::array<std::filesystem::path, NUM_SYSTEM_ICON_PATH> directories{
			std::filesystem::path{USER_ICON_PATH} / theme / dimension,
			std::filesystem::path{GLOBAL_ICON_PATH} / theme / dimension,
		};

		for (const auto dir : directories) {
			std::vector<std::filesystem::path> subdirectories;
			if (std::filesystem::exists(dir)) {
				for (const auto subdir : std::filesystem::directory_iterator(dir)) {
					std::filesystem::path iconPath = subdir / std::filesystem::path{iconName + ".png"};
					if (std::filesystem::exists(iconPath)) {
						m_iconPathCache[theme].emplace(std::make_pair(iconName, iconPath));
						return iconPath;
					}
				}
			}
		}

		return {};
	}
#endif

	void* FileDialog::m_getIcon(const std::filesystem::path& path)
	{
		const std::string pathU8 = path.u8string();

		if (m_icons.contains(pathU8)) {
			return m_icons[pathU8];
		}

#ifdef _WIN32
		DWORD attrs = 0;
		UINT flags = SHGFI_ICON | SHGFI_LARGEICON;
		if (!std::filesystem::exists(path)) {
			flags |= SHGFI_USEFILEATTRIBUTES;
			attrs = FILE_ATTRIBUTE_DIRECTORY;
		}

		SHFILEINFOW fileInfo = { 0 };
		std::wstring pathW = path.wstring();
		for (int i = 0; i < pathW.size(); i++) {
			if (pathW[i] == '/') {
				pathW[i] = '\\';
			}
		}
		SHGetFileInfoW(pathW.c_str(), attrs, &fileInfo, sizeof(SHFILEINFOW), flags);

		if (fileInfo.hIcon == nullptr) {
			return nullptr;
		}

		ICONINFO iconInfo = { 0 };
		GetIconInfo(fileInfo.hIcon, &iconInfo);
		
		if (iconInfo.hbmColor == nullptr) {
			return nullptr;
		}

		DIBSECTION ds;
		GetObject(iconInfo.hbmColor, sizeof(ds), &ds);
		int byteSize = ds.dsBm.bmWidth * ds.dsBm.bmHeight * (ds.dsBm.bmBitsPixel / 8);

		if (byteSize == 0) {
			return nullptr;
		}

		std::vector<uint8_t> bitmap;
		bitmap.reserve(byteSize);
		GetBitmapBits(iconInfo.hbmColor, byteSize, bitmap.data());

		m_icons[pathU8] = this->createTexture(bitmap.data(), ds.dsBm.bmWidth, ds.dsBm.bmHeight, Format::BGRA);

#elif defined(__linux__)
		GFile* gFile;
		if (std::filesystem::exists(path)) {
			gFile = g_file_new_for_path(pathU8.c_str());
		} else {
			// treat non-exists path as a director, such as "Quick access"
			gFile = g_file_new_for_path("/");
		}

		if (!G_IS_OBJECT(gFile)) {
			return nullptr;
		}
		
		const auto gFileInfo = g_file_query_info(gFile, G_FILE_ATTRIBUTE_STANDARD_ICON, G_FILE_QUERY_INFO_NONE, nullptr, nullptr);
		if (!G_IS_OBJECT(gFileInfo)) {
			g_object_unref(gFile);
			return nullptr;
		}

		const auto icon = g_file_info_get_icon(gFileInfo);
		if (!G_IS_OBJECT(icon)) {
			g_object_unref(gFileInfo);
			g_object_unref(gFile);
			return nullptr;
		}

		std::filesystem::path iconPath{};
		if (G_IS_THEMED_ICON(icon)) {
			const auto names = g_themed_icon_get_names(G_THEMED_ICON(icon));
			for (int i = 0; names[i] != NULL; i++) {
				iconPath = m_locateIcon(names[i], DEFAULT_ICON_SIZE);
				if (!iconPath.empty()) {
					break;
				}
			}
		} else if (G_IS_FILE_ICON(icon)) {
			GFile *file_icon = g_file_icon_get_file(G_FILE_ICON(icon));
			iconPath = g_file_get_path(file_icon);
		}

		if (iconPath.empty()) {
			m_loadDefaultIcon(path, pathU8);
		} else {

			int width, height, channel;
			const auto image_data = stbi_load(iconPath.string().c_str(), &width, &height, &channel, 0);
			m_icons[pathU8] = this->createTexture(image_data, width, height, Format::RGBA);
		}

		g_object_unref(gFileInfo);
		g_object_unref(gFile);
#elif defined(__APPLE__)
		NSImage *icon = nullptr;

		if (std::filesystem::exists(path)) {
			icon = [[NSWorkspace sharedWorkspace] iconForFile:[NSString stringWithUTF8String:path.u8string().c_str()]];
		} else {
      		icon = [[NSWorkspace sharedWorkspace] iconForFile:@"/bin"];
		}

		if (icon == nullptr) {
			return nullptr;
		}

		CGImageRef cgImage = [icon CGImageForProposedRect:nullptr context:nullptr hints:nullptr];
		CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
		auto width = CGImageGetWidth(cgImage) * APPLE_ICON_DEFAULT_SCALE_LEVEL;
    	auto height = CGImageGetHeight(cgImage) * APPLE_ICON_DEFAULT_SCALE_LEVEL;
		// use alloc to ensure all initialized to zero
		std::unique_ptr<uint8_t> rawData{reinterpret_cast<uint8_t*>(calloc(width * height * DEFAULT_ICON_CHANNELS, sizeof(uint8_t)))};
		CGContextRef bitmapContext = CGBitmapContextCreate(rawData.get(), 
														   width, 
														   height, 
														   CGImageGetBitsPerComponent(cgImage), 
														   CGImageGetBytesPerRow(cgImage) * APPLE_ICON_DEFAULT_SCALE_LEVEL, 
														   colorSpace, 
														   CGImageGetAlphaInfo(cgImage));
		
		if (bitmapContext == nullptr) {
			CGImageRelease(cgImage);
			CGColorSpaceRelease(colorSpace);
			return nullptr;
		}

		CGContextScaleCTM(bitmapContext, APPLE_ICON_DEFAULT_SCALE_LEVEL, APPLE_ICON_DEFAULT_SCALE_LEVEL);
		CGContextDrawImage(bitmapContext, CGRectMake(0, 0, width / APPLE_ICON_DEFAULT_SCALE_LEVEL, height / APPLE_ICON_DEFAULT_SCALE_LEVEL), cgImage);

		m_icons[pathU8] = this->createTexture(reinterpret_cast<const uint8_t*>(rawData.get()), width, height, Format::RGBA);

		CGImageRelease(cgImage);
		CGColorSpaceRelease(colorSpace);
		CGContextRelease(bitmapContext);
#else
		m_loadDefaultIcon(path, pathU8);
#endif
		return m_icons[pathU8];
	}

	void FileDialog::m_loadDefaultIcon(const std::filesystem::path& path, const std::string& pathU8)
	{
		auto icon = DEFAULT_FILE_ICON;
		if (std::filesystem::is_directory(path) || !std::filesystem::exists(path)) {
			// treat non-exists path as a director, such as "Quick access"
			icon = DEFAULT_FOLDER_ICON;
		}

		ImVec4 wndBg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);

		// light theme - load default icons
		if (ImGui::GetStyleColorVec4(ImGuiCol_WindowBg) == IMGUI_LIGHT_THEME_WINDOW_BG) {
			m_icons[pathU8] = this->createTexture(reinterpret_cast<const uint8_t*>(icon.data()), DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE, Format::BGRA);
		}
		// dark theme - invert the colors
		else {
			std::vector<uint32_t> invertedIcon;
			invertedIcon.reserve(DEFAULT_ICON_SIZE * DEFAULT_ICON_SIZE * DEFAULT_ICON_CHANNELS);
			std::transform(icon.cbegin(), icon.cend(), invertedIcon.begin(), [](auto rgba){
				return (RGB_MASK - (rgba & RGB_MASK)) | (rgba & ALPHA_MASK);
			});

			m_icons[pathU8] = this->createTexture(reinterpret_cast<const uint8_t*>(invertedIcon.data()), DEFAULT_ICON_SIZE, DEFAULT_ICON_SIZE, Format::BGRA);
		}
	}

	void FileDialog::m_clearIcons()
	{
		std::vector<unsigned int> deletedIcons;

		// delete textures
		for (auto& icon : m_icons) {
			unsigned int ptr = (unsigned int)((uintptr_t)icon.second);

			if (std::count(deletedIcons.begin(), deletedIcons.end(), ptr)) { // skip duplicates
				continue;
			}

			deletedIcons.push_back(ptr);
			deleteTexture(icon.second);
		}

		m_icons.clear();
	}

	void FileDialog::m_refreshIconPreview()
	{
		if (m_zoom >= ZOOM_LEVEL_RENDER_PREVIEW) {
			if (!m_previewLoader.joinable()) {
				m_previewLoaderRunning = true;
				m_previewLoader = std::thread(&FileDialog::m_loadPreview, this);
			}
		} else {
			m_clearIconPreview();
		}
	}

	void FileDialog::m_clearIconPreview()
	{
		m_stopPreviewLoader();

		for (auto& data : m_content) {
			if (!data.hasIconPreview) {
				continue;
			}

			data.hasIconPreview = false;
			this->deleteTexture(data.iconPreview);

			if (data.iconPreviewData != nullptr) {
				stbi_image_free(data.iconPreviewData);
				data.iconPreviewData = nullptr;
			}
		}
	}

	void FileDialog::m_stopPreviewLoader()
	{
		m_previewLoaderRunning = false;

		if (m_previewLoader.joinable()) {
			m_previewLoader.join();
		}
	}

	void FileDialog::m_loadPreview()
	{
		for (size_t i = 0; m_previewLoaderRunning && i < m_content.size(); i++) {
			auto& data = m_content[i];

			if (data.hasIconPreview) {
				continue;
			}

			if (data.path.has_extension()) {
				std::string ext = data.path.extension().u8string();
				if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
					int width, height, nrChannels;
					unsigned char* image = stbi_load(data.path.u8string().c_str(), &width, &height, &nrChannels, STBI_rgb_alpha);

					if (image == nullptr || width == 0 || height == 0) {
						continue;
					}

					data.hasIconPreview = true;
					data.iconPreviewData = image;
					data.iconPreviewWidth = width;
					data.iconPreviewHeight = height;
				}
			}
		}

		m_previewLoaderRunning = false;
	}

	void FileDialog::m_setDirectory(const std::filesystem::path& p, bool addHistory)
	{
		bool isSameDir = m_currentDirectory == p;

		if (addHistory && !isSameDir) {
			m_backHistory.push(m_currentDirectory);
		}

		m_currentDirectory = p;
#ifdef _WIN32
		// drives don't work well without the backslash symbol
		if (p.u8string().size() == 2 && p.u8string()[1] == ':') {
			m_currentDirectory = std::filesystem::u8path(p.u8string() + "\\");
		}
#endif

		m_clearIconPreview();
		m_content.clear(); // p == "" after this line, due to reference
		m_selectedFileItem = -1;
		
		if (m_type == DialogType::openDirectory || m_type == DialogType::openFile) {
			m_inputTextbox = "";
		}

		m_selections.clear();

		if (!isSameDir) {
			m_searchBuffer.clear();
			m_clearIcons();
		}

		if (p.u8string() == "Quick Access") {
			for (auto& node : m_treeCache) {
				if (node->path == p) {
					for (auto& c : node->children) {
						m_content.push_back(FileData(c->path));
					}
				}
			}
		} else if (p.u8string() == "This PC") {
			for (auto& node : m_treeCache) {
				if (node->path == p) {
					for (auto& c : node->children) {
						m_content.push_back(FileData(c->path));
					}
				}
			}
		} else {
			std::error_code ec;
			if (std::filesystem::exists(m_currentDirectory, ec)) {
				for (const auto& entry : std::filesystem::directory_iterator(m_currentDirectory, ec)) {
					FileData info(entry.path());

					// skip files when IFD_DIALOG_DIRECTORY
					if (!info.isDirectory && m_type == DialogType::openDirectory) {
						continue;
					}

					// check if filename matches search query
					if (!m_searchBuffer.empty()) {
						std::string filename = info.path.u8string();

						std::string filenameSearch = filename;
						std::string query = m_searchBuffer;
						std::transform(filenameSearch.begin(), filenameSearch.end(), filenameSearch.begin(), ::tolower);
						std::transform(query.begin(), query.end(), query.begin(), ::tolower);

						if (filenameSearch.find(query, 0) == std::string::npos) {
							continue;
						}
					}

					// check if extension matches
					if (!info.isDirectory && m_type != DialogType::openDirectory) {
						if (m_filterSelection < m_filterExtensions.size()) {
							const auto& exts = m_filterExtensions[m_filterSelection];

							if (exts.size() > 0) {
								std::string extension = info.path.extension().u8string();

								// extension not found? skip
								if (std::count(exts.begin(), exts.end(), extension) == 0) {
									continue;
								}
							}
						}
					}

					m_content.push_back(info);
				}
			}
		}

		m_sortContent(m_sortColumn, m_sortDirection);
		m_refreshIconPreview();
	}

	void FileDialog::m_sortContent(unsigned int column, unsigned int sortDirection)
	{
		// 0 -> name, 1 -> date, 2 -> size
		m_sortColumn = column;
		m_sortDirection = sortDirection;

		// split into directories and files
		std::partition(m_content.begin(), m_content.end(), [](const FileData& data) {
			return data.isDirectory;
		});

		if (m_content.size() > 0) {
			// find where the file list starts
			size_t fileIndex = 0;
			for (; fileIndex < m_content.size(); fileIndex++) {
				if (!m_content[fileIndex].isDirectory) {
					break;
				}
			}

			// compare function
			auto compareFn = [column, sortDirection](const FileData& left, const FileData& right) -> bool {
				// name
				if (column == 0) {
					std::string lName = left.path.u8string();
					std::string rName = right.path.u8string();

					std::transform(lName.begin(), lName.end(), lName.begin(), ::tolower);
					std::transform(rName.begin(), rName.end(), rName.begin(), ::tolower);

					int comp = lName.compare(rName);

					if (sortDirection == ImGuiSortDirection_Ascending)
						return comp < 0;
					return comp > 0;
				}  else if (column == 1) { // date
					if (sortDirection == ImGuiSortDirection_Ascending) {
						return left.dateModified < right.dateModified;
					} else {
						return left.dateModified > right.dateModified;
					}
				} else if (column == 2) { // size
					if (sortDirection == ImGuiSortDirection_Ascending) {
						return left.size < right.size;
					} else {
						return left.size > right.size;
					}
				}

				return false;
			};

			// sort the directories
			std::sort(m_content.begin(), m_content.begin() + fileIndex, compareFn);

			// sort the files
			std::sort(m_content.begin() + fileIndex, m_content.end(), compareFn);
		}
	}

	void FileDialog::m_renderTree(FileTreeNode& node)
	{
		// directory
		std::error_code ec;
		ImGui::PushID(node.path.u8string().c_str());
		bool isClicked = false;
		std::string displayName = node.path.stem().u8string();

		if (displayName.size() == 0) {
			displayName = node.path.u8string();
		}

		if (folderNode(displayName.c_str(), (ImTextureID)m_getIcon(node.path), isClicked)) {
			if (!node.read) {
				// cache children if it's not already cached
				if (std::filesystem::exists(node.path, ec)) {
					for (const auto& entry : std::filesystem::directory_iterator(node.path, ec)) {

						if (std::filesystem::is_directory(entry, ec)) {
							node.children.emplace_back(std::make_unique<FileTreeNode>(entry.path().u8string()));
						}
					}
				}

				node.read = true;
			}

			// display children
			for (auto& c : node.children) {
				m_renderTree(*c);
			}

			ImGui::TreePop();
		}

		if (isClicked) {
			m_setDirectory(node.path);
		}

		ImGui::PopID();
	}

	void FileDialog::m_renderContent()
	{
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			m_selectedFileItem = -1;
		}

		// table view
		if (m_zoom == ZOOM_LEVEL_LIST_VIEW) {
			if (ImGui::BeginTable("##contentTable", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable | ImGuiTableFlags_NoBordersInBody, ImVec2(0, -FLT_MIN))) {
				// header
				ImGui::TableSetupColumn(__("Name##filename"), ImGuiTableColumnFlags_WidthStretch, 0.0f -1.0f, 0);
				ImGui::TableSetupColumn(__("Date modified##filedate"), ImGuiTableColumnFlags_WidthStretch, 0.0f, 1);
				ImGui::TableSetupColumn(__("Size##filesize"), ImGuiTableColumnFlags_WidthStretch, 0.0f, 2);
                ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableHeadersRow();

				// sort
				if (ImGuiTableSortSpecs* sortSpecs = ImGui::TableGetSortSpecs()) {
                    if (sortSpecs->SpecsDirty) {
						sortSpecs->SpecsDirty = false;
						m_sortContent(sortSpecs->Specs->ColumnUserID, sortSpecs->Specs->SortDirection);
                    }
				}

				// content
				int fileId = 0;
				for (auto& entry : m_content) {
					std::string filename = entry.path.filename().u8string();

					if (filename.size() == 0) {
						filename = entry.path.u8string(); // drive
					}
					
					bool isSelected = std::count(m_selections.begin(), m_selections.end(), entry.path);

					ImGui::TableNextRow();

					// file name
					ImGui::TableSetColumnIndex(0);
					ImGui::Image((ImTextureID)m_getIcon(entry.path), ImVec2(computeIconSize(ImGui::GetFont()->FontSize), computeIconSize(ImGui::GetFont()->FontSize)));
					ImGui::SameLine();

					if (ImGui::Selectable(filename.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
						std::error_code ec;
						bool isDir = std::filesystem::is_directory(entry.path, ec);

						if (ImGui::IsMouseDoubleClicked(0)) {
							if (isDir) {
								m_setDirectory(entry.path);
								break;
							} else {
								m_finalize(filename);
							}
						} else {
							if ((isDir && m_type == DialogType::openDirectory) || !isDir) {
								m_select(entry.path, ImGui::GetIO().KeyCtrl);
							}
						}
					}

					if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
						m_selectedFileItem = fileId;
					}

					fileId++;

					// date
					ImGui::TableSetColumnIndex(1);
					auto tm = std::localtime(&entry.dateModified);
					if (tm != nullptr) {
						ImGui::Text("%d/%d/%d %02d:%02d", tm->tm_mon + 1, tm->tm_mday, 1900 + tm->tm_year, tm->tm_hour, tm->tm_min);
					} else {
						ImGui::Text("---");
					}

					// size
					ImGui::TableSetColumnIndex(2);
					if (!entry.isDirectory) {
						ImGui::Text("%.3f %s", entry.size.size, entry.size.unit.c_str());
					}
				}

				ImGui::EndTable();
			}
		} else { // "icon" view
			// content
			int fileId = 0;
			for (auto& entry : m_content) {
				if (entry.hasIconPreview && entry.iconPreviewData != nullptr) {
					entry.iconPreview = this->createTexture(entry.iconPreviewData, entry.iconPreviewWidth, entry.iconPreviewHeight, Format::RGBA);
					stbi_image_free(entry.iconPreviewData);
					entry.iconPreviewData = nullptr;
				}

				std::string filename = entry.path.filename().u8string();
				if (filename.size() == 0) {
					filename = entry.path.u8string(); // drive
				}

				bool isSelected = std::count(m_selections.begin(), m_selections.end(), entry.path);

				if (fileIcon(filename.c_str(), isSelected, entry.hasIconPreview ? entry.iconPreview : (ImTextureID)m_getIcon(entry.path), ImVec2(32 + 16 * m_zoom, 32 + 16 * m_zoom), entry.hasIconPreview, entry.iconPreviewWidth, entry.iconPreviewHeight)) {
					std::error_code ec;
					bool isDir = std::filesystem::is_directory(entry.path, ec);

					if (ImGui::IsMouseDoubleClicked(0)) {
						if (isDir) {
							m_setDirectory(entry.path);
							break;
						} else {
							m_finalize(filename);
						}
					} else {
						if ((isDir && m_type == DialogType::openDirectory) || !isDir) {
							m_select(entry.path, ImGui::GetIO().KeyCtrl);
						}
					}
				}

				if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
					m_selectedFileItem = fileId;
				}

				fileId++;
			}
		}
	}

	void FileDialog::m_renderPopups()
	{
		bool openAreYouSureDlg = false, openNewFileDlg = false, openNewDirectoryDlg = false;
		if (ImGui::BeginPopupContextItem("##dir_context")) {
			if (ImGui::Selectable(__("New file"))) {
				openNewFileDlg = true;
			}

			if (ImGui::Selectable(__("New directory"))) {
				openNewDirectoryDlg = true;
			}

			if (m_selectedFileItem != -1 && ImGui::Selectable(__("Delete"))) {
				openAreYouSureDlg = true;
			}

			ImGui::EndPopup();
		}

		if (openAreYouSureDlg) {
			ImGui::OpenPopup(__("Are you sure?##delete"));
		}

		if (openNewFileDlg) {
			ImGui::OpenPopup(__("Enter file name##newfile"));
		}

		if (openNewDirectoryDlg) {
			ImGui::OpenPopup(__("Enter directory name##newdir"));
		}

		if (ImGui::BeginPopupModal(__("Are you sure?##delete"))) {
			if (m_selectedFileItem >= static_cast<int>(m_content.size()) || m_content.size() == 0) {
				ImGui::CloseCurrentPopup();
			} else {
				const FileData& data = m_content[m_selectedFileItem];
				ImGui::TextWrapped(__("Are you sure you want to delete %s?"), data.path.filename().u8string().c_str());
				if (ImGui::Button(__("Yes"))) {
					std::error_code ec;
					std::filesystem::remove_all(data.path, ec);
					m_setDirectory(m_currentDirectory, false); // refresh
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button(__("No"))) {
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopupModal(__("Enter file name##newfile"))) {
			ImGui::PushItemWidth(250.0f);
			ImGui::InputText("##newfilename", &m_newEntryBuffer);
			ImGui::PopItemWidth();

			if (ImGui::Button(__("OK"))) {
				std::ofstream out((m_currentDirectory / m_newEntryBuffer).string());
				out << "";
				out.close();

				m_setDirectory(m_currentDirectory, false); // refresh
				m_newEntryBuffer.clear();

				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button(__("Cancel"))) {
				m_newEntryBuffer.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopupModal(__("Enter directory name##newdir"))) {
			ImGui::PushItemWidth(250.0f);
			ImGui::InputText("##newfilename", &m_newEntryBuffer); // TODO: remove hardcoded literals
			ImGui::PopItemWidth();

			if (ImGui::Button(__("OK"))) {
				std::error_code ec;
				std::filesystem::create_directory(m_currentDirectory / m_newEntryBuffer, ec);
				m_setDirectory(m_currentDirectory, false); // refresh
				m_newEntryBuffer.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button(__("Cancel"))) {
				ImGui::CloseCurrentPopup();
				m_newEntryBuffer.clear();
			}
			ImGui::EndPopup();
		}
	}

	void FileDialog::m_renderFileDialog()
	{
		/***** TOP BAR *****/
		bool noBackHistory = m_backHistory.empty(), noForwardHistory = m_forwardHistory.empty();
		
		ImGui::PushStyleColor(ImGuiCol_Button, 0);
		if (noBackHistory) {
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		if (ImGui::ArrowButtonEx("##back", ImGuiDir_Left, ImVec2(computeGuiElementSize(GImGui->FontSize), computeGuiElementSize(GImGui->FontSize)), m_backHistory.empty() * ImGuiItemFlags_Disabled)) {
			std::filesystem::path newPath = m_backHistory.top();
			m_backHistory.pop();
			m_forwardHistory.push(m_currentDirectory);

			m_setDirectory(newPath, false);
		}
	
		if (noBackHistory) {
			ImGui::PopStyleVar();
		}

		ImGui::SameLine();
		
		if (noForwardHistory) {
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}

		if (ImGui::ArrowButtonEx("##forward", ImGuiDir_Right, ImVec2(computeGuiElementSize(GImGui->FontSize), computeGuiElementSize(GImGui->FontSize)), m_forwardHistory.empty() * ImGuiItemFlags_Disabled)) {
			std::filesystem::path newPath = m_forwardHistory.top();
			m_forwardHistory.pop();
			m_backHistory.push(m_currentDirectory);

			m_setDirectory(newPath, false);
		}

		if (noForwardHistory) {
			ImGui::PopStyleVar();
		}

		ImGui::SameLine();
		
		if (ImGui::ArrowButtonEx("##up", ImGuiDir_Up, ImVec2(computeGuiElementSize(GImGui->FontSize), computeGuiElementSize(GImGui->FontSize)))) {
			if (m_currentDirectory.has_parent_path()) {
				m_setDirectory(m_currentDirectory.parent_path());
			}
		}
		
		std::filesystem::path curDirCopy = m_currentDirectory;
		if (pathBox("##pathbox", curDirCopy, m_pathBuffer, ImVec2(-250, computeGuiElementSize(GImGui->FontSize)))) {
			m_setDirectory(curDirCopy);
		}
		ImGui::SameLine();
		
		if (favoriteButton("##dirfav", std::count(m_favorites.begin(), m_favorites.end(), m_currentDirectory.u8string()))) {
			if (std::count(m_favorites.begin(), m_favorites.end(), m_currentDirectory.u8string())) {
				removeFavorite(m_currentDirectory.u8string());
			} else { 
				addFavorite(m_currentDirectory.u8string());
			}
		}
		ImGui::SameLine();
		ImGui::PopStyleColor();

		if (ImGui::InputTextWithHint("##searchTB", __("Search"), &m_searchBuffer)) {
			m_setDirectory(m_currentDirectory, false); // refresh
		}

		/***** CONTENT *****/
		float bottomBarHeight = (GImGui->FontSize + ImGui::GetStyle().FramePadding.y + ImGui::GetStyle().ItemSpacing.y * 2.0f) * 2;
		if (ImGui::BeginTable("##table", 2, ImGuiTableFlags_Resizable, ImVec2(0, -bottomBarHeight))) {
			ImGui::TableSetupColumn("##tree", ImGuiTableColumnFlags_WidthFixed, 125.0f);
			ImGui::TableSetupColumn("##content", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableNextRow();

			// the tree on the left side
			ImGui::TableSetColumnIndex(0);
			ImGui::BeginChild("##treeContainer", ImVec2(0, -bottomBarHeight));
			for (auto& node : m_treeCache) {
				m_renderTree(*node);
			}
			ImGui::EndChild();
			
			// content on the right side
			ImGui::TableSetColumnIndex(1);
			ImGui::BeginChild("##contentContainer", ImVec2(0, -bottomBarHeight));
			m_renderContent();
			ImGui::EndChild();
			if (ImGui::IsItemHovered()) {
#ifdef __APPLE__
				if (ImGui::GetIO().KeySuper) {
#else
				if (ImGui::GetIO().KeyCtrl) {
#endif
					if (ImGui::GetIO().MouseWheel != MOUSE_WHEEL_NOT_SCROLLING) {
						m_zoom = std::min<float>(MAX_ZOOM_LEVEL, std::max<float>(MIN_ZOOM_LEVEL, m_zoom + ImGui::GetIO().MouseWheel));
					}

					if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Equal))) {
						m_zoom = std::clamp(m_zoom + 1, MIN_ZOOM_LEVEL, MAX_ZOOM_LEVEL);
					} else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Minus))) {
						m_zoom = std::clamp(m_zoom - 1, MIN_ZOOM_LEVEL, MAX_ZOOM_LEVEL);
					}
					
					m_refreshIconPreview();
				}
			}

			// New file, New directory and Delete popups
			m_renderPopups();

			ImGui::EndTable();
		}
		
		/***** BOTTOM BAR *****/
		ImGui::Text(__("File name:"));
		ImGui::SameLine();
		if (ImGui::InputTextWithHint("##file_input", __("Filename"), &m_inputTextbox, ImGuiInputTextFlags_EnterReturnsTrue)) {
			bool success = m_finalize(m_inputTextbox);
#ifdef _WIN32
			if (!success)
				MessageBeep(MB_ICONERROR);
#else
			(void)success;
#endif
		}
	
		if (m_type != DialogType::openDirectory) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(-FLT_MIN);
			int sel = static_cast<int>(m_filterSelection);
			if (ImGui::Combo("##ext_combo", &sel, m_filter.c_str())) {
				m_filterSelection = static_cast<size_t>(sel);
				m_setDirectory(m_currentDirectory, false); // refresh
			}
		}

		// buttons
		float ok_cancel_width = computeGuiElementSize(GImGui->FontSize) * 7;
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - ok_cancel_width);
		if (ImGui::Button(m_type == DialogType::saveFile ? __("Save") : __("Open"), ImVec2(ok_cancel_width / 2 - ImGui::GetStyle().ItemSpacing.x, 0.0f))) {
			bool success = false;
			if (!m_inputTextbox.empty() || m_type == DialogType::openDirectory) {
				success = m_finalize(m_inputTextbox);
			}
#ifdef _WIN32
			if (!success) {
				MessageBeep(MB_ICONERROR);
			}
#else
			(void)success;
#endif
		}

		ImGui::SameLine();
		if (ImGui::Button(__("Cancel"), ImVec2(-FLT_MIN, 0.0f))) {
			if (m_type == DialogType::openDirectory) {
				m_isOpen = false;
			} else {
				m_finalize();
			}
		}

		int escapeKey = ImGui::GetIO().KeyMap[ImGuiKey_Escape];
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
			 escapeKey >= 0 && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
			m_isOpen = false;
		}

		if (confirmationPopup && !ImGui::IsPopupOpen(__("Confirmation"))) {
			ImGui::OpenPopup(__("Confirmation"), ImGuiPopupFlags_AnyPopupLevel);
		}

		if (ImGui::BeginPopupModal(__("Confirmation"), &confirmationPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text(__("File exists, do you want to overwrite it?"));

			ImGuiStyle& style = ImGui::GetStyle();
			float width = 0.0f;
			width += ImGui::CalcTextSize(__("Yes")).x;
			width += style.ItemSpacing.x;
			width += ImGui::CalcTextSize(__("No!")).x;
			AlignForWidth(width);

			if(ImGui::Button(__("Yes"))) {
				m_finalize(m_inputTextbox);
			}
			ImGui::SameLine();
			if(ImGui::Button(__("No"))) {
				confirmationPopup = false;
			}
			ImGui::EndPopup();
		}
	}
}
