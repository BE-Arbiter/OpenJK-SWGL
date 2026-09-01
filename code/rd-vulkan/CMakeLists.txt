#============================================================================
# Copyright (C) 2013 - 2018, OpenJK contributors
#
# This file is part of the OpenJK source code.
#
# OpenJK is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, see <http://www.gnu.org/licenses/>.
#============================================================================

# Make sure the user is not executing this script directly
if(NOT InOpenJK)
	message(FATAL_ERROR "Use the top-level cmake script!")
endif(NOT InOpenJK)

# Check for dependencies first

set(SPVulkanRendererIncludeDirectories ${SPDir})
set(SPVulkanRendererIncludeDirectories
	${SPVulkanRendererIncludeDirectories}
	${SharedDir}
	${SPDir}
	"${SPDir}/rd-vulkan"
	"${SharedDir}/rd-vulkan"
	"${SharedDir}/rd-vulkan/vulkan"
	"${GSLIncludeDirectory}"
	)

set(SPVulkanRendererFiles
	"${SPDir}/rd-vulkan/G2_API.cpp"
	"${SPDir}/rd-vulkan/G2_bolts.cpp"
	"${SPDir}/rd-vulkan/G2_bones.cpp"
	"${SharedDir}/rd-vulkan/G2_gore_r2.cpp"
	"${SharedDir}/rd-vulkan/G2_gore_r2.h"
	"${SPDir}/rd-vulkan/G2_misc.cpp"
	"${SPDir}/rd-vulkan/G2_surfaces.cpp"
	"${SharedDir}/rd-vulkan/tr_backend.cpp"
	"${SharedDir}/rd-vulkan/tr_bsp.cpp"
	"${SharedDir}/rd-vulkan/tr_cache.h"
	"${SharedDir}/rd-vulkan/tr_cache.cpp"
	"${SharedDir}/rd-vulkan/tr_cmds.cpp"
	"${SharedDir}/rd-vulkan/tr_curve.cpp"
	"${SharedDir}/rd-vulkan/tr_decals.cpp"
	"${SharedDir}/rd-vulkan/tr_ghoul2.cpp"
	"${SharedDir}/rd-vulkan/tr_image.cpp"
	"${SPDir}/rd-vulkan/tr_init.cpp"
	"${SharedDir}/rd-vulkan/tr_light.cpp"
	"${SharedDir}/rd-vulkan/tr_local.h"
	"${SharedDir}/rd-vulkan/tr_main.cpp"
	"${SharedDir}/rd-vulkan/tr_marks.cpp"
	"${SharedDir}/rd-vulkan/tr_mesh.cpp"
	"${SharedDir}/rd-vulkan/tr_model.cpp"
	"${SharedDir}/rd-vulkan/tr_quicksprite.cpp"
	"${SharedDir}/rd-vulkan/tr_quicksprite.h"
	"${SharedDir}/rd-vulkan/tr_scene.cpp"
	"${SharedDir}/rd-vulkan/tr_shade.cpp"
	"${SharedDir}/rd-vulkan/tr_shader.cpp"
	"${SharedDir}/rd-vulkan/tr_shade_calc.cpp"
	"${SharedDir}/rd-vulkan/tr_shadows.cpp"
	"${SharedDir}/rd-vulkan/tr_skin.cpp"
	"${SharedDir}/rd-vulkan/tr_sky.cpp"
	"${SharedDir}/rd-vulkan/tr_subs.cpp"
	"${SharedDir}/rd-vulkan/tr_surface.cpp"
	"${SharedDir}/rd-vulkan/tr_surfacesprites.cpp"
	"${SharedDir}/rd-vulkan/tr_world.cpp"
	"${SharedDir}/rd-vulkan/tr_WorldEffects.cpp"
	"${SharedDir}/rd-vulkan/tr_WorldEffects.h"
	"${SharedDir}/rd-vulkan/vk_attachments.cpp"
	"${SharedDir}/rd-vulkan/vk_bloom.cpp"
	"${SharedDir}/rd-vulkan/vk_cmd.cpp"
	"${SharedDir}/rd-vulkan/vk_debug.cpp"
	"${SharedDir}/rd-vulkan/vk_dynamic_glow.cpp"
	"${SharedDir}/rd-vulkan/vk_flares.cpp"
	"${SharedDir}/rd-vulkan/vk_frame.cpp"
	"${SharedDir}/rd-vulkan/vk_image.cpp"
	"${SharedDir}/rd-vulkan/vk_image_process.cpp"
	"${SharedDir}/rd-vulkan/vk_info.cpp"
	"${SharedDir}/rd-vulkan/vk_init.cpp"
	"${SharedDir}/rd-vulkan/vk_instance.cpp"
	"${SharedDir}/rd-vulkan/vk_local.h"
	"${SharedDir}/rd-vulkan/vk_pipelines.cpp"
	"${SharedDir}/rd-vulkan/vk_shaders.cpp"
	"${SharedDir}/rd-vulkan/vk_shade_geometry.cpp"
	"${SharedDir}/rd-vulkan/vk_swapchain.cpp"
	"${SharedDir}/rd-vulkan/vk_vbo.cpp"
	"${SharedDir}/rd-vulkan/vk_vbo_surfacesprites.cpp"
	)
