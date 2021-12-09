#include <geodesuka/engine.h>

/* --------------- Standard C Libraries --------------- */
#include <cstdlib>
#include <cstring>
#include <cassert>

/* --------------- Standard C++ Libraries --------------- */
#include <iostream>

#include <vector>
#include <chrono>

/* --------------- Platform Dependent Libraries --------------- */
#if defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#elif defined(__APPLE__) || defined(MACOSX)

#elif defined(__linux__) && !defined(__ANDROID__)
#include <unistd.h>
#elif defined(__ANDROID__)
#include <unistd.h>
#endif

/* --------------- Third Party Libraries --------------- */

#include <glslang/Public/ShaderLang.h>


namespace geodesuka {

	using namespace core;
	using namespace gcl;
	using namespace object;

	engine::engine(int argc, char* argv[]) {

		this->State = state::CREATION;
		this->isReady = false;
		this->Shutdown.store(false);
		//this->ThreadsLaunched.store(false);
		this->Handle = VK_NULL_HANDLE;

		bool isGLSLANGReady				= false;
		bool isGLFWReady				= false;
		bool isVulkanReady				= false;
		bool isSystemDisplayAvailable	= false;
		bool isGCDeviceAvailable		= false;



		// --------------- Initialization Process --------------- //

		// Init Process:
		// Start glslang
		// Start glfw
		// Start vulkan
		// Query for Monitors (No primary monitor = Start up failure)
		// Query for gcl devices (No discrete gpu = Start up failure)
		//

		this->SystemTerminal = new system_terminal(this, nullptr);

		// (GLSLang)
		isGLSLANGReady = glslang::InitializeProcess();

		// (GLFW) Must be initialized first for OS extensions.
		isGLFWReady = glfwInit();

		// (Vulkan) Load required window extensions.
		if (isGLFWReady) {
			// Validation Layers.
			this->EnabledLayer.push_back("VK_LAYER_KHRONOS_validation");

			// Certain extensions needed for interacting with Operating System window system.
			uint32_t OSExtensionCount = 0;
			const char** OSExtensionList = glfwGetRequiredInstanceExtensions(&OSExtensionCount);

			for (uint32_t i = 0; i < OSExtensionCount; i++) {
				this->RequiredExtension.push_back(OSExtensionList[i]);
			}
			//this->RequiredExtension.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);		

			//for (size_t i = 0; i < this->RequiredExtension.size(); i++) {
			//	std::cout << this->RequiredExtension[i] << std::endl;
			//}

			this->AppInfo.sType							= VkStructureType::VK_STRUCTURE_TYPE_APPLICATION_INFO;
			this->AppInfo.pNext							= NULL;
			this->AppInfo.pApplicationName				= "No Name";
			this->AppInfo.applicationVersion			= VK_MAKE_VERSION(0, 0, 1);
			this->AppInfo.pEngineName					= "Geodesuka Engine";
			this->AppInfo.engineVersion					= VK_MAKE_VERSION(this->Version.Major, this->Version.Minor, this->Version.Patch);
			this->AppInfo.apiVersion					= VK_MAKE_VERSION(1, 2, 0);

			this->CreateInfo.sType						= VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			this->CreateInfo.pNext						= NULL;
			this->CreateInfo.flags						= 0;
			this->CreateInfo.pApplicationInfo			= &this->AppInfo;
			this->CreateInfo.enabledLayerCount			= (uint32_t)this->EnabledLayer.size();
			this->CreateInfo.ppEnabledLayerNames		= this->EnabledLayer.data();
			this->CreateInfo.enabledExtensionCount		= (uint32_t)this->RequiredExtension.size();
			this->CreateInfo.ppEnabledExtensionNames	= this->RequiredExtension.data();

			VkResult Result = vkCreateInstance(&this->CreateInfo, NULL, &this->Handle);
			if (Result == VK_SUCCESS) {
				isVulkanReady = true;
			}
			else {
				isVulkanReady = false;
			}
		}

		if (isGLSLANGReady && isGLFWReady && isVulkanReady) {

			if (glfwGetPrimaryMonitor() != NULL) {
				this->PrimaryDisplay = new system_display(this, nullptr, glfwGetPrimaryMonitor());
				int MonitorCount;
				GLFWmonitor** Monitors = glfwGetMonitors(&MonitorCount);
				for (int i = 0; i < MonitorCount; i++) {
					if (Monitors[i] != glfwGetPrimaryMonitor()) {
						this->Display[i] = new system_display(this, nullptr, Monitors[i]);
					}
					else {
						this->Display[i] = this->PrimaryDisplay;
					}
				}


				isSystemDisplayAvailable = true;
			}

			uint32_t PhysicalDeviceCount = 0;
			vkEnumeratePhysicalDevices(this->Handle, &PhysicalDeviceCount, NULL);
			if (PhysicalDeviceCount > 0) {
				VkPhysicalDevice* PhysicalDevice = (VkPhysicalDevice*)malloc(PhysicalDeviceCount * sizeof(VkPhysicalDevice));
				this->DeviceCount = PhysicalDeviceCount;
				vkEnumeratePhysicalDevices(this->Handle, &PhysicalDeviceCount, PhysicalDevice);
				for (uint32_t i = 0; i < PhysicalDeviceCount; i++) {
					this->Device[i] = new device(this->Handle, PhysicalDevice[i]);
				}
				free(PhysicalDevice);
				isGCDeviceAvailable = true;
			}

		}

		this->FileCount = 0;
		this->File = NULL;

		this->ContextCount = 0;
		memset(this->Context, 0x00, 512 * sizeof(context*));
		memset(this->Workload, 0x00, 512 * sizeof(workload*));

		this->ObjectCount = 1 + this->DisplayCount;
		this->Object = (object_t**)malloc(this->ObjectCount * sizeof(object_t*));
		this->Object[0] = this->SystemTerminal;
		for (int i = 1; i < this->ObjectCount; i++) {
			this->Object[i] = this->Display[i - 1];
		}

		this->StageCount = 0;
		this->Stage = NULL;

		// Store main thread ID.
		this->MainThreadID = std::this_thread::get_id();

		this->SignalCreate.store(false);
		this->WindowCreated.store(false);
		// Window Temp Data?
		this->ReturnWindow = NULL;
		this->DestroyWindow.store(NULL);

		this->isReady = isGLSLANGReady && isGLFWReady && isVulkanReady && isSystemDisplayAvailable && isGCDeviceAvailable;
		this->State = state::READY;

	}

