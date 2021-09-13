#pragma once
#ifndef GEODESUKA_CORE_IO_DYNALIB_H
#define GEODESUKA_CORE_IO_DYNALIB_H

/*
* This will be for loading in runtime libraries that
* utilize the engine. The idea is to allow for loadable
* run time mods through dynamically loaded libraries.
* Most likely to extend object.h
*/

#include "../util/text.h"

#include "file.h"

namespace geodesuka {
	namespace core {
		namespace io {

			class dynalib : public file {
			public:

				dynalib(const char* aLibraryPath);

				~dynalib();

				void* get_pfn(const char* aName);

			//private:

				void* Handle;

				bool mloadlib(const char* aLibraryPath);
				//bool mfreelib(void* aLibraryHandle);

			};

		}
	}
}

#endif // !GEODESUKA_CORE_IO_DYNALIB_H