#include <geodesuka/core/gcl/image.h>

// Standard C Libs
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>
#include <float.h>

#include <vector>

#include <geodesuka/core/math.h>

#include <geodesuka/core/util/variable.h>

//#include <geodesuka/core/object.h>
//#include <geodesuka/core/object/system_window.h>

namespace geodesuka::core::gcl {

	image::prop::prop() {
		this->ArrayLayerCount	= 1;
		this->SampleCounts		= sample::COUNT_1;
		this->Tiling			= tiling::OPTIMAL;
		this->Usage				= 0;
	}

	image::image() {
		this->Context		= nullptr;
		this->CreateInfo	= {};
		this->Handle		= VK_NULL_HANDLE;
		this->AllocateInfo	= {};
		this->MemoryHandle	= VK_NULL_HANDLE;
		this->MemoryType	= 0;
		this->BytesPerPixel = 0;
		this->MemorySize	= 0;
		this->Layout		= NULL;
		this->MipExtent		= NULL;
	}

	image::image(context* aContext, int aMemoryType, prop aProperty, int aFormat, int aWidth, int aHeight, int aDepth, void* aTextureData) {
		if (aContext == nullptr) return;
		this->Context = aContext;

		VkResult Result							= VkResult::VK_SUCCESS;
		this->CreateInfo.sType					= VkStructureType::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		this->CreateInfo.pNext					= NULL;
		this->CreateInfo.flags					= 0;
		if ((aWidth > 1) && (aHeight > 1) && (aDepth > 1)) {
			this->CreateInfo.imageType = VkImageType::VK_IMAGE_TYPE_3D;
		}
		else if ((aWidth > 1) && (aHeight > 1)) {
			this->CreateInfo.imageType = VkImageType::VK_IMAGE_TYPE_2D;
		}
		else if (aWidth > 1) {
			this->CreateInfo.imageType = VkImageType::VK_IMAGE_TYPE_1D;
		}
		else {
			return;
		}
		this->CreateInfo.format					= (VkFormat)aFormat;
		switch (this->CreateInfo.imageType) {
		default:
			return;
		case VK_IMAGE_TYPE_1D:
			this->CreateInfo.extent = { (uint32_t)aWidth, 1u, 1u };
			break;
		case VK_IMAGE_TYPE_2D:
			this->CreateInfo.extent = { (uint32_t)aWidth, (uint32_t)aHeight, 1u };
			break;
		case VK_IMAGE_TYPE_3D:
			this->CreateInfo.extent = { (uint32_t)aWidth, (uint32_t)aHeight, (uint32_t)aDepth };
			break;
		}
		this->CreateInfo.mipLevels				= this->miplevelcalc(this->CreateInfo.imageType, this->CreateInfo.extent);
		this->CreateInfo.arrayLayers			= aProperty.ArrayLayerCount;
		this->CreateInfo.samples				= (VkSampleCountFlagBits)aProperty.SampleCounts;
		this->CreateInfo.tiling					= (VkImageTiling)aProperty.Tiling;
		this->CreateInfo.usage					= aProperty.Usage | image::usage::TRANSFER_SRC | image::usage::TRANSFER_DST; // Enable Transfer
		this->CreateInfo.sharingMode			= VkSharingMode::VK_SHARING_MODE_EXCLUSIVE;
		this->CreateInfo.queueFamilyIndexCount	= 0;
		this->CreateInfo.pQueueFamilyIndices	= NULL;
		this->CreateInfo.initialLayout			= VkImageLayout::VK_IMAGE_LAYOUT_UNDEFINED;

		this->BytesPerPixel = this->bytesperpixel(this->CreateInfo.format);
		// So apparently mip levels can have their own image layouts.
		// and possibly elements of a texture array.
		this->MemorySize = this->CreateInfo.arrayLayers * this->CreateInfo.extent.width * this->CreateInfo.extent.height * this->CreateInfo.extent.depth * this->BytesPerPixel;

		Result = vkCreateImage(this->Context->handle(), &this->CreateInfo, NULL, &this->Handle);

		// Allocate device memory for image handle
		if (Result == VkResult::VK_SUCCESS) {
			VkMemoryRequirements MemoryRequirements;
			vkGetImageMemoryRequirements(this->Context->handle(), this->Handle, &MemoryRequirements);

			VkPhysicalDeviceMemoryProperties MemoryProperties = this->Context->parent()->get_memory_properties();

			this->AllocateInfo.sType			= VkStructureType::VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			this->AllocateInfo.pNext			= NULL;
			this->AllocateInfo.allocationSize	= MemoryRequirements.size;

			// First search for exact memory type.
			this->MemoryType = -1;
			for (uint32_t i = 0; i < MemoryProperties.memoryTypeCount; i++) {
				if (((MemoryRequirements.memoryTypeBits & (1 << i)) >> i) && (MemoryProperties.memoryTypes[i].propertyFlags == aMemoryType)) {
					this->AllocateInfo.memoryTypeIndex = i;
					this->MemoryType = aMemoryType;
					break;
				}
			}

			// If exact memory type doesn't exist, search for approximate type.
			if (this->MemoryType == -1) {
				for (uint32_t i = 0; i < MemoryProperties.memoryTypeCount; i++) {
					if (((MemoryRequirements.memoryTypeBits & (1 << i)) >> i) && ((MemoryProperties.memoryTypes[i].propertyFlags & aMemoryType) == aMemoryType)) {
						this->AllocateInfo.memoryTypeIndex = i;
						this->MemoryType = MemoryProperties.memoryTypes[i].propertyFlags;
						break;
					}
				}
			}

			if (this->MemoryType == -1) {
				vkDestroyImage(this->Context->handle(), this->Handle, NULL);
				this->Handle = VK_NULL_HANDLE;
				return;
			}

			Result = vkAllocateMemory(this->Context->handle(), &this->AllocateInfo, NULL, &this->MemoryHandle);
		}

		// Bind image handle to memory.
		if (Result == VkResult::VK_SUCCESS) {
			Result = vkBindImageMemory(this->Context->handle(), this->Handle, this->MemoryHandle, 0);
		}

		// TODO: Fix later for failed allocations.
		// All initialized layouts is above.
		// i = mip level, and j = array level;
		this->Layout = (VkImageLayout**)malloc(this->CreateInfo.mipLevels * sizeof(VkImageLayout*));
		for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
			this->Layout[i] = (VkImageLayout*)malloc(this->CreateInfo.arrayLayers * sizeof(VkImageLayout));
			for (uint32_t j = 0; j < this->CreateInfo.arrayLayers; j++) {
				this->Layout[i][j] = this->CreateInfo.initialLayout;
			}
		}

		// Mip Level resolutions.
		this->MipExtent = (VkExtent3D*)malloc(this->CreateInfo.mipLevels * sizeof(VkExtent3D));
		for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
			switch (this->CreateInfo.imageType) {
			default:
				return;
			case VK_IMAGE_TYPE_1D:
				this->MipExtent[i] = { (this->CreateInfo.extent.width >> i), 1u, 1u };
				break;
			case VK_IMAGE_TYPE_2D:
				this->MipExtent[i] = { (this->CreateInfo.extent.width >> i), (this->CreateInfo.extent.height >> i), 1u };
				break;
			case VK_IMAGE_TYPE_3D:
				this->MipExtent[i] = { (this->CreateInfo.extent.width >> i), (this->CreateInfo.extent.height >> i), (this->CreateInfo.extent.depth >> i) };
				break;
			}
		}

	
		// Create staging buffer and prepare for transfer.
		buffer StagingBuffer(
			Context,
			device::HOST_VISIBLE | device::HOST_COHERENT,
			buffer::TRANSFER_SRC | buffer::TRANSFER_DST,
			this->MemorySize,
			aTextureData
		);

		VkSubmitInfo Submission[2] = { {}, {} };
		VkCommandBuffer CommandBuffer[2] = { VK_NULL_HANDLE, VK_NULL_HANDLE };
		VkSemaphoreCreateInfo SemaphoreCreateInfo{};
		VkSemaphore Semaphore;
		VkFenceCreateInfo FenceCreateInfo{};
		VkFence Fence[2];
		VkPipelineStageFlags PipelineStage = VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

		// Transfer.
		Submission[0].sType						= VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
		Submission[0].pNext						= NULL;
		Submission[0].waitSemaphoreCount		= 0;
		Submission[0].pWaitSemaphores			= NULL;
		Submission[0].pWaitDstStageMask			= NULL;
		Submission[0].commandBufferCount		= 1;
		Submission[0].pCommandBuffers			= &CommandBuffer[0];
		Submission[0].signalSemaphoreCount		= 1;
		Submission[0].pSignalSemaphores			= &Semaphore;

		// Graphics Submission
		Submission[1].sType						= VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
		Submission[1].pNext						= NULL;
		Submission[1].waitSemaphoreCount		= 1;
		Submission[1].pWaitSemaphores			= &Semaphore;
		Submission[1].pWaitDstStageMask			= &PipelineStage;
		Submission[1].commandBufferCount		= 1;
		Submission[1].pCommandBuffers			= &CommandBuffer[1];
		Submission[1].signalSemaphoreCount		= 0;
		Submission[1].pSignalSemaphores			= NULL;

		SemaphoreCreateInfo.sType				= VkStructureType::VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		SemaphoreCreateInfo.pNext				= NULL;
		SemaphoreCreateInfo.flags				= 0;

		FenceCreateInfo.sType					= VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		FenceCreateInfo.pNext					= NULL;
		FenceCreateInfo.flags					= 0;

		Result = vkCreateSemaphore(this->Context->handle(), &SemaphoreCreateInfo, NULL, &Semaphore);
		Result = vkCreateFence(this->Context->handle(), &FenceCreateInfo, NULL, &Fence[0]);
		Result = vkCreateFence(this->Context->handle(), &FenceCreateInfo, NULL, &Fence[1]);
		CommandBuffer[0] = (*this << StagingBuffer);
		CommandBuffer[1] = this->generate_mipmaps(VkFilter::VK_FILTER_NEAREST);

		Result = this->Context->submit(device::TRANSFER, 1, &Submission[0], Fence[0]);
		Result = this->Context->submit(device::GRAPHICS, 1, &Submission[1], Fence[1]);
		Result = vkWaitForFences(this->Context->handle(), 2, Fence, VK_TRUE, UINT64_MAX);
		
		vkDestroyFence(this->Context->handle(), Fence[0], NULL);
		vkDestroyFence(this->Context->handle(), Fence[1], NULL);
		vkDestroySemaphore(this->Context->handle(), Semaphore, NULL);
		this->Context->destroy(device::qfs::TRANSFER, CommandBuffer[0]);
		this->Context->destroy(device::qfs::GRAPHICS, CommandBuffer[1]);

	}

	image::~image() {
		if (this->Layout != NULL) {
			for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
				free(this->Layout[i]); this->Layout[i] = NULL;
			}
		}
		free(this->Layout); this->Layout = NULL;
		free(this->MipExtent); this->MipExtent = NULL;
		if (this->Context != nullptr) {
			vkDestroyImage(this->Context->handle(), this->Handle, NULL);
			this->Handle = VK_NULL_HANDLE;
			vkFreeMemory(this->Context->handle(), this->MemoryHandle, NULL);
			this->MemoryHandle = VK_NULL_HANDLE;
		}
		this->Context = nullptr;
	}

	image::image(image& aInput) {
		this->Context			= aInput.Context;
		this->CreateInfo		= aInput.CreateInfo;
		this->AllocateInfo		= aInput.AllocateInfo;
		this->MemoryType		= aInput.MemoryType;
		this->BytesPerPixel		= aInput.BytesPerPixel;
		this->MemorySize		= aInput.MemorySize;

		this->MipExtent = (VkExtent3D*)malloc(this->CreateInfo.mipLevels * sizeof(VkExtent3D));
		this->Layout = (VkImageLayout**)malloc(this->CreateInfo.mipLevels * sizeof(VkImageLayout*));
		if (this->Layout != NULL) {
			for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
				this->Layout[i] = (VkImageLayout*)malloc(this->CreateInfo.arrayLayers * sizeof(VkImageLayout));
			}
		}

		bool AllocationFailure = false;
		AllocationFailure |= (this->MipExtent == NULL);
		AllocationFailure |= (this->Layout == NULL);
		if (this->Layout != NULL) {
			for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
				AllocationFailure |= (this->Layout[i] == NULL);
			}
		}
		if (AllocationFailure) {
			if (this->Layout != NULL) {
				for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
					free(this->Layout[i]); this->Layout[i] = NULL;
				}
			}
			free(this->Layout); this->Layout = NULL;
			free(this->MipExtent); this->MipExtent = NULL;
			return;
		}

		// Set initial layout for all mip levels and array elements.
		for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
			for (uint32_t j = 0; j < this->CreateInfo.arrayLayers; j++) {
				this->Layout[i][j] = this->CreateInfo.initialLayout;
			}
		}

		// Generate Mip resolutions.
		// Mip Level resolutions.
		this->MipExtent = (VkExtent3D*)malloc(this->CreateInfo.mipLevels * sizeof(VkExtent3D));
		for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
			switch (this->CreateInfo.imageType) {
			default:
				return;
			case VK_IMAGE_TYPE_1D:
				this->MipExtent[i] = { (this->CreateInfo.extent.width >> i), 1u, 1u };
				break;
			case VK_IMAGE_TYPE_2D:
				this->MipExtent[i] = { (this->CreateInfo.extent.width >> i), (this->CreateInfo.extent.height >> i), 1u };
				break;
			case VK_IMAGE_TYPE_3D:
				this->MipExtent[i] = { (this->CreateInfo.extent.width >> i), (this->CreateInfo.extent.height >> i), (this->CreateInfo.extent.depth >> i) };
				break;
			}
		}

		VkResult Result = VkResult::VK_SUCCESS;
		Result = vkCreateImage(this->Context->handle(), &this->CreateInfo, NULL, &this->Handle);
		if (Result == VkResult::VK_SUCCESS) {
			Result = vkAllocateMemory(this->Context->handle(), &this->AllocateInfo, NULL, &this->MemoryHandle);
		}
		if (Result == VkResult::VK_SUCCESS) {
			Result = vkBindImageMemory(this->Context->handle(), this->Handle, this->MemoryHandle, 0);
		}

		// If successful.
		if (Result != VkResult::VK_SUCCESS) {
			if (this->Layout != NULL) {
				for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
					free(this->Layout[i]); this->Layout[i] = NULL;
				}
			}
			free(this->Layout); this->Layout = NULL;
			free(this->MipExtent); this->MipExtent = NULL;
			if (this->Context != nullptr) {
				vkDestroyImage(this->Context->handle(), this->Handle, NULL);
				this->Handle = VK_NULL_HANDLE;
				vkFreeMemory(this->Context->handle(), this->MemoryHandle, NULL);
				this->MemoryHandle = VK_NULL_HANDLE;
			}
			this->Context = nullptr;
			return;
		}

		VkSubmitInfo Submission{};
		VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
		VkFenceCreateInfo FenceCreateInfo{};
		VkFence Fence;


		Submission.sType					= VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
		Submission.pNext					= NULL;
		Submission.waitSemaphoreCount		= 0;
		Submission.pWaitSemaphores			= NULL;
		Submission.pWaitDstStageMask		= NULL;
		Submission.commandBufferCount		= 1;
		Submission.pCommandBuffers			= &CommandBuffer;
		Submission.signalSemaphoreCount		= 0;
		Submission.pSignalSemaphores		= NULL;

		FenceCreateInfo.sType				= VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		FenceCreateInfo.pNext				= NULL;
		FenceCreateInfo.flags				= 0;

		CommandBuffer = (*this << aInput);
		Result = vkCreateFence(this->Context->handle(), &FenceCreateInfo, NULL, &Fence);
		Result = this->Context->submit(device::qfs::TRANSFER, 1, &Submission, Fence);
		Result = vkWaitForFences(this->Context->handle(), 1, &Fence, VK_TRUE, UINT64_MAX);
		this->Context->destroy(device::qfs::TRANSFER, CommandBuffer);
		vkDestroyFence(this->Context->handle(), Fence, NULL);

	}

	image::image(image&& aInput) noexcept {
		this->Context			= aInput.Context;
		this->CreateInfo		= aInput.CreateInfo;
		this->Handle			= aInput.Handle;
		this->AllocateInfo		= aInput.AllocateInfo;
		this->MemoryHandle		= aInput.MemoryHandle;
		this->MemoryType		= aInput.MemoryType;
		this->BytesPerPixel		= aInput.BytesPerPixel;
		this->MemorySize		= aInput.MemorySize;
		this->Layout			= aInput.Layout;
		this->MipExtent			= aInput.MipExtent;

		aInput.Context			= nullptr;
		aInput.CreateInfo		= {};
		aInput.Handle			= VK_NULL_HANDLE;
		aInput.AllocateInfo		= {};
		aInput.MemoryHandle		= VK_NULL_HANDLE;
		aInput.MemoryType		= 0;
		aInput.BytesPerPixel	= 0;
		aInput.MemorySize		= 0;
		aInput.Layout			= NULL;
		aInput.MipExtent		= NULL;
	}

	image& image::operator=(image& aRhs) {
		if (this == &aRhs) return *this;
		this->pmclearall();

		this->Context			= aRhs.Context;
		this->CreateInfo		= aRhs.CreateInfo;
		this->AllocateInfo		= aRhs.AllocateInfo;
		this->MemoryType		= aRhs.MemoryType;
		this->BytesPerPixel		= aRhs.BytesPerPixel;
		this->MemorySize		= aRhs.MemorySize;

		// Allocate Host memory.
		this->Layout = (VkImageLayout**)malloc(this->CreateInfo.mipLevels * sizeof(VkImageLayout*));
		for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
			this->Layout[i] = (VkImageLayout*)malloc(this->CreateInfo.arrayLayers * sizeof(VkImageLayout));
		}
		this->MipExtent = (VkExtent3D*)malloc(this->CreateInfo.mipLevels * sizeof(VkExtent3D));

		// Allocate Device memory.
		VkResult Result = VkResult::VK_SUCCESS;
		Result = vkCreateImage(this->Context->handle(), &this->CreateInfo, NULL, &this->Handle);
		if (Result == VkResult::VK_SUCCESS) {
			Result = vkAllocateMemory(this->Context->handle(), &this->AllocateInfo, NULL, &this->MemoryHandle);
		}
		if (Result == VkResult::VK_SUCCESS) {
			Result = vkBindImageMemory(this->Context->handle(), this->Handle, this->MemoryHandle, 0);
		}

		// Check for memory allocation failure.
		bool AllocationFailure = false;
		AllocationFailure |= (this->Layout == NULL);
		for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
			AllocationFailure |= (this->Layout[i] == NULL);
		}
		AllocationFailure |= (this->MipExtent == NULL);
		
		// clear all allocation failure.
		if (AllocationFailure) {
			this->pmclearall();
			return *this;
		}

		// Set initial layouts.
		for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
			for (uint32_t j = 0; j < this->CreateInfo.arrayLayers; j++) {
				this->Layout[i][j] = this->CreateInfo.initialLayout;
			}
		}

		// Generate Mip resolutions.
		// Mip Level resolutions.
		this->MipExtent = (VkExtent3D*)malloc(this->CreateInfo.mipLevels * sizeof(VkExtent3D));
		for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
			switch (this->CreateInfo.imageType) {
			default: 
				break;
			case VK_IMAGE_TYPE_1D:
				this->MipExtent[i] = { (this->CreateInfo.extent.width >> i), 1u, 1u };
				break;
			case VK_IMAGE_TYPE_2D:
				this->MipExtent[i] = { (this->CreateInfo.extent.width >> i), (this->CreateInfo.extent.height >> i), 1u };
				break;
			case VK_IMAGE_TYPE_3D:
				this->MipExtent[i] = { (this->CreateInfo.extent.width >> i), (this->CreateInfo.extent.height >> i), (this->CreateInfo.extent.depth >> i) };
				break;
			}
		}


		VkSubmitInfo Submission{};
		VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
		VkFenceCreateInfo FenceCreateInfo{};
		VkFence Fence;

		Submission.sType					= VkStructureType::VK_STRUCTURE_TYPE_SUBMIT_INFO;
		Submission.pNext					= NULL;
		Submission.waitSemaphoreCount		= 0;
		Submission.pWaitSemaphores			= NULL;
		Submission.pWaitDstStageMask		= NULL;
		Submission.commandBufferCount		= 1;
		Submission.pCommandBuffers			= &CommandBuffer;
		Submission.signalSemaphoreCount		= 0;
		Submission.pSignalSemaphores		= NULL;

		FenceCreateInfo.sType				= VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		FenceCreateInfo.pNext				= NULL;
		FenceCreateInfo.flags				= 0;

		Result = vkCreateFence(this->Context->handle(), &FenceCreateInfo, NULL, &Fence);
		CommandBuffer = (*this << aRhs);
		Result = this->Context->submit(device::qfs::TRANSFER, 1, &Submission, Fence);
		Result = vkWaitForFences(this->Context->handle(), 1, &Fence, VK_TRUE, UINT64_MAX);

		vkDestroyFence(this->Context->handle(), Fence, NULL);
		this->Context->destroy(device::qfs::TRANSFER, CommandBuffer);

		return *this;
	}

	image& image::operator=(image&& aRhs) noexcept {
		this->pmclearall();

		this->Context			= aRhs.Context;
		this->CreateInfo		= aRhs.CreateInfo;
		this->Handle			= aRhs.Handle;
		this->AllocateInfo		= aRhs.AllocateInfo;
		this->MemoryHandle		= aRhs.MemoryHandle;
		this->MemoryType		= aRhs.MemoryType;
		this->BytesPerPixel		= aRhs.BytesPerPixel;
		this->MemorySize		= aRhs.MemorySize;
		this->Layout			= aRhs.Layout;
		this->MipExtent			= aRhs.MipExtent;

		aRhs.Context		= nullptr;
		aRhs.CreateInfo		= {};
		aRhs.Handle			= VK_NULL_HANDLE;
		aRhs.AllocateInfo	= {};
		aRhs.MemoryHandle	= VK_NULL_HANDLE;
		aRhs.MemoryType		= 0;
		aRhs.BytesPerPixel	= 0;
		aRhs.MemorySize		= 0;
		aRhs.Layout			= NULL;
		aRhs.MipExtent		= NULL;

		return *this;
	}

	VkCommandBuffer image::operator<<(image& aRhs) {
		VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
		// Cannot do transfer operations if they do not share the
		// same context.
		if (this->Context != aRhs.Context) return CommandBuffer;
		// Check if dimensions are equal.
		if ((this->CreateInfo.extent.width != aRhs.CreateInfo.extent.width) ||
			(this->CreateInfo.extent.height != aRhs.CreateInfo.extent.height) ||
			(this->CreateInfo.extent.depth != aRhs.CreateInfo.extent.depth) ||
			(this->CreateInfo.arrayLayers != aRhs.CreateInfo.arrayLayers) ||
			(this->CreateInfo.mipLevels != aRhs.CreateInfo.mipLevels) ||
			(this->CreateInfo.format != aRhs.CreateInfo.format)
		) return CommandBuffer;

		// Check if proper usage flags are enabled.
		if (
			((this->CreateInfo.usage & image::usage::TRANSFER_DST) != image::usage::TRANSFER_DST) 
			||
			((aRhs.CreateInfo.usage & image::usage::TRANSFER_SRC) != image::usage::TRANSFER_SRC)
		) return CommandBuffer;

		// Ready to setup transfer operations.
		VkResult Result = VkResult::VK_SUCCESS;
		VkCommandBufferBeginInfo BeginInfo{};
		std::vector<VkImageMemoryBarrier> Barrier;
		std::vector<VkImageCopy> Region;

		BeginInfo.sType					= VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		BeginInfo.pNext					= NULL;
		BeginInfo.flags					= 0;
		BeginInfo.pInheritanceInfo		= NULL;

		// Use Barrier layout transitions for al
		for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
			for (uint32_t j = 0; j < this->CreateInfo.arrayLayers; j++) {

				// Image layout transitions for source.
				if (this->Layout[i][j] != VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
					VkImageMemoryBarrier temp{};

					temp.sType								= VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					temp.pNext								= NULL;
					temp.srcAccessMask						= 0; // Hopefully finishes all previous writes?
					temp.dstAccessMask						= VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
					temp.oldLayout							= this->Layout[i][j];
					temp.newLayout							= VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
					temp.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
					temp.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
					temp.image								= this->Handle;
					temp.subresourceRange.aspectMask		= VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
					temp.subresourceRange.baseMipLevel		= i;
					temp.subresourceRange.levelCount		= 1;
					temp.subresourceRange.baseArrayLayer	= j;
					temp.subresourceRange.layerCount		= 1;
					this->Layout[i][j]						= temp.newLayout;
					Barrier.push_back(temp);
				}

				// Image layout transitions for source.
				if (aRhs.Layout[i][j] != VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
					VkImageMemoryBarrier temp{};

					temp.sType								= VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					temp.pNext								= NULL;
					temp.srcAccessMask						= VkAccessFlagBits::VK_ACCESS_MEMORY_WRITE_BIT; // Hopefully finishes all previous writes?
					temp.dstAccessMask						= VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
					temp.oldLayout							= aRhs.Layout[i][j];
					temp.newLayout							= VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
					temp.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
					temp.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
					temp.image								= aRhs.Handle;
					temp.subresourceRange.aspectMask		= VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
					temp.subresourceRange.baseMipLevel		= i;
					temp.subresourceRange.levelCount		= 1;
					temp.subresourceRange.baseArrayLayer	= j;
					temp.subresourceRange.layerCount		= 1;
					aRhs.Layout[i][j]						= temp.newLayout;
					Barrier.push_back(temp);
				}

			}

			VkImageCopy SubRegion{};
			// Specify memory regions to copy.
			SubRegion.srcSubresource.aspectMask			= VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
			SubRegion.srcSubresource.mipLevel			= i;
			SubRegion.srcSubresource.baseArrayLayer		= 0;
			SubRegion.srcSubresource.layerCount			= aRhs.CreateInfo.arrayLayers;
			SubRegion.srcOffset							= { 0, 0, 0 };
			SubRegion.dstSubresource.aspectMask			= VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
			SubRegion.dstSubresource.mipLevel			= i;
			SubRegion.dstSubresource.baseArrayLayer		= 0;
			SubRegion.dstSubresource.layerCount			= this->CreateInfo.arrayLayers;
			SubRegion.dstOffset							= { 0, 0, 0 };
			SubRegion.extent							= this->MipExtent[i];

			Region.push_back(SubRegion);
		}

		CommandBuffer = this->Context->create(device::qfs::TRANSFER);
		Result = vkBeginCommandBuffer(CommandBuffer, &BeginInfo);

		// Transition all images.
		vkCmdPipelineBarrier(CommandBuffer,
			VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, NULL,
			0, NULL,
			Barrier.size(), Barrier.data()
		);

		vkCmdCopyImage(CommandBuffer,
			aRhs.Handle, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			this->Handle, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			Region.size(), Region.data()
		);

		Result = vkEndCommandBuffer(CommandBuffer);

		return CommandBuffer;
	}

	VkCommandBuffer image::operator>>(image& aRhs) {
		return (aRhs << *this);
	}

	VkCommandBuffer image::operator<<(buffer& aRhs) {
		VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
		// Both operands must share the same parent context and memory size.
		if ((this->Context != aRhs.Context) || (this->MemorySize != (size_t)aRhs.CreateInfo.size)) return CommandBuffer;
		// Texture must have enabled, TRANSFER_DST. buffer must have TRANSFER_SRC enabled in usage.
		if (
			((this->CreateInfo.usage & image::usage::TRANSFER_DST) != image::usage::TRANSFER_DST)
			||
			((aRhs.CreateInfo.usage & buffer::usage::TRANSFER_SRC) != buffer::usage::TRANSFER_SRC)
		) return CommandBuffer;

		VkResult Result = VkResult::VK_SUCCESS;
		VkCommandBufferBeginInfo BeginInfo{};
		std::vector<VkImageMemoryBarrier> Barrier(this->CreateInfo.arrayLayers);
		VkBufferImageCopy CopyRegion{};

		BeginInfo.sType									= VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		BeginInfo.pNext									= NULL;
		BeginInfo.flags									= 0;
		BeginInfo.pInheritanceInfo						= NULL;

		for (uint32_t i = 0; i < this->CreateInfo.arrayLayers; i++) {
			Barrier[i].sType								= VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			Barrier[i].pNext								= NULL;
			Barrier[i].srcAccessMask						= 0;
			Barrier[i].dstAccessMask						= VkAccessFlagBits::VK_ACCESS_MEMORY_READ_BIT;
			Barrier[i].oldLayout							= this->Layout[0][i];
			Barrier[i].newLayout							= VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			Barrier[i].srcQueueFamilyIndex					= VK_QUEUE_FAMILY_IGNORED;
			Barrier[i].dstQueueFamilyIndex					= VK_QUEUE_FAMILY_IGNORED;
			Barrier[i].image								= this->Handle;
			Barrier[i].subresourceRange.aspectMask			= VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
			Barrier[i].subresourceRange.baseMipLevel		= 0;
			Barrier[i].subresourceRange.levelCount			= 1;
			Barrier[i].subresourceRange.baseArrayLayer		= i;
			Barrier[i].subresourceRange.layerCount			= 1;
			this->Layout[0][i]								= Barrier[i].newLayout;
		}

		CopyRegion.bufferOffset							= 0;
		CopyRegion.bufferRowLength						= 0;
		CopyRegion.bufferImageHeight					= 0;
		CopyRegion.imageSubresource.aspectMask			= VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
		CopyRegion.imageSubresource.mipLevel			= 0;
		CopyRegion.imageSubresource.baseArrayLayer		= 0;
		CopyRegion.imageSubresource.layerCount			= this->CreateInfo.arrayLayers;
		CopyRegion.imageOffset							= { 0, 0, 0 };
		CopyRegion.imageExtent							= this->CreateInfo.extent;

		//Result = this->Context->create(context::cmdtype::TRANSFER_OTS, 1, &CommandBuffer);
		CommandBuffer = this->Context->create(device::qfs::TRANSFER);
		Result = vkBeginCommandBuffer(CommandBuffer, &BeginInfo);

		// Setup pipeline barriers.
		vkCmdPipelineBarrier(CommandBuffer,
			VkPipelineStageFlagBits::VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
			0,
			0, NULL,
			0, NULL,
			Barrier.size(), Barrier.data()
		);

		// Actual transfer.
		vkCmdCopyBufferToImage(CommandBuffer, 
			aRhs.Handle, 
			this->Handle, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &CopyRegion
		);

		Result = vkEndCommandBuffer(CommandBuffer);

		return CommandBuffer;
	}

	VkCommandBuffer image::operator>>(buffer& aRhs) {
		return (aRhs << *this);
	}

	VkCommandBuffer image::generate_mipmaps(VkFilter aFilter) {
		VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
		if (this->Context == nullptr) return CommandBuffer;


		VkResult Result = VkResult::VK_SUCCESS;
		VkCommandBufferBeginInfo BeginInfo{};

		BeginInfo.sType									= VkStructureType::VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		BeginInfo.pNext									= NULL;
		BeginInfo.flags									= 0;
		BeginInfo.pInheritanceInfo						= NULL;

		//Result = this->Context->create(context::cmdtype::GRAPHICS, 1, &CommandBuffer);
		CommandBuffer = this->Context->create(device::qfs::GRAPHICS);
		Result = vkBeginCommandBuffer(CommandBuffer, &BeginInfo);

		for (uint32_t i = 0; i < this->CreateInfo.mipLevels - 1; i++) {
			std::vector<VkImageMemoryBarrier> Barrier;
			VkImageBlit Region{};

			// Generate source mip level.
			for (uint32_t j = 0; j < this->CreateInfo.arrayLayers; j++) {
				VkImageMemoryBarrier temp;
				temp.sType								= VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				temp.pNext								= NULL;
				temp.srcAccessMask						= VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
				temp.dstAccessMask						= VkAccessFlagBits::VK_ACCESS_TRANSFER_READ_BIT;
				temp.oldLayout							= this->Layout[i][j];
				temp.newLayout							= VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
				temp.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
				temp.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
				temp.image								= this->Handle;
				temp.subresourceRange.aspectMask		= VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
				temp.subresourceRange.baseMipLevel		= i;
				temp.subresourceRange.levelCount		= 1;
				temp.subresourceRange.baseArrayLayer	= j;
				temp.subresourceRange.layerCount		= 1;
				this->Layout[i][j]						= temp.newLayout;
				Barrier.push_back(temp);
			}

			// Generate destination mip level barriers.
			for (uint32_t j = 0; j < this->CreateInfo.arrayLayers; j++) {
				VkImageMemoryBarrier temp;
				temp.sType								= VkStructureType::VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				temp.pNext								= NULL;
				temp.srcAccessMask						= 0;
				temp.dstAccessMask						= VkAccessFlagBits::VK_ACCESS_TRANSFER_WRITE_BIT;
				temp.oldLayout							= this->Layout[i + 1][j];
				temp.newLayout							= VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
				temp.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
				temp.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
				temp.image								= this->Handle;
				temp.subresourceRange.aspectMask		= VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
				temp.subresourceRange.baseMipLevel		= i + 1;
				temp.subresourceRange.levelCount		= 1;
				temp.subresourceRange.baseArrayLayer	= j;
				temp.subresourceRange.layerCount		= 1;
				this->Layout[i + 1][j]					= temp.newLayout;
				Barrier.push_back(temp);
			}

			// Blit regions.
			Region.srcSubresource.aspectMask		= VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
			Region.srcSubresource.mipLevel			= i;
			Region.srcSubresource.baseArrayLayer	= 0;
			Region.srcSubresource.layerCount		= this->CreateInfo.arrayLayers;
			Region.srcOffsets[0]					= { 0, 0, 0 };
			Region.srcOffsets[1]					= { (int32_t)this->MipExtent[i].width, (int32_t)this->MipExtent[i].height, (int32_t)this->MipExtent[i].depth };
			Region.dstSubresource.aspectMask		= VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
			Region.dstSubresource.mipLevel			= i + 1;
			Region.dstSubresource.baseArrayLayer	= 0;
			Region.dstSubresource.layerCount		= this->CreateInfo.arrayLayers;
			Region.dstOffsets[0]					= { 0, 0, 0 };
			Region.dstOffsets[1]					= { (int32_t)this->MipExtent[i + 1].width, (int32_t)this->MipExtent[i + 1].height, (int32_t)this->MipExtent[i + 1].depth };

			vkCmdPipelineBarrier(CommandBuffer,
				VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
				VkPipelineStageFlagBits::VK_PIPELINE_STAGE_TRANSFER_BIT,
				0,
				0, NULL,
				0, NULL,
				Barrier.size(), Barrier.data()
			);

			vkCmdBlitImage(CommandBuffer,
				this->Handle, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				this->Handle, VkImageLayout::VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1, &Region, 
				aFilter
			);

		}

		Result = vkEndCommandBuffer(CommandBuffer);

		return CommandBuffer;
	}

	VkImageView image::view() {
		// Change later after screwing with.
		VkImageView temp = VK_NULL_HANDLE;
		VkImageViewCreateInfo ImageViewCreateInfo{};
		ImageViewCreateInfo.sType				= VkStructureType::VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		ImageViewCreateInfo.pNext				= NULL;
		ImageViewCreateInfo.flags				= 0;
		ImageViewCreateInfo.image				= this->Handle;
		switch (this->CreateInfo.imageType) {
		default:
			break;
		case VkImageType::VK_IMAGE_TYPE_1D:
			ImageViewCreateInfo.viewType			= VkImageViewType::VK_IMAGE_VIEW_TYPE_1D;
			break;
		case VkImageType::VK_IMAGE_TYPE_2D:
			ImageViewCreateInfo.viewType			= VkImageViewType::VK_IMAGE_VIEW_TYPE_2D;
			break;
		case VkImageType::VK_IMAGE_TYPE_3D:
			ImageViewCreateInfo.viewType			= VkImageViewType::VK_IMAGE_VIEW_TYPE_3D;
			break;
		}
		ImageViewCreateInfo.format				= this->CreateInfo.format;
		ImageViewCreateInfo.components.r		= VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
		ImageViewCreateInfo.components.g		= VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
		ImageViewCreateInfo.components.b		= VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
		ImageViewCreateInfo.components.a		= VkComponentSwizzle::VK_COMPONENT_SWIZZLE_IDENTITY;
		ImageViewCreateInfo.subresourceRange.aspectMask			= VkImageAspectFlagBits::VK_IMAGE_ASPECT_COLOR_BIT;
		ImageViewCreateInfo.subresourceRange.baseMipLevel		= 0;
		ImageViewCreateInfo.subresourceRange.levelCount			= this->CreateInfo.mipLevels;
		ImageViewCreateInfo.subresourceRange.baseArrayLayer		= 0;
		ImageViewCreateInfo.subresourceRange.layerCount			= this->CreateInfo.arrayLayers;
		VkResult Result = vkCreateImageView(this->Context->handle(), &ImageViewCreateInfo, NULL, &temp);
		return temp;
	}

	VkAttachmentDescription image::description(loadop aLoadOp, storeop aStoreOp, loadop aStencilLoadOp, storeop aStencilStoreOp, layout aInitialLayout, layout aFinalLayout) {
		VkAttachmentDescription temp;
		temp.flags					= 0;
		temp.format					= this->CreateInfo.format;
		temp.samples				= this->CreateInfo.samples;
		temp.loadOp					= (VkAttachmentLoadOp)aLoadOp;
		temp.storeOp				= (VkAttachmentStoreOp)aStoreOp;
		temp.stencilLoadOp			= (VkAttachmentLoadOp)aStencilLoadOp;
		temp.stencilStoreOp			= (VkAttachmentStoreOp)aStencilStoreOp;
		temp.initialLayout			= (VkImageLayout)aInitialLayout;
		temp.finalLayout			= (VkImageLayout)aFinalLayout;
		return temp;
	}

	/*
	0 = 640, 480
	1 = 320, 240
	2 = 160, 120
	3 = 80, 60
	4 = 40, 30
	5 = 20, 15

	6 mip levels.
	*/
	uint32_t image::miplevelcalc(VkImageType aImageType, VkExtent3D aExtent) {
		uint32_t MipLevelCount = 1;
		switch (aImageType) {
		default:
			return 0;
		case VK_IMAGE_TYPE_1D:
			while (true) {
				if (aExtent.width % 2 == 0) {
					aExtent.width /= 2;
					MipLevelCount += 1;
				}
				else {
					break;
				}
			}
			break;
		case VK_IMAGE_TYPE_2D:
			while (true) {
				if ((aExtent.width % 2 == 0) && (aExtent.height % 2 == 0)) {
					aExtent.width /= 2;
					aExtent.height /= 2;
					MipLevelCount += 1;
				}
				else {
					break;
				}
			}
			break;
		case VK_IMAGE_TYPE_3D:
			while (true) {
				if ((aExtent.width % 2 == 0) && (aExtent.height % 2 == 0) && (aExtent.depth % 2 == 0)) {
					aExtent.width /= 2;
					aExtent.height /= 2;
					aExtent.depth /= 2;
					MipLevelCount += 1;
				}
				else {
					break;
				}
			}
			break;
		}
		return MipLevelCount;
	}

	void image::pmclearall() {
		if (this->Context != nullptr) {
			if (this->Handle != VK_NULL_HANDLE) {
				vkDestroyImage(this->Context->handle(), this->Handle, NULL);
				this->Handle = VK_NULL_HANDLE;
			}
			if (this->MemoryHandle != VK_NULL_HANDLE) {
				vkFreeMemory(this->Context->handle(), this->MemoryHandle, NULL);
				this->MemoryHandle = VK_NULL_HANDLE;
			}
		}

		if (this->Layout != NULL) {
			for (uint32_t i = 0; i < this->CreateInfo.mipLevels; i++) {
				free(this->Layout[i]); this->Layout[i] = NULL;
			}
		}
		free(this->Layout); this->Layout = NULL;
		free(this->MipExtent); this->MipExtent = NULL;

		this->Context			= nullptr;
		this->CreateInfo		= {};
		this->AllocateInfo		= {};
		this->MemoryType		= 0;
		this->BytesPerPixel		= 0;
		this->MemorySize		= 0;
	}

	size_t image::bytesperpixel(VkFormat aFormat) {
		return (image::bitsperpixel(aFormat) / 8);
	}

	// So gross.
	// Group these later based on spec.
	// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#texel-block-size

	/*
	Size table;

	8
	16
	24
	32
	48
	64
	96
	128
	192
	256

	*/

	size_t image::bitsperpixel(VkFormat aFormat) {

		switch (aFormat) {
		default: return 0;
		//case VK_FORMAT_UNDEFINED                                        : return 0;
		case VK_FORMAT_R4G4_UNORM_PACK8                                 : return 8;
		case VK_FORMAT_R4G4B4A4_UNORM_PACK16                            : return 16;
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16                            : return 16;
		case VK_FORMAT_R5G6B5_UNORM_PACK16                              : return 16;
		case VK_FORMAT_B5G6R5_UNORM_PACK16                              : return 16;
		case VK_FORMAT_R5G5B5A1_UNORM_PACK16                            : return 16;
		case VK_FORMAT_B5G5R5A1_UNORM_PACK16                            : return 16;
		case VK_FORMAT_A1R5G5B5_UNORM_PACK16                            : return 16;
		case VK_FORMAT_R8_UNORM                                         : return 8;
		case VK_FORMAT_R8_SNORM                                         : return 8;
		case VK_FORMAT_R8_USCALED                                       : return 8;
		case VK_FORMAT_R8_SSCALED                                       : return 8;
		case VK_FORMAT_R8_UINT                                          : return 8;
		case VK_FORMAT_R8_SINT                                          : return 8;
		case VK_FORMAT_R8_SRGB                                          : return 8;
		case VK_FORMAT_R8G8_UNORM                                       : return 16;
		case VK_FORMAT_R8G8_SNORM                                       : return 16;
		case VK_FORMAT_R8G8_USCALED                                     : return 16;
		case VK_FORMAT_R8G8_SSCALED                                     : return 16;
		case VK_FORMAT_R8G8_UINT                                        : return 16;
		case VK_FORMAT_R8G8_SINT                                        : return 16;
		case VK_FORMAT_R8G8_SRGB                                        : return 16;
		case VK_FORMAT_R8G8B8_UNORM                                     : return 24;
		case VK_FORMAT_R8G8B8_SNORM                                     : return 24;
		case VK_FORMAT_R8G8B8_USCALED                                   : return 24;
		case VK_FORMAT_R8G8B8_SSCALED                                   : return 24;
		case VK_FORMAT_R8G8B8_UINT                                      : return 24;
		case VK_FORMAT_R8G8B8_SINT                                      : return 24;
		case VK_FORMAT_R8G8B8_SRGB                                      : return 24;
		case VK_FORMAT_B8G8R8_UNORM                                     : return 24;
		case VK_FORMAT_B8G8R8_SNORM                                     : return 24;
		case VK_FORMAT_B8G8R8_USCALED                                   : return 24;
		case VK_FORMAT_B8G8R8_SSCALED                                   : return 24;
		case VK_FORMAT_B8G8R8_UINT                                      : return 24;
		case VK_FORMAT_B8G8R8_SINT                                      : return 24;
		case VK_FORMAT_B8G8R8_SRGB                                      : return 24;
		case VK_FORMAT_R8G8B8A8_UNORM                                   : return 32;
		case VK_FORMAT_R8G8B8A8_SNORM                                   : return 32;
		case VK_FORMAT_R8G8B8A8_USCALED                                 : return 32;
		case VK_FORMAT_R8G8B8A8_SSCALED                                 : return 32;
		case VK_FORMAT_R8G8B8A8_UINT                                    : return 32;
		case VK_FORMAT_R8G8B8A8_SINT                                    : return 32;
		case VK_FORMAT_R8G8B8A8_SRGB                                    : return 32;
		case VK_FORMAT_B8G8R8A8_UNORM                                   : return 32;
		case VK_FORMAT_B8G8R8A8_SNORM                                   : return 32;
		case VK_FORMAT_B8G8R8A8_USCALED                                 : return 32;
		case VK_FORMAT_B8G8R8A8_SSCALED                                 : return 32;
		case VK_FORMAT_B8G8R8A8_UINT                                    : return 32;
		case VK_FORMAT_B8G8R8A8_SINT                                    : return 32;
		case VK_FORMAT_B8G8R8A8_SRGB                                    : return 32;
		case VK_FORMAT_A8B8G8R8_UNORM_PACK32                            : return 32;
		case VK_FORMAT_A8B8G8R8_SNORM_PACK32                            : return 32;
		case VK_FORMAT_A8B8G8R8_USCALED_PACK32                          : return 32;
		case VK_FORMAT_A8B8G8R8_SSCALED_PACK32                          : return 32;
		case VK_FORMAT_A8B8G8R8_UINT_PACK32                             : return 32;
		case VK_FORMAT_A8B8G8R8_SINT_PACK32                             : return 32;
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32                             : return 32;
		case VK_FORMAT_A2R10G10B10_UNORM_PACK32                         : return 32;
		case VK_FORMAT_A2R10G10B10_SNORM_PACK32                         : return 32;
		case VK_FORMAT_A2R10G10B10_USCALED_PACK32                       : return 32;
		case VK_FORMAT_A2R10G10B10_SSCALED_PACK32                       : return 32;
		case VK_FORMAT_A2R10G10B10_UINT_PACK32                          : return 32;
		case VK_FORMAT_A2R10G10B10_SINT_PACK32                          : return 32;
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32                         : return 32;
		case VK_FORMAT_A2B10G10R10_SNORM_PACK32                         : return 32;
		case VK_FORMAT_A2B10G10R10_USCALED_PACK32                       : return 32;
		case VK_FORMAT_A2B10G10R10_SSCALED_PACK32                       : return 32;
		case VK_FORMAT_A2B10G10R10_UINT_PACK32                          : return 32;
		case VK_FORMAT_A2B10G10R10_SINT_PACK32                          : return 32;
		case VK_FORMAT_R16_UNORM                                        : return 16;
		case VK_FORMAT_R16_SNORM                                        : return 16;
		case VK_FORMAT_R16_USCALED                                      : return 16;
		case VK_FORMAT_R16_SSCALED                                      : return 16;
		case VK_FORMAT_R16_UINT                                         : return 16;
		case VK_FORMAT_R16_SINT                                         : return 16;
		case VK_FORMAT_R16_SFLOAT                                       : return 16;
		case VK_FORMAT_R16G16_UNORM                                     : return 16;
		case VK_FORMAT_R16G16_SNORM                                     : return 16;
		case VK_FORMAT_R16G16_USCALED                                   : return 16;
		case VK_FORMAT_R16G16_SSCALED                                   : return 16;
		case VK_FORMAT_R16G16_UINT                                      : return 16;
		case VK_FORMAT_R16G16_SINT                                      : return 16;
		case VK_FORMAT_R16G16_SFLOAT                                    : return 16;
		case VK_FORMAT_R16G16B16_UNORM                                  : return 48;
		case VK_FORMAT_R16G16B16_SNORM                                  : return 48;
		case VK_FORMAT_R16G16B16_USCALED                                : return 48;
		case VK_FORMAT_R16G16B16_SSCALED                                : return 48;
		case VK_FORMAT_R16G16B16_UINT                                   : return 48;
		case VK_FORMAT_R16G16B16_SINT                                   : return 48;
		case VK_FORMAT_R16G16B16_SFLOAT                                 : return 48;
		case VK_FORMAT_R16G16B16A16_UNORM                               : return 64;
		case VK_FORMAT_R16G16B16A16_SNORM                               : return 64;
		case VK_FORMAT_R16G16B16A16_USCALED                             : return 64;
		case VK_FORMAT_R16G16B16A16_SSCALED                             : return 64;
		case VK_FORMAT_R16G16B16A16_UINT                                : return 64;
		case VK_FORMAT_R16G16B16A16_SINT                                : return 64;
		case VK_FORMAT_R16G16B16A16_SFLOAT                              : return 64;
		case VK_FORMAT_R32_UINT                                         : return 32;
		case VK_FORMAT_R32_SINT                                         : return 32;
		case VK_FORMAT_R32_SFLOAT                                       : return 32;
		case VK_FORMAT_R32G32_UINT                                      : return 64;
		case VK_FORMAT_R32G32_SINT                                      : return 64;
		case VK_FORMAT_R32G32_SFLOAT                                    : return 64;
		case VK_FORMAT_R32G32B32_UINT                                   : return 96;
		case VK_FORMAT_R32G32B32_SINT                                   : return 96;
		case VK_FORMAT_R32G32B32_SFLOAT                                 : return 96;
		case VK_FORMAT_R32G32B32A32_UINT                                : return 128;
		case VK_FORMAT_R32G32B32A32_SINT                                : return 128;
		case VK_FORMAT_R32G32B32A32_SFLOAT                              : return 128;
		case VK_FORMAT_R64_UINT                                         : return 64;
		case VK_FORMAT_R64_SINT                                         : return 64;
		case VK_FORMAT_R64_SFLOAT                                       : return 64;
		case VK_FORMAT_R64G64_UINT                                      : return 128;
		case VK_FORMAT_R64G64_SINT                                      : return 128;
		case VK_FORMAT_R64G64_SFLOAT                                    : return 128;
		case VK_FORMAT_R64G64B64_UINT                                   : return 192;
		case VK_FORMAT_R64G64B64_SINT                                   : return 192;
		case VK_FORMAT_R64G64B64_SFLOAT                                 : return 192;
		case VK_FORMAT_R64G64B64A64_UINT                                : return 256;
		case VK_FORMAT_R64G64B64A64_SINT                                : return 256;
		case VK_FORMAT_R64G64B64A64_SFLOAT                              : return 256;
		case VK_FORMAT_B10G11R11_UFLOAT_PACK32                          : return 32;
			// I might comment this section out.
			/*
		case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32                           : return 32;
		case VK_FORMAT_D16_UNORM                                        : return 16;
		case VK_FORMAT_X8_D24_UNORM_PACK32                              : return 32;
		case VK_FORMAT_D32_SFLOAT                                       : return 32;
		case VK_FORMAT_S8_UINT                                          : return 8;
		case VK_FORMAT_D16_UNORM_S8_UINT                                : return 24;
		case VK_FORMAT_D24_UNORM_S8_UINT                                : return 32;
		case VK_FORMAT_D32_SFLOAT_S8_UINT                               : return 0;
		case VK_FORMAT_BC1_RGB_UNORM_BLOCK                              : return 0;
		case VK_FORMAT_BC1_RGB_SRGB_BLOCK                               : return 0;
		case VK_FORMAT_BC1_RGBA_UNORM_BLOCK                             : return 0;
		case VK_FORMAT_BC1_RGBA_SRGB_BLOCK                              : return 0;
		case VK_FORMAT_BC2_UNORM_BLOCK                                  : return 0;
		case VK_FORMAT_BC2_SRGB_BLOCK                                   : return 0;
		case VK_FORMAT_BC3_UNORM_BLOCK                                  : return 0;
		case VK_FORMAT_BC3_SRGB_BLOCK                                   : return 0;
		case VK_FORMAT_BC4_UNORM_BLOCK                                  : return 0;
		case VK_FORMAT_BC4_SNORM_BLOCK                                  : return 0;
		case VK_FORMAT_BC5_UNORM_BLOCK                                  : return 0;
		case VK_FORMAT_BC5_SNORM_BLOCK                                  : return 0;
		case VK_FORMAT_BC6H_UFLOAT_BLOCK                                : return 0;
		case VK_FORMAT_BC6H_SFLOAT_BLOCK                                : return 0;
		case VK_FORMAT_BC7_UNORM_BLOCK                                  : return 0;
		case VK_FORMAT_BC7_SRGB_BLOCK                                   : return 0;
		case VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK                          : return 0;
		case VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK                           : return 0;
		case VK_FORMAT_ETC2_R8G8B8A1_UNORM_BLOCK                        : return 0;
		case VK_FORMAT_ETC2_R8G8B8A1_SRGB_BLOCK                         : return 0;
		case VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK                        : return 0;
		case VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK                         : return 0;
		case VK_FORMAT_EAC_R11_UNORM_BLOCK                              : return 0;
		case VK_FORMAT_EAC_R11_SNORM_BLOCK                              : return 0;
		case VK_FORMAT_EAC_R11G11_UNORM_BLOCK                           : return 0;
		case VK_FORMAT_EAC_R11G11_SNORM_BLOCK                           : return 0;
		case VK_FORMAT_ASTC_4x4_UNORM_BLOCK                             : return 0;
		case VK_FORMAT_ASTC_4x4_SRGB_BLOCK                              : return 0;
		case VK_FORMAT_ASTC_5x4_UNORM_BLOCK                             : return 0;
		case VK_FORMAT_ASTC_5x4_SRGB_BLOCK                              : return 0;
		case VK_FORMAT_ASTC_5x5_UNORM_BLOCK                             : return 0;
		case VK_FORMAT_ASTC_5x5_SRGB_BLOCK                              : return 0;
		case VK_FORMAT_ASTC_6x5_UNORM_BLOCK                             : return 0;
		case VK_FORMAT_ASTC_6x5_SRGB_BLOCK                              : return 0;
		case VK_FORMAT_ASTC_6x6_UNORM_BLOCK                             : return 0;
		case VK_FORMAT_ASTC_6x6_SRGB_BLOCK                              : return 0;
		case VK_FORMAT_ASTC_8x5_UNORM_BLOCK                             : return 0;
		case VK_FORMAT_ASTC_8x5_SRGB_BLOCK                              : return 0;
		case VK_FORMAT_ASTC_8x6_UNORM_BLOCK                             : return 0;
		case VK_FORMAT_ASTC_8x6_SRGB_BLOCK                              : return 0;
		case VK_FORMAT_ASTC_8x8_UNORM_BLOCK                             : return 0;
		case VK_FORMAT_ASTC_8x8_SRGB_BLOCK                              : return 0;
		case VK_FORMAT_ASTC_10x5_UNORM_BLOCK                            : return 0;
		case VK_FORMAT_ASTC_10x5_SRGB_BLOCK                             : return 0;
		case VK_FORMAT_ASTC_10x6_UNORM_BLOCK                            : return 0;
		case VK_FORMAT_ASTC_10x6_SRGB_BLOCK                             : return 0;
		case VK_FORMAT_ASTC_10x8_UNORM_BLOCK                            : return 0;
		case VK_FORMAT_ASTC_10x8_SRGB_BLOCK                             : return 0;
		case VK_FORMAT_ASTC_10x10_UNORM_BLOCK                           : return 0;
		case VK_FORMAT_ASTC_10x10_SRGB_BLOCK                            : return 0;
		case VK_FORMAT_ASTC_12x10_UNORM_BLOCK                           : return 0;
		case VK_FORMAT_ASTC_12x10_SRGB_BLOCK                            : return 0;
		case VK_FORMAT_ASTC_12x12_UNORM_BLOCK                           : return 0;
		case VK_FORMAT_ASTC_12x12_SRGB_BLOCK                            : return 0;
		case VK_FORMAT_G8B8G8R8_422_UNORM                               : return 0;
		case VK_FORMAT_B8G8R8G8_422_UNORM                               : return 0;
		case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM                        : return 0;
		case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM                         : return 0;
		case VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM                        : return 0;
		case VK_FORMAT_G8_B8R8_2PLANE_422_UNORM                         : return 0;
		case VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM                        : return 0;
		case VK_FORMAT_R10X6_UNORM_PACK16                               : return 0;
		case VK_FORMAT_R10X6G10X6_UNORM_2PACK16                         : return 0;
		case VK_FORMAT_R10X6G10X6B10X6A10X6_UNORM_4PACK16               : return 0;
		case VK_FORMAT_G10X6B10X6G10X6R10X6_422_UNORM_4PACK16           : return 0;
		case VK_FORMAT_B10X6G10X6R10X6G10X6_422_UNORM_4PACK16           : return 0;
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_420_UNORM_3PACK16       : return 0;
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16        : return 0;
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_422_UNORM_3PACK16       : return 0;
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16        : return 0;
		case VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16       : return 0;
		case VK_FORMAT_R12X4_UNORM_PACK16                               : return 0;
		case VK_FORMAT_R12X4G12X4_UNORM_2PACK16                         : return 0;
		case VK_FORMAT_R12X4G12X4B12X4A12X4_UNORM_4PACK16               : return 0;
		case VK_FORMAT_G12X4B12X4G12X4R12X4_422_UNORM_4PACK16           : return 0;
		case VK_FORMAT_B12X4G12X4R12X4G12X4_422_UNORM_4PACK16           : return 0;
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_420_UNORM_3PACK16       : return 0;
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16        : return 0;
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_422_UNORM_3PACK16       : return 0;
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_422_UNORM_3PACK16        : return 0;
		case VK_FORMAT_G12X4_B12X4_R12X4_3PLANE_444_UNORM_3PACK16       : return 0;
		case VK_FORMAT_G16B16G16R16_422_UNORM                           : return 0;
		case VK_FORMAT_B16G16R16G16_422_UNORM                           : return 0;
		case VK_FORMAT_G16_B16_R16_3PLANE_420_UNORM                     : return 0;
		case VK_FORMAT_G16_B16R16_2PLANE_420_UNORM                      : return 0;
		case VK_FORMAT_G16_B16_R16_3PLANE_422_UNORM                     : return 0;
		case VK_FORMAT_G16_B16R16_2PLANE_422_UNORM                      : return 0;
		case VK_FORMAT_G16_B16_R16_3PLANE_444_UNORM                     : return 0;
		case VK_FORMAT_PVRTC1_2BPP_UNORM_BLOCK_IMG                      : return 0;
		case VK_FORMAT_PVRTC1_4BPP_UNORM_BLOCK_IMG                      : return 0;
		case VK_FORMAT_PVRTC2_2BPP_UNORM_BLOCK_IMG                      : return 0;
		case VK_FORMAT_PVRTC2_4BPP_UNORM_BLOCK_IMG                      : return 0;
		case VK_FORMAT_PVRTC1_2BPP_SRGB_BLOCK_IMG                       : return 0;
		case VK_FORMAT_PVRTC1_4BPP_SRGB_BLOCK_IMG                       : return 0;
		case VK_FORMAT_PVRTC2_2BPP_SRGB_BLOCK_IMG                       : return 0;
		case VK_FORMAT_PVRTC2_4BPP_SRGB_BLOCK_IMG                       : return 0;
		case VK_FORMAT_ASTC_4x4_SFLOAT_BLOCK_EXT                        : return 0;
		case VK_FORMAT_ASTC_5x4_SFLOAT_BLOCK_EXT                        : return 0;
		case VK_FORMAT_ASTC_5x5_SFLOAT_BLOCK_EXT                        : return 0;
		case VK_FORMAT_ASTC_6x5_SFLOAT_BLOCK_EXT                        : return 0;
		case VK_FORMAT_ASTC_6x6_SFLOAT_BLOCK_EXT                        : return 0;
		case VK_FORMAT_ASTC_8x5_SFLOAT_BLOCK_EXT                        : return 0;
		case VK_FORMAT_ASTC_8x6_SFLOAT_BLOCK_EXT                        : return 0;
		case VK_FORMAT_ASTC_8x8_SFLOAT_BLOCK_EXT                        : return 0;
		case VK_FORMAT_ASTC_10x5_SFLOAT_BLOCK_EXT                       : return 0;
		case VK_FORMAT_ASTC_10x6_SFLOAT_BLOCK_EXT                       : return 0;
		case VK_FORMAT_ASTC_10x8_SFLOAT_BLOCK_EXT                       : return 0;
		case VK_FORMAT_ASTC_10x10_SFLOAT_BLOCK_EXT                      : return 0;
		case VK_FORMAT_ASTC_12x10_SFLOAT_BLOCK_EXT                      : return 0;
		case VK_FORMAT_ASTC_12x12_SFLOAT_BLOCK_EXT                      : return 0;
		case VK_FORMAT_G8_B8R8_2PLANE_444_UNORM_EXT                     : return 0;
		case VK_FORMAT_G10X6_B10X6R10X6_2PLANE_444_UNORM_3PACK16_EXT    : return 0;
		case VK_FORMAT_G12X4_B12X4R12X4_2PLANE_444_UNORM_3PACK16_EXT    : return 0;
		case VK_FORMAT_G16_B16R16_2PLANE_444_UNORM_EXT                  : return 0;
		case VK_FORMAT_A4R4G4B4_UNORM_PACK16_EXT                        : return 0;
		case VK_FORMAT_A4B4G4R4_UNORM_PACK16_EXT                        : return 0;
			*/
		}
	}

}
