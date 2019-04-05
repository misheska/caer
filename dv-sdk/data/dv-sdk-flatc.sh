./flatc --cpp --scoped-enums --gen-object-api --cpp-ptr-type "std::unique_ptr" --cpp-str-type "dv::cstring" --cpp-str-flex-ctor --cpp-vec-type "dv::cvector" --reflect-types --reflect-names "${1}".fbs
mv "${1}"_generated.h "${1}".hpp
clang-format -i "${1}".hpp
