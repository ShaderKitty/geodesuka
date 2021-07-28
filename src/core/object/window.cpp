#include <geodesuka/core/object/window.h>

// Standard C Library.
#include <stdlib.h>
#include <string.h>

#include <geodesuka/core/math/gmath.h>

//#include "../hid/mouse.h"
//#include "../hid/keyboard.h"
//#include "../hid/joystick.h"

#include <geodesuka/core/object/object.h>

namespace geodesuka {
	namespace core {
		namespace object {

			window::prop::prop() {
				this->Resizable				= GLFW_TRUE;
				this->Decorated				= GLFW_FALSE;
				this->UserFocused			= GLFW_TRUE;
				this->AutoMinimize			= GLFW_TRUE;
				this->Floating				= GLFW_FALSE;
				this->Maximized				= GLFW_FALSE;
				this->Minimized				= GLFW_FALSE;
				this->Visible				= GLFW_TRUE;
				this->ScaleToMonitor		= GLFW_FALSE;
				this->CenterCursor			= GLFW_TRUE;
				this->FocusOnShow			= GLFW_TRUE;
				this->Hovered				= GLFW_TRUE;
				this->RefreshRate			= GLFW_DONT_CARE;
			}

		}
	}
}