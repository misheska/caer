#ifndef CSTRING_HPP
#define CSTRING_HPP

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <type_traits>

namespace dv {

class cstring {

};

} // namespace dv

static_assert(std::is_standard_layout_v<dv::cstring>, "cstring is not standard layout");

#endif // CSTRING_HPP
