#include <geodesuka/core/math/numbers/constants.h>

#include <geodesuka/core/math/config.h>

#include <geodesuka/core/math/numbers/natural.h>
#include <geodesuka/core/math/numbers/integer.h>
#include <geodesuka/core/math/numbers/real.h>

namespace geodesuka {
	namespace core {
		namespace math {
#ifdef REAL_TYPE_FLOAT
			const real constant::pi = 3.1415926536f;
			const real constant::e = 2.7182818284f;
#endif // REAL_TYPE_FLOAT

#ifdef REAL_TYPE_DOUBLE
			const real constant::pi = 3.14159265358979323846;
			const real constant::e = 2.71828182845904523536;
#endif // REAL_TYPE_DOUBLE

		}
	}
}