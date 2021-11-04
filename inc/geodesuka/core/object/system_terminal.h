#pragma once
#ifndef GEODESUKA_CORE_OBJECT_SYSTEM_TERMINAL_H
#define GEODESUKA_CORE_OBJECT_SYSTEM_TERMINAL_H

#include "../util/text.h"

#include "../gcl/context.h"

#include "../object.h"

/*
* This class may be really dumb and retarded,
* but I am importing some of my code from my
* mbed rc car code to handle terminal input.
* 
*/

namespace geodesuka::core::object {

	class system_terminal : public object_t {
	public:

		friend class engine;

		~system_terminal();

		// Returns true if input is gathered. Returns false if empty line.
		bool operator>>(util::text &aRhs);

		// Prints output to terminal.
		bool operator<<(util::text& aRhs);

		// Same as above, but good for string literals.
		bool operator<<(const char* aRhs);

	protected:

	private:

		// Only engine can create this.
		system_terminal(engine* aEngine, gcl::context* aContext);

	};

}

#endif // !GEODESUKA_CORE_OBJECT_SYSTEM_TERMINAL_H