	engine::~engine() {

		this->State = state::DESTRUCTION;

		for (int i = 0; i < this->StageCount; i++) {
			int Index = (this->StageCount - 1) - i;
			delete this->Stage[Index];
		}
		free(this->Stage);
		this->Stage = NULL;
		this->StageCount = 0;

		for (int i = 0; i < this->ObjectCount; i++) {
			int Index = (this->ObjectCount - 1) - i;
			delete this->Object[Index];
		}
		free(this->Object);
		this->Object = NULL;
		this->ObjectCount = 0;

		for (int i = 0; i < this->ContextCount; i++) {
			int Index = (this->ContextCount - 1) - i;
			delete this->Workload[Index];
			delete this->Context[Index];
		}
		this->ContextCount = 0;

		for (int i = 0; i < this->FileCount; i++) {
			int Index = (this->FileCount - 1) - i;
			delete this->File[Index];
		}
		free(this->File);
		this->File = NULL;
		this->FileCount = 0;

		for (int i = 0; i < this->DeviceCount; i++) {
			delete this->Device[i];
		}
		this->DeviceCount = 0;

		this->PrimaryDisplay = nullptr;
		this->SystemTerminal = nullptr;


		vkDestroyInstance(this->Handle, NULL);

		glfwTerminate();

		glslang::FinalizeProcess();
	}

