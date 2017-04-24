/* Generated SBE (Simple Binary Encoding) message codec */
#ifndef _MKTDATA_MDUPDATEACTION_H_
#define _MKTDATA_MDUPDATEACTION_H_

#if defined(SBE_HAVE_CMATH)
/* cmath needed for std::numeric_limits<double>::quiet_NaN() */
#  include <cmath>
#  define SBE_FLOAT_NAN std::numeric_limits<float>::quiet_NaN()
#  define SBE_DOUBLE_NAN std::numeric_limits<double>::quiet_NaN()
#else
/* math.h needed for NAN */
#  include <math.h>
#  define SBE_FLOAT_NAN NAN
#  define SBE_DOUBLE_NAN NAN
#endif

#if __cplusplus >= 201103L
#  include <cstdint>
#  include <functional>
#  include <string>
#  include <cstring>
#endif

#include <sbe/sbe.h>

using namespace sbe;

namespace mktdata {

class MDUpdateAction
{
public:

    enum Value 
    {
        New = (std::uint8_t)0,
        Change = (std::uint8_t)1,
        Delete = (std::uint8_t)2,
        DeleteThru = (std::uint8_t)3,
        DeleteFrom = (std::uint8_t)4,
        Overlay = (std::uint8_t)5,
        NULL_VALUE = (std::uint8_t)255
    };

    static MDUpdateAction::Value get(const std::uint8_t value)
    {
        switch (value)
        {
            case 0: return New;
            case 1: return Change;
            case 2: return Delete;
            case 3: return DeleteThru;
            case 4: return DeleteFrom;
            case 5: return Overlay;
            case 255: return NULL_VALUE;
        }

        throw std::runtime_error("unknown value for enum MDUpdateAction [E103]");
    }
};
}
#endif
