./flatc --cpp --scoped-enums --gen-object-api --gen-compare --gen-name-strings --cpp-ptr-type "std::unique_ptr" --cpp-str-type "dv::cstring" --cpp-str-flex-ctor --cpp-vec-type "dv::cvector" --reflect-types --reflect-names "${1}".fbs
mv `basename "${1}"`_generated.h "${1}"_base.hpp
clang-format -i "${1}"_base.hpp
