#pragma once
#ifndef GEODESUKA_CORE_GCL_COMMAND_BATCH_H
#define GEODESUKA_CORE_GCL_COMMAND_BATCH_H

#include <vulkan/vulkan.h>

namespace geodesuka::core::gcl {

	/*
	* Simply a utility to manage batched queue submissions for execution.
	*/
	class command_batch {
	public:

		friend class engine;
		friend class context;

		command_batch();
		command_batch(VkSubmitInfo aSubmission);
		command_batch(size_t aSubmissionCount, VkSubmitInfo* aSubmission);
		command_batch(VkPresentInfoKHR aPresentation);
		command_batch(size_t aPresentationCount, VkPresentInfoKHR* aPresentation);
		command_batch(const command_batch& aInput);
		command_batch(command_batch&& aInput) noexcept;
		~command_batch();

		//VkSubmitInfo operator[](int aIndex) const;
		//VkSubmitInfo& operator[](int aIndex);

		command_batch& operator=(const command_batch& aRhs);
		command_batch& operator=(command_batch&& aRhs) noexcept;

		// Will be used to aggregate render_target submissions.
		command_batch& operator+=(VkSubmitInfo aRhs);
		command_batch& operator+=(VkPresentInfoKHR aRhs);
		command_batch& operator+=(const command_batch& aRhs);

		void clear();

	private:
		size_t SubmissionCount;
		VkSubmitInfo* Submission;

		size_t PresentationCount;
		VkPresentInfoKHR* Presentation;
	};

}

#endif // !GEODESUKA_CORE_GCL_COMMAND_BATCH_H