	core::io::file* engine::open(const char* aFilePath) {
		// utilizes singleton.
		core::io::file* lFileHandle = core::io::file::open(aFilePath);
		if (lFileHandle != nullptr) {

		}

		return nullptr;
	}

	void engine::close(core::io::file* aFile) {

	}

	core::gcl::device** engine::get_device_list(int* aListSize) {
		*aListSize = this->DeviceCount;
		return this->Device;
	}

	core::object::system_display** engine::get_display_list(int* aListSize) {
		*aListSize = this->DisplayCount;
		return this->Display;
	}

	core::object::system_display* engine::get_primary_display() {
		return this->PrimaryDisplay;
	}

	VkInstance engine::handle() {
		return this->Handle;
	}

	bool engine::is_ready() {
		return this->isReady;
	}

	engine::version engine::get_version() {
		return this->Version;
	}

	int engine::get_date() {
		return this->Date;
	}

	engine::workload::workload(core::gcl::context* aContext) {
		this->Context = aContext;
		this->TransferFence = VK_NULL_HANDLE;
		this->ComputeFence = VK_NULL_HANDLE;
		this->GraphicsFence = VK_NULL_HANDLE;

		if (this->Context != nullptr) {
			VkFenceCreateInfo CreateInfo{};
			CreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			CreateInfo.pNext = NULL;
			CreateInfo.flags = 0;
			VkResult Result = VkResult::VK_SUCCESS;
			Result = vkCreateFence(this->Context->handle(), &CreateInfo, NULL, &TransferFence);
			Result = vkCreateFence(this->Context->handle(), &CreateInfo, NULL, &ComputeFence);
			Result = vkCreateFence(this->Context->handle(), &CreateInfo, NULL, &GraphicsFence);
			if ((this->TransferFence == VK_NULL_HANDLE) || (this->ComputeFence == VK_NULL_HANDLE) || (this->GraphicsFence == VK_NULL_HANDLE)) {
				if (this->TransferFence != VK_NULL_HANDLE) {
					vkDestroyFence(this->Context->handle(), this->TransferFence, NULL);
					this->TransferFence = VK_NULL_HANDLE;
				}
				if (this->ComputeFence != VK_NULL_HANDLE) {
					vkDestroyFence(this->Context->handle(), this->ComputeFence, NULL);
					this->ComputeFence = VK_NULL_HANDLE;
				}
				if (this->GraphicsFence != VK_NULL_HANDLE) {
					vkDestroyFence(this->Context->handle(), this->GraphicsFence, NULL);
					this->GraphicsFence = VK_NULL_HANDLE;
				}
			}
		}
	}

	engine::workload::~workload() {
		if (this->Context != nullptr) {
			if (this->TransferFence != VK_NULL_HANDLE) {
				vkDestroyFence(this->Context->handle(), this->TransferFence, NULL);
				this->TransferFence = VK_NULL_HANDLE;
			}
			if (this->ComputeFence != VK_NULL_HANDLE) {
				vkDestroyFence(this->Context->handle(), this->ComputeFence, NULL);
				this->ComputeFence = VK_NULL_HANDLE;
			}
			if (this->GraphicsFence != VK_NULL_HANDLE) {
				vkDestroyFence(this->Context->handle(), this->GraphicsFence, NULL);
				this->GraphicsFence = VK_NULL_HANDLE;
			}
		}
		this->Context = nullptr;
	}

	VkResult engine::workload::waitfor(core::gcl::device::qfs aQFS) {
		VkResult temp = VkResult::VK_SUCCESS;
		this->Mutex.lock();
		switch (aQFS) {
		default:
			break;
		case device::qfs::TRANSFER:
			temp = vkWaitForFences(this->Context->handle(), 1, &this->TransferFence, VK_TRUE, UINT64_MAX);
			break;
		case device::qfs::COMPUTE:
			temp = vkWaitForFences(this->Context->handle(), 1, &this->ComputeFence, VK_TRUE, UINT64_MAX);
			break;
		case device::qfs::GRAPHICS:
			temp = vkWaitForFences(this->Context->handle(), 1, &this->GraphicsFence, VK_TRUE, UINT64_MAX);
			break;
		case device::qfs::PRESENT:
			break;
		}
		this->Mutex.unlock();
		return temp;
	}

	VkResult engine::workload::reset(core::gcl::device::qfs aQFS) {
		VkResult temp = VkResult::VK_SUCCESS;
		switch (aQFS) {
		default:
			break;
		case device::qfs::TRANSFER:
			temp = vkResetFences(this->Context->handle(), 1, &this->TransferFence);
			break;
		case device::qfs::COMPUTE:
			temp = vkResetFences(this->Context->handle(), 1, &this->ComputeFence);
			break;
		case device::qfs::GRAPHICS:
			temp = vkResetFences(this->Context->handle(), 1, &this->GraphicsFence);
			break;
		case device::qfs::PRESENT:
			break;
		}
		return temp;
	}

	void engine::submit(core::gcl::context* aContext) {
		if (!((this->State == state::READY) || (this->State == state::RUNNING))) return;
		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(true);
			this->RenderUpdateTrap.wait_until(2);
		}

