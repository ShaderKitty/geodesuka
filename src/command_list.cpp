#include <geodesuka/core/gcl/command_list.h>

#include <cstdlib>
#include <cstring>

#include <vulkan/vulkan.h>

namespace geodesuka::core::gcl {

	command_list::command_list() {
		n = 0;
		ptr = NULL;
	}

	command_list::command_list(VkCommandBuffer aCommandBuffer) {
		n = 0;
		ptr = (VkCommandBuffer*)malloc(sizeof(VkCommandBuffer));
		if (ptr != NULL) {
			ptr[0] = aCommandBuffer;
			n = 1;
		}
	}

	command_list::command_list(uint32_t aCommandBufferCount, VkCommandBuffer* aCommandBufferList) {
		n = 0;
		ptr = NULL;
		if ((aCommandBufferCount > 0) && (aCommandBufferList != NULL)) {
			ptr = (VkCommandBuffer*)malloc(aCommandBufferCount * sizeof(VkCommandBuffer));
			if (ptr != NULL) {
				memcpy(ptr, aCommandBufferList, aCommandBufferCount * sizeof(VkCommandBuffer));
				n = aCommandBufferCount;
			}
			else {
				n = 0;
			}
		}
	}

	command_list::command_list(const command_list& aCommandList) {
		n = 0;
		ptr = NULL;
		if ((aCommandList.n > 0) && (aCommandList.ptr != NULL)) {
			ptr = (VkCommandBuffer*)malloc(aCommandList.n * sizeof(VkCommandBuffer));
			if (ptr != NULL) {
				memcpy(ptr, aCommandList.ptr, aCommandList.n * sizeof(VkCommandBuffer));
				n = aCommandList.n;
			}
			else {
				n = 0;
			}
		}
	}

	command_list::command_list(command_list&& aCommandList) noexcept {
		n = aCommandList.n;
		ptr = aCommandList.ptr;
		aCommandList.n = 0;
		aCommandList.ptr = NULL;
	}

	command_list::~command_list() {
		n = 0;
		free(ptr);
		ptr = NULL;
	}

	command_list& command_list::operator=(const command_list& aRhs) {
		n = 0;
		free(ptr);
		ptr = NULL;

		void* nptr = NULL;
		if (ptr == NULL) {
			nptr = malloc(aRhs.n * sizeof(VkCommandBuffer));
		}
		else if (n != aRhs.n) {
			nptr = realloc(ptr, aRhs.n * sizeof(VkCommandBuffer));
		}
		else {
			nptr = ptr;
		}

		if (nptr == NULL) return *this;
		if (nptr != (void*)ptr) ptr = (VkCommandBuffer*)nptr;
		n = aRhs.n;

		memcpy(ptr, aRhs.ptr, aRhs.n * sizeof(VkCommandBuffer));

		return *this;
	}

	command_list& command_list::operator=(command_list&& aRhs) noexcept {
		free(ptr);
		n = aRhs.n;
		ptr = aRhs.ptr;
		aRhs.n = 0;
		aRhs.ptr = NULL;
		return *this;
	}

	VkCommandBuffer& command_list::operator[](uint32_t aIndex) {
		return ptr[aIndex];
	}

	command_list& command_list::operator+=(VkCommandBuffer aRhs) {
		// // O: insert return statement here
		*this += command_list(aRhs);
		return *this;
	}

	command_list& command_list::operator+=(const command_list& aRhs) {
		// // O: insert return statement here
		void* nptr = NULL;
		if (ptr == NULL) {
			nptr = malloc(aRhs.n * sizeof(VkCommandBuffer));
		}
		else if (n != aRhs.n) {
			nptr = realloc(ptr, aRhs.n * sizeof(VkCommandBuffer));
		}
		else {
			nptr = ptr;
		}

		if (nptr == NULL) return *this;
		if (nptr != (void*)ptr) ptr = (VkCommandBuffer*)nptr;

		memcpy(&ptr[n], aRhs.ptr, aRhs.n * sizeof(VkCommandBuffer));
		n = aRhs.n;

		return *this;
	}

}