source_group("renderer" FILES ${SPVulkanRendererFiles})

set(SPVulkanRendererGhoul2Files
	"${SPDir}/ghoul2/G2.h"
	"${SPDir}/ghoul2/ghoul2_gore.h")
source_group("ghoul2" FILES ${SPVulkanRendererGhoul2Files})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererGhoul2Files})

set(SPVulkanRendererRdCommonFiles
	"${SPDir}/rd-common/mdx_format.h"
	"${SPDir}/rd-common/tr_common.h"
	"${SPDir}/rd-common/tr_font.cpp"
	"${SPDir}/rd-common/tr_font.h"
	"${SPDir}/rd-common/tr_image_load.cpp"
	"${SPDir}/rd-common/tr_image_jpg.cpp"
	"${SPDir}/rd-common/tr_image_tga.cpp"
	"${SPDir}/rd-common/tr_image_png.cpp"
	"${SPDir}/rd-common/tr_noise.cpp"
	"${SPDir}/rd-common/tr_public.h"
	"${SPDir}/rd-common/tr_types.h")
source_group("rd-common" FILES ${SPVulkanRendererRdCommonFiles})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererRdCommonFiles})

set(SPVulkanRendererVulkanAssets
	"${SharedDir}/rd-vulkan/vulkan/vk_platform.h"
	"${SharedDir}/rd-vulkan/vulkan/vulkan.h"
	"${SharedDir}/rd-vulkan/vulkan/vulkan_core.h"
	"${SharedDir}/rd-vulkan/vulkan/vulkan_win32.h"
	"${SharedDir}/rd-vulkan/vulkan/vulkan_xlib.h"
	"${SharedDir}/rd-vulkan/vulkan/vulkan_xlib_xrandr.h")
source_group("renderer\\vulkan" FILES ${SPVulkanRendererVulkanAssets})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererVulkanAssets})

set(SPVulkanRendererVulkanVideoAssets
    "${SharedDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codecs_common.h"
    "${SharedDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_av1std.h"
    "${SharedDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_av1std_decode.h"
    "${SharedDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_av1std_encode.h"
    "${SharedDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_h264std.h"
    "${SharedDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_h264std_decode.h"
    "${SharedDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_h264std_encode.h"
    "${SharedDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_h265std.h"
    "${SharedDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_h265std_decode.h"
    "${SharedDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_h265std_encode.h"
    "${SharedDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_vp9std.h"
    "${SharedDir}/rd-vulkan/vulkan/vk_video/vulkan_video_codec_vp9std_decode.h"
)
source_group("renderer\\vulkan\\vk_video" FILES ${SPVulkanRendererVulkanVideoAssets})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererVulkanVideoAssets})

set(SPVulkanRendererUtils
	"${SharedDir}/rd-vulkan/utils/stb_dxt.h")
source_group("renderer\\utils" FILES ${SPVulkanRendererUtils})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererUtils})

set(SPVulkanRendererCommonFiles
	"${SPDir}/qcommon/matcomp.cpp"
	"${SPDir}/qcommon/q_shared.cpp"

	${SharedCommonFiles})
source_group("common" FILES ${SPVulkanRendererCommonFiles})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererCommonFiles})

set(SPVulkanRendererCommonSafeFiles
	${SharedCommonSafeFiles}
	)
source_group("common/safe" FILES ${SPVulkanRendererCommonSafeFiles})
set(SPVulkanRendererFiles ${SPVulkanRendererFiles} ${SPVulkanRendererCommonSafeFiles})

# Transparently use either bundled or system libjpeg.
list(APPEND SPVulkanRendererIncludeDirectories ${JPEG_INCLUDE_DIR})
list(APPEND SPVulkanRendererLibraries          ${JPEG_LIBRARIES})

# Transparently use either bundled or system libpng.  Order is important --
# libpng used zlib, so it must come before it on the linker command line.
list(APPEND SPVulkanRendererIncludeDirectories ${PNG_INCLUDE_DIRS})
list(APPEND SPVulkanRendererLibraries          ${PNG_LIBRARIES})

# Transparently use either bundled or system zlib.
list(APPEND SPVulkanRendererIncludeDirectories ${ZLIB_INCLUDE_DIR})
list(APPEND SPVulkanRendererLibraries          ${ZLIB_LIBRARIES})

# Transparently use our bundled minizip.
list(APPEND SPVulkanRendererIncludeDirectories ${MINIZIP_INCLUDE_DIRS})
list(APPEND SPVulkanRendererLibraries          ${MINIZIP_LIBRARIES})

#find_package(OpenGL REQUIRED)
#set(SPVulkanRendererIncludeDirectories ${SPVulkanRendererIncludeDirectories} ${OPENGL_INCLUDE_DIR})
#set(SPVulkanRendererLibraries ${SPVulkanRendererLibraries} ${OPENGL_LIBRARIES})

set(SPVulkanRendererIncludeDirectories ${SPVulkanRendererIncludeDirectories} ${OpenJKLibDir})
add_library(${SPVulkan} SHARED ${SPVulkanRendererFiles})

if(NOT MSVC)
	# remove "lib" prefix for .so/.dylib files
	set_target_properties(${SPVulkan} PROPERTIES PREFIX "")
endif()

if(WIN32)
	install(TARGETS ${SPVulkan}
		RUNTIME
		DESTINATION ${JKAInstallDir}
		COMPONENT ${JKASPClientComponent})
else(WIN32)
	if(MakeApplicationBundles AND BuildSPEngine)
		install(TARGETS ${SPVulkan}
			LIBRARY
			DESTINATION "${JKAInstallDir}/${SPEngine}.app/Contents/MacOS/"
			COMPONENT ${JKASPClientComponent})
	else()
		install(TARGETS ${SPVulkan}
			LIBRARY
			DESTINATION ${JKAInstallDir}
			COMPONENT ${JKASPClientComponent})
	endif()
endif()

set(SPVulkanDefines ${SharedDefines} "RENDERER")
set_target_properties(${SPVulkan} PROPERTIES COMPILE_DEFINITIONS "${SPVulkanDefines}")

# Hide symbols not explicitly marked public.
set_property(TARGET ${SPVulkan} APPEND PROPERTY COMPILE_OPTIONS ${OPENJK_VISIBILITY_FLAGS})

set_target_properties(${SPVulkan} PROPERTIES INCLUDE_DIRECTORIES "${SPVulkanRendererIncludeDirectories}")
set_target_properties(${SPVulkan} PROPERTIES PROJECT_LABEL "SP Vulkan Renderer")
target_link_libraries(${SPVulkan} ${SPVulkanRendererLibraries})