		this->Context[this->ContextCount] = aContext;
		this->Workload[this->ContextCount] = new workload(aContext);
		this->ContextCount += 1;

		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(false);
		}
	}

	void engine::remove(core::gcl::context* aContext) {
		if (!((this->State == state::READY) || (this->State == state::RUNNING))) return;
		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(true);
			this->RenderUpdateTrap.wait_until(2);
		}

		int CtxIdx = this->ctxidx(aContext);
		int MoveCount = (this->ContextCount - 1) - CtxIdx;

		delete this->Workload[CtxIdx];
		this->Context[CtxIdx] = nullptr;
		this->Workload[CtxIdx] = nullptr;

		memmove(&this->Context[CtxIdx], &this->Context[CtxIdx + 1], MoveCount * sizeof(context*));
		memmove(&this->Workload[CtxIdx], &this->Workload[CtxIdx + 1], MoveCount * sizeof(context*));

		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(false);
		}
	}

	void engine::submit(core::object_t* aObject) {
		if (!((this->State == state::READY) || (this->State == state::RUNNING))) return;
		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(true);
			this->RenderUpdateTrap.wait_until(2);
		}

		void *nptr = NULL;

		if (this->Object == NULL) {
			nptr = malloc(sizeof(object_t*));
		}
		else {
			nptr = realloc(this->Object, (this->ObjectCount + 1) * sizeof(object_t*));
		}

		assert(!(nptr == NULL));

		this->Object = (object_t**)nptr;
		this->Object[this->ObjectCount] = aObject;
		this->ObjectCount += 1;

		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(false);
		}
	}

	void engine::remove(core::object_t* aObject) {
		// Should be fine?
		//if (this->State != state::READY) return;

		if (!((this->State == state::READY) || (this->State == state::RUNNING))) return;
		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(true);
			this->RenderUpdateTrap.wait_until(2);
		}

		int ObjIdx = this->objidx(aObject);
		if (ObjIdx >= 0) {
			if (this->ObjectCount == 1) {
				free(this->Object);
				this->Object = NULL;
				this->ObjectCount = 0;
			}
			else {
				this->Object[ObjIdx] = nullptr;
				int MoveCount = (this->ObjectCount - 1) - ObjIdx;
				if (MoveCount > 0) {
					memmove(&this->Object[ObjIdx], &this->Object[ObjIdx + 1], MoveCount * sizeof(object_t*));
				}
				void* nptr = realloc(this->Object, (this->ObjectCount - 1) * sizeof(object_t*));
				assert(!(nptr == NULL));
				this->Object = (object_t**)nptr;
				this->ObjectCount -= 1;
			}
		}

		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(false);
		}
	}

	void engine::submit(core::object::system_window* aSystemWindow) {
		if (!((this->State == state::READY) || (this->State == state::RUNNING))) return;
		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(true);
			this->RenderUpdateTrap.wait_until(2);
		}

		// Put at end of SystemWindow List.
		int Offset = 1 + this->DisplayCount + this->SystemWindowCount;
		int MoveCount = (this->ObjectCount - 1) - Offset;

		void* nptr = NULL;

		if (this->Object == NULL) {
			nptr = malloc(sizeof(object_t*));
		}
		else {
			nptr = realloc(this->Object, (this->ObjectCount + 1)*sizeof(object_t*));
		}

		// If this dereferences a null pointer, oh well.
		assert(!(nptr == NULL));

		this->Object = (object_t**)nptr;

		this->SystemWindow[this->SystemWindowCount] = aSystemWindow;
		if (MoveCount > 0) {
			memmove(&this->Object[Offset], &this->Object[Offset + 1], MoveCount * sizeof(object_t*));
		}
		this->Object[Offset] = aSystemWindow;

		this->SystemWindowCount += 1;
		this->ObjectCount += 1;

		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(false);
		}
	}

	void engine::remove(core::object::system_window* aSystemWindow) {
		if (!((this->State == state::READY) || (this->State == state::RUNNING))) return;
		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(true);
			this->RenderUpdateTrap.wait_until(2);
		}

		int WinIdx = this->winidx(aSystemWindow);
		if (WinIdx >= 0) {
			int MoveCount = (this->SystemWindowCount - 1) - WinIdx;
			this->SystemWindow[WinIdx] = nullptr;
			if (MoveCount > 0) {
				memmove(&this->SystemWindow[WinIdx], &this->SystemWindow[WinIdx + 1], MoveCount * sizeof(system_window*));
			}
			this->SystemWindow[this->SystemWindowCount - 1] = nullptr;
			this->SystemWindowCount -= 1;
		}

		int ObjIdx = this->objidx(aSystemWindow);
		if (ObjIdx >= 0) {
			int MoveCount = (this->ObjectCount - 1) - ObjIdx;
			this->Object[ObjIdx] = nullptr;
			if (MoveCount > 0) {
				memmove(&this->Object[ObjIdx], &this->Object[ObjIdx + 1], MoveCount * sizeof(object_t*));
			}
			this->Object[this->ObjectCount - 1] = nullptr;
			this->ObjectCount -= 1;
		}

		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(false);
		}
	}

	void engine::submit(core::stage_t* aStage) {
		if (!((this->State == state::READY) || (this->State == state::RUNNING))) return;
		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(true);
			this->RenderUpdateTrap.wait_until(2);
		}

		void *nptr = NULL;
		if (this->Stage == NULL) {
			nptr = malloc(sizeof(stage_t*));
		}
		else {
			nptr = realloc(this->Stage, (this->StageCount + 1) * sizeof(stage_t*));
		}

		assert(!(nptr == NULL));

		this->Stage[this->StageCount] = aStage;
		this->StageCount += 1;

		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(false);
		}
	}

	void engine::remove(core::stage_t* aStage) {
		if (!((this->State == state::READY) || (this->State == state::RUNNING))) return;
		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(true);
			this->RenderUpdateTrap.wait_until(2);
		}

		int StgIdx = this->stgidx(aStage);
		if (StgIdx >= 0) {
			if (this->StageCount == 1) {
				free(this->Stage);
				this->Stage = NULL;
				this->StageCount = 0;
			}
			else {
				int MoveCount = (this->StageCount - 1) - StgIdx;
				if (MoveCount) {
					memmove(&this->Stage[StgIdx], &this->Stage[StgIdx + 1], MoveCount * sizeof(stage_t*));
				}
				void* nptr = realloc(this->Stage, (this->StageCount - 1) * sizeof(stage_t*));
				assert(!(nptr == NULL));
				this->Stage = (stage_t**)nptr;
				this->StageCount -= 1;
			}
		}

		if (this->State == state::RUNNING) {
			this->RenderUpdateTrap.set(false);
		}
	}

		GLFWwindow* engine::create_window_handle(core::object::window::prop aProperty, int aWidth, int aHeight, const char* aTitle, GLFWmonitor* aMonitor, GLFWwindow* aWindow) {
		GLFWwindow* Temp = NULL;
		if (this->MainThreadID == std::this_thread::get_id()) {
			glfwWindowHint(GLFW_RESIZABLE,			aProperty.Resizable);
			glfwWindowHint(GLFW_DECORATED,			aProperty.Decorated);
			glfwWindowHint(GLFW_FOCUSED,			aProperty.UserFocused);
			glfwWindowHint(GLFW_AUTO_ICONIFY,		aProperty.AutoMinimize);
			glfwWindowHint(GLFW_FLOATING,			aProperty.Floating);
			glfwWindowHint(GLFW_MAXIMIZED,			aProperty.Maximized);
			glfwWindowHint(GLFW_VISIBLE,			aProperty.Visible);
			glfwWindowHint(GLFW_SCALE_TO_MONITOR,	aProperty.ScaleToMonitor);
			glfwWindowHint(GLFW_CENTER_CURSOR,		aProperty.CenterCursor);
			glfwWindowHint(GLFW_FOCUS_ON_SHOW,		aProperty.FocusOnShow);
			glfwWindowHint(GLFW_CLIENT_API,			GLFW_NO_API);
			glfwWindowHint(GLFW_REFRESH_RATE,		GLFW_DONT_CARE); // TODO: Change to GLFW_DONT_CARE, and remove option.

			Temp = glfwCreateWindow(aWidth, aHeight, aTitle, aMonitor, aWindow);
		}
		else {
			this->WindowTempData.Property = aProperty;
			this->WindowTempData.Width = aWidth;
			this->WindowTempData.Height = aHeight;
			this->WindowTempData.Title = aTitle;
			this->WindowTempData.Monitor = aMonitor;
			this->WindowTempData.Window = aWindow;

			this->WindowCreated.store(false);
			this->SignalCreate.store(true);
			// Wait for window to be created.
			while (!this->WindowCreated.load()) {}
			Temp = this->ReturnWindow;
		}
		return Temp;
	}

	void engine::destroy_window_handle(GLFWwindow* aWindow) {
		if (this->MainThreadID == std::this_thread::get_id()) {
			glfwDestroyWindow(aWindow);
		}
		else {
			while (this->DestroyWindow.load() != NULL) {}
			this->DestroyWindow.store(aWindow);
		}
	}

	void engine::mtcd_process_window_handle_call() {
		if (this->SignalCreate.load()) {
			glfwWindowHint(GLFW_RESIZABLE,			WindowTempData.Property.Resizable);
			glfwWindowHint(GLFW_DECORATED,			WindowTempData.Property.Decorated);
			glfwWindowHint(GLFW_FOCUSED,			WindowTempData.Property.UserFocused);
			glfwWindowHint(GLFW_AUTO_ICONIFY,		WindowTempData.Property.AutoMinimize);
			glfwWindowHint(GLFW_FLOATING,			WindowTempData.Property.Floating);
			glfwWindowHint(GLFW_MAXIMIZED,			WindowTempData.Property.Maximized);
			glfwWindowHint(GLFW_VISIBLE,			WindowTempData.Property.Visible);
			glfwWindowHint(GLFW_SCALE_TO_MONITOR,	WindowTempData.Property.ScaleToMonitor);
			glfwWindowHint(GLFW_CENTER_CURSOR,		WindowTempData.Property.CenterCursor);
			glfwWindowHint(GLFW_FOCUS_ON_SHOW,		WindowTempData.Property.FocusOnShow);
			glfwWindowHint(GLFW_CLIENT_API,			GLFW_NO_API);
			glfwWindowHint(GLFW_REFRESH_RATE,		GLFW_DONT_CARE); // TODO: Change to GLFW_DONT_CARE, and remove option.

			this->ReturnWindow = glfwCreateWindow(WindowTempData.Width, WindowTempData.Height, WindowTempData.Title, WindowTempData.Monitor, WindowTempData.Window);
			this->WindowCreated.store(true);
			this->SignalCreate.store(false);
		}
		
		// Check if window needs to be destroyed.
		GLFWwindow* temp = this->DestroyWindow.load();
		if (temp != NULL) {
			glfwDestroyWindow(temp);
			this->DestroyWindow.store(NULL);
		}		
	}

	int engine::filidx(core::io::file* aFile) {
		for (int i = 0; i < this->FileCount; i++) {
			if (this->File[i] == aFile) return i;
		}
		return -1;
	}

	int engine::ctxidx(core::gcl::context* aCtx) {
		for (int i = 0; i < this->ContextCount; i++) {
			if (this->Context[i] == aCtx) return i;
		}
		return -1;
	}

	int engine::objidx(core::object_t* aObj) {
		for (int i = 0; i < this->ObjectCount; i++) {
			if (this->Object[i] == aObj) return i;
		}
		return -1;
	}

	int engine::winidx(core::object::system_window* aWin) {
		for (int i = 0; i < this->SystemWindowCount; i++) {
			if (this->SystemWindow[i] == aWin) return i;
		}
		return 0;
	}

	int engine::stgidx(core::stage_t* aStg) {
		for (int i = 0; i < this->StageCount; i++) {
			if (this->Stage[i] == aStg) return i;
		}
		return -1;
	}

	// --------------- Engine Main Thread --------------- //
	// The main thread is used to spawn backend threads along
	// with the app thread.
	// --------------- Engine Main Thread --------------- //
	int engine::run(core::app* aApp) {

		// Store main thread ID.

		this->State = state::RUNNING;
		this->MainThreadID				= std::this_thread::get_id();
		this->RenderThread				= std::thread(&engine::trender, this);
		this->SystemTerminalThread		= std::thread(&engine::tsterminal, this);
		this->AppThread					= std::thread(&core::app::run, aApp);

		//std::cout << "Main Thread ID:   " << std::this_thread::get_id() << std::endl;
		//std::cout << "ST Thread ID:     " << this->SystemTerminalThread.get_id() << std::endl;
		//std::cout << "Render Thread ID: " << this->RenderThread.get_id() << std::endl;
		//std::cout << "App Thread ID:    " << this->AppThread.get_id() << std::endl;
		//this->ThreadsLaunched.store(true);

		double t1, t2;
		double wt, ht;
		double t, dt;
		double ts = 0.01;
		dt = 0.0;

		stage_t::batch TransferBatch[512];
		stage_t::batch ComputeBatch[512];
		VkResult Result = VkResult::VK_SUCCESS;

		// The update thread is the main thread.
		while (!this->Shutdown.load()) {
			this->RenderUpdateTrap.door();

			t1 = core::logic::get_time();
			this->mtcd_process_window_handle_call();
			glfwPollEvents();

			// Poll for Object Updates.
			for (int i = 0; i < this->ObjectCount; i++) {
				int CtxIdx = this->ctxidx(this->Object[i]->Context);
				if (CtxIdx >= 0) {
					this->Workload[CtxIdx]->TransferBatch += this->Object[i]->update(dt);
				}
			}

			// Poll for Stage Updates and 
			for (int i = 0; i < this->StageCount; i++) {
				int CtxIdx = this->ctxidx(this->Stage[i]->Context);
				if (CtxIdx >= 0) {
					this->Workload[CtxIdx]->TransferBatch += this->Stage[i]->update(dt);
				}
			}

			// Wait for all submissions to finish before presentation.
			for (int i = 0; i < this->ContextCount; i++) {
				if (this->Workload[i]->GraphicsBatch.count() > 0) {
					Result = this->Workload[i]->waitfor(device::qfs::GRAPHICS);
				}
				else {

				}
			}

			// Submit all transfer operations.
			for (int i = 0; i < this->ContextCount; i++) {
				if (this->Workload[i]->TransferBatch.count() == 0) continue;
				this->Context[i]->submit(
					device::qfs::TRANSFER, 
					this->Workload[i]->TransferBatch.count(),
					this->Workload[i]->TransferBatch.ptr(),
					this->Workload[i]->TransferFence
				);
			}


			t2 = core::logic::get_time();
			wt = t2 - t1;
			if (wt < ts) {
				ht = ts - wt;
				core::logic::waitfor(ht);
			}
			else {
				ht = 0.0;
			}
			dt = wt + ht;
		}

		// This is redundant. Only the App thread can shutdown all other threads.
		this->Shutdown.store(true);

		this->RenderThread.join();
		this->SystemTerminalThread.join();
		this->AppThread.join();
		this->State = state::READY;

		return 0;
	}

	// --------------- Render Thread --------------- //
	// The job of the render thread is to honor and schedule draw
	// calls of respective render targets stored in memory.
	// --------------- Render Thread --------------- //
	void engine::trender() {

		// A context can create multiple windows, and since
		// the present queue belongs to a specific context,
		// it would be wise to group presentation updates
		std::vector<VkPresentInfoKHR> Presentation;
		VkResult Result = VkResult::VK_SUCCESS;

		while (!this->Shutdown.load()) {
			this->RenderUpdateTrap.door();

			// Aggregate all render operations from stages.
			for (int i = 0; this->StageCount; i++) {
				//int j = (this->StageCount - 1) - i;
				int CtxIdx = this->ctxidx(this->Stage[i]->Context);
				if (CtxIdx >= 0) {
					this->Workload[CtxIdx]->GraphicsBatch += this->Stage[i]->render();
				}
			}

			// Reset fences for work submission.
			for (int i = 0; i < this->ContextCount; i++) {
				if (this->Workload[i]->GraphicsBatch.count() == 0) continue;
				Result = this->Workload[i]->reset(device::qfs::GRAPHICS);
			}

			// Submit all render operations.
			for (int i = 0; i < this->ContextCount; i++) {
				if (this->Workload[i]->GraphicsBatch.count() == 0) continue;
				this->Context[i]->submit(
					device::qfs::GRAPHICS, 
					this->Workload[i]->GraphicsBatch.count(),
					this->Workload[i]->GraphicsBatch.ptr(), 
					this->Workload[i]->GraphicsFence
				);
			}

			// Wait for all submissions to finish before presentation.
			for (int i = 0; i < this->ContextCount; i++) {
				if (this->Workload[i]->GraphicsBatch.count() > 0) {
					Result = vkWaitForFences(this->Context[i]->handle(), 1, &this->Workload[i]->GraphicsFence, VK_TRUE, UINT64_MAX);
				}
			}

			// Present image indices.
			for (int i = 0; i < this->ContextCount; i++) {
				if (this->Workload[i]->Presentation.count() > 0) {

				}
			}


			core::logic::waitfor(0.001);
		}
	}

	void engine::taudio() {
		// Does nothing currently.
		while (!this->Shutdown.load()) {
			core::logic::waitfor(1);
		}
	}

	// --------------- System Terminal Thread --------------- //
	// Will be used for runtime debugging of engine using terminal.
	// --------------- System Terminal Thread --------------- //
	void engine::tsterminal() {

		while (!this->Shutdown.load()) {
			core::logic::waitfor(1.0);
		}
	}

}
