#pragma once
#ifndef GEODESUKA_CORE_OBJECT_H
#define GEODESUKA_CORE_OBJECT_H

// ------------------------------ object.h ------------------------------ //
/*
* object.h will serve as a generic object that is extendable for the user
* to implement custom object derivative classes. cube.h is an example of this
* and more examples will be provided. The difference here is that the extended
* classes of object.h will have full control over how they will be rendered to
* the target window.h or camera.h object.
* 
* Since an object will have part of itself associated with memory objects
* of particular GPUs, it is important to recognize that objects exist within
* a common space in which they are rendered. On top of that, a rendering context
* will indicate which graphics capable device it exists on. Maybe figure out
* how to stream line 
*/

#include <mutex>

#include "./math.h"

#include "gcl/context.h"

#include "hid/keyboard.h"
#include "hid/mouse.h"
#include "hid/joystick.h"

namespace geodesuka {

	class engine;

	namespace core {
		namespace object {
			/*
			* These are forward declarations that will be used for render
			* targets. I am not sure if I want to implement a super class
			* render_target.h just yet. Seems pointless since no use.
			*/
			class system_display;
			class system_window;
			class virtual_window;

			class camera2d;
			class camera3d;
		}
	}
}


namespace geodesuka::core {

	class object_t {
	public:

		//friend class engine;

		std::mutex Mutex;
		//std::vector<gcl::command*> DrawCommand;
		
		virtual ~object_t() = default;

		/*
		* If input stream is being forwarded to a particular instance of an object,
		* this method will be called depending on input recieved by the engine. This
		* method is effectively a forwarded callback stream for the particular instance
		* of the object.
		*/
		virtual void input(const hid::keyboard& aKeyboard);

		/*
		* Basically a call back as well, but uses mouse info instead.
		*/
		virtual void input(const hid::mouse& aMouse);

		/*
		* The implementation of this method will allow the object to internally time evolve
		* itself with a provided time step. It will be the responsibility of the engine
		* to insure that the time step remains stable.
		*/
		virtual void update(math::real aDeltaTime);

		/*
		* To insure that how an object is rendered is left up to the implementation
		* of the derivative class of object.h. These will be the primary draw methods
		* for object.h which must be implemented by the derivative class.
		*/

		/*
		* As of right now, there is no method for direct draw of object to
		* a system_display, maybe in further updates this will be extended.
		* Does nothing as of right now. The framebuffer provided will not
		* be displayed anywhere.
		*/
		virtual void draw(object::system_display* aTargetSystemDisplay);

		/*
		* This method implementation must specify how the object in question
		* will be rendered to an operating system_window.h. This will be
		* mostly be used for GUI stuff since direct window draw.
		*/
		virtual void draw(object::system_window* aTargetSystemWindow);

		/*
		* An implementation of this method must define how the object in question
		* will be drawn to a virtual_window.h, which may exist in its own right
		* as an object in 3d space.
		*/
		virtual void draw(object::virtual_window* aTargetVirtualWindow);

		/*
		* This method will be called when the an object has been requested to be
		* drawn to a camera2d object.
		*/
		virtual void draw(object::camera2d* aTargetCamera2D);

		/*
		* Same as above, but with a 3D camera, will provide internals of
		* camera so extended object can decide how it will be drawn.
		*/
		virtual void draw(object::camera3d* aTargetCamera3D);

		// ------------------------- Utilities ------------------------- //

		virtual void set_position(math::real3 aPosition);
		math::real3 get_position() const;

	protected:

		engine* ParentEngine;
		gcl::context* ParentContext;

		math::real3 InputVelocity;
		math::real3 InputForce;

		/*
		* Do not forget, position is in reference to a particular space
		* the object is in. This is up to the user to decide how to
		* interpret and use an objects position.
		*/

		math::real Mass;			// Kilogram		[kg]
		math::real Time;			// Second 		[s]
		math::real3 Position;		// Meter		[m]
		math::real3 Momentum;		//				[kg*m/s]
		math::real3 Force;			// Newton		[kg*m^2/s^2]
		math::real3 DirectionX;		// Right		[Normalized]
		math::real3 DirectionY;		// Up			[Normalized]
		math::real3 DirectionZ;		// Forward		[Normalized]

		//integer WorldID;				// Which world is this object in?
		//integer LevelID;				// Which Level is this object in?
		//integer StageID;				// Which Stage is this object in?
		//integer ID;						// 

		//boolean isStationary;			// Is this object stationary, or is it allowed to move?
		//boolean isDeterministic;		// Does this object have predefined motion?
		//boolean isAnimate;				// Is this object a living creature?
		//boolean isPickup;				// Can an entitiy pick up this object?

		//boolean isCollisionActive;
		//boolean isGraphicalActive;

	};


}

#endif // !GEODESUKA_CORE_OBJECT_H
