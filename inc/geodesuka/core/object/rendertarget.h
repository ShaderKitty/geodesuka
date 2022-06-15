/// <summary>
/// rendertarget is an extendable class which the engine user can use to create custom
/// rendertargets for whatever the user may need. For something to be considered a render
/// target, it must have frame attachments which can be used as targets for rendering commands
/// by derived object classes.
/// </summary>

#pragma once
#ifndef GEODESUKA_CORE_OBJECT_RENDERTARGET_H
#define GEODESUKA_CORE_OBJECT_RENDERTARGET_H

//#include <vector>

#include "../gcl.h"
#include "../gcl/context.h"
#include "../gcl/image.h"

//#include "../gcl/command_list.h"
#include "../gcl/command_pool.h"
#include "../gcl/command_batch.h"

#include "../logic/timer.h"

#include "../object.h"

namespace geodesuka::core::object {

	class rendertarget : public object_t {
	public:

		friend class engine;
		friend class stage_t;

		uint32_t FrameCount;			// The total number of back buffer frames of the rendertarget.
		double FrameRate;				// The rate at which the frames will be iterated through by the engine per unit of time.
		uint3 Resolution;				// [Pixels] The grid resolution of every frame and frame attachment of the rendertarget.
		
		int FrameAttachmentCount;									// The number of attachments for each frame.
		VkAttachmentDescription* FrameAttachmentDescription;		// The attachment descriptions of each frame.
		VkImageView** FrameAttachment;								// The image view handles of each frame attachment.

		// This is the current frame that is being drawn to. Do not use for reading in same operation.
		uint32_t FrameDrawIndex;

		gcl::command_pool DrawCommandPool;

		~rendertarget();

		// Used for runtime rendertarget discrimination.
		virtual int id() = 0;

		// -------------------- Called By Stage.render() ------------------------- \\

		// Must be called by stage backend.
		virtual gcl::command_batch render(stage_t* aStage);

		// Will acquire next frame index, if semephore is not VK_NULL_HANDLE, 
		// use as wait semaphore for render operations. This really only applies
		// to a system_window.
		virtual void next_frame();

		// Propose a collection of objects (Most likely from a stage), to 
		// draw those objects to the render target. The objects will
		// produce user implemented draw commands to the rendertarget
		// for aggregation and eventual execution by the geodesuka engine.
		virtual VkSubmitInfo draw(size_t aObjectCount, object_t** aObject) = 0;

		// This will present VKPresentInfoKHR. Must use a semaphore to make presentation
		// dependant on rendering operations to complete.
		virtual VkPresentInfoKHR present_frame();

	protected:

		// Should be used only for backend render logic.
		// TODO: Add event based trigger system for GUI libs.
		logic::timer FrameRateTimer;

		// This is used if a rendertarget is resized at runtime and
		// needs to be Rebuilt.
		//bool Rebuild;

		// Used to back store aggregated draw commands.
		uint32_t* AggregatedDrawCommandCount;
		VkCommandBuffer** AggregatedDrawCommandList;

		rendertarget(engine* aEngine, gcl::context* aContext, stage_t* aStage/*, int aFrameCount, double aFrameRate*/);

	};
}

#endif // !GEODESUKA_CORE_OBJECT_RENDERTARGET_H
