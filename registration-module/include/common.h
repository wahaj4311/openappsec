#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <assert.h>
#include "messaging_enums.h"

#define dbgError(x) std::cerr
#define dbgWarning(x) std::cerr
#define dbgTrace(x) std::cout
#define dbgInfo(x) std::cout
#define dbgAssert(x) do { if (!(x)) assert(x); } while(0)

// Adding the stream operator for HTTPStatusCode
inline std::ostream &
operator<<(std::ostream &os, const HTTPStatusCode &status)
{
    // Cast to underlying type for printing
    using UnderlyingType = std::underlying_type_t<HTTPStatusCode>;
    return os << static_cast<UnderlyingType>(status);
}

#endif // COMMON_H 