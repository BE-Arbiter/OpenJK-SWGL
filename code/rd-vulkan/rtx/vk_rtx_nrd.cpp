/*
===========================================================================
Copyright (C) 1999 - 2005, Id Software, Inc.
Copyright (C) 2000 - 2013, Raven Software, Inc.
Copyright (C) 2001 - 2013, Activision, Inc.
Copyright (C) 2013 - 2015, OpenJK contributors

This file is part of the OpenJK source code.

OpenJK is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, see <http://www.gnu.org/licenses/>.
===========================================================================
*/

// This software contains source code provided by NVIDIA Corporation.

// NVIDIA NRD denoiser instance. No Vulkan objects and no rendering use yet.

#include "tr_local.h"
#include "NRD.h"

static nrd::Instance *nrd_instance = NULL;

/*
=============
vk_rtx_nrd_init

Creates one RELAX diffuse/specular denoiser (identifier 0) and prints its layout.
=============
*/
void vk_rtx_nrd_init( void )
{
	const nrd::DenoiserDesc denoiser = { 0, nrd::Denoiser::RELAX_DIFFUSE_SPECULAR };
	nrd::InstanceCreationDesc desc = {};
	nrd::Result result;
	const nrd::InstanceDesc *info;
	const nrd::LibraryDesc *lib;

	if ( nrd_instance )
		vk_rtx_nrd_shutdown();

	// Null callbacks select the default allocator.
	desc.denoisers = &denoiser;
	desc.denoisersNum = 1;

	result = nrd::CreateInstance( desc, nrd_instance );
	if ( result != nrd::Result::SUCCESS || !nrd_instance ) {
		ri.Printf( PRINT_WARNING, "NRD: CreateInstance failed (result %d)\n", (int)result );
		nrd_instance = NULL;
		return;
	}

	if ( !pt_verbose || !pt_verbose->integer )
		return;

	lib = nrd::GetLibraryDesc();
	info = nrd::GetInstanceDesc( *nrd_instance );

	ri.Printf( PRINT_ALL, "NRD v%d.%d.%d: RELAX_DIFFUSE_SPECULAR instance created\n",
		lib->versionMajor, lib->versionMinor, lib->versionBuild );
	ri.Printf( PRINT_ALL, "NRD: %u pipelines, permanent pool %u, transient pool %u\n",
		info->pipelinesNum, info->permanentPoolSize, info->transientPoolSize );
	ri.Printf( PRINT_ALL, "NRD: constant buffer max %u bytes, %u samplers, entry point %s\n",
		info->constantBufferMaxDataSize, info->samplersNum, info->shaderEntryPoint );
	ri.Printf( PRINT_ALL, "NRD: spaces cb/samplers %u, resources %u; registers cb %u, samplers %u, resources %u\n",
		info->constantBufferAndSamplersSpaceIndex, info->resourcesSpaceIndex,
		info->constantBufferRegisterIndex, info->samplersBaseRegisterIndex, info->resourcesBaseRegisterIndex );
	ri.Printf( PRINT_ALL, "NRD: SPIR-V offsets s %u, b %u, u %u, t %u\n",
		lib->spirvBindingOffsets.samplerOffset, lib->spirvBindingOffsets.constantBufferOffset,
		lib->spirvBindingOffsets.storageTextureAndBufferOffset, lib->spirvBindingOffsets.textureOffset );
}

/*
=============
vk_rtx_nrd_shutdown
=============
*/
void vk_rtx_nrd_shutdown( void )
{
	if ( !nrd_instance )
		return;

	nrd::DestroyInstance( *nrd_instance );
	nrd_instance = NULL;
}
