#include <geodesuka/builtin/stage/example.h>

#include <geodesuka/engine.h>

namespace geodesuka::builtin::stage {

	example::example(engine* aEngine, core::gcl::context* aContext) : stage_t(aEngine, aContext) {
		
		Property.Window.Resizable			= true;
		Property.Window.Decorated			= true;
		Property.Window.UserFocused			= true;
		Property.Window.AutoMinimize		= false;
		Property.Window.Floating			= false;
		Property.Window.Maximized			= false;
		Property.Window.Minimized			= false;
		Property.Window.Visible				= true;
		Property.Window.ScaleToMonitor		= false;
		Property.Window.CenterCursor		= false;
		Property.Window.FocusOnShow			= false;

		Property.Swapchain.FrameCount		= 3;
		Property.Swapchain.FrameRate		= 1.0;
		Property.Swapchain.ColorSpace		= core::object::system_window::colorspace::EXTENDED_SRGB_LINEAR;
		Property.Swapchain.Usage			= core::gcl::image::usage::COLOR_ATTACHMENT;
		Property.Swapchain.CompositeAlpha	= core::object::system_window::composite::ALPHA_OPAQUE;
		Property.Swapchain.PresentMode		= core::object::system_window::present_mode::MAILBOX; // This is not supported on my hardware, but other options don't work either?
		Property.Swapchain.Clipped			= true;
		Property.PixelFormat				= VK_FORMAT_R16G16B16A16_SFLOAT;
		Property.Position					= float3(0, 0, 0);
		Property.Size						= float2(0.3, 0.3);
		Property.Title						= "Triangle";

		SystemDisplay = Engine->get_primary_display();

		SystemWindow = new core::object::system_window(Engine, Context, SystemDisplay, Property);

		RenderTarget.push_back(SystemWindow);

		Triangle = new object::triangle(aEngine, aContext, this);

		Object.push_back(Triangle);

		// Will generate all draw commands per object-rendertarget pair.
		this->generate_renderops();
		// Declare that instance is ready to be processed by engine.
		isReadyToBeProcessed.store(true);
	}

	example::~example() {
		isReadyToBeProcessed.store(false);
		delete Triangle;
		delete SystemWindow;
	}



}