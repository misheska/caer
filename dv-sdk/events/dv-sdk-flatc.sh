flatc --cpp --scoped-enums --gen-object-api --cpp-vec-type "dv::cvector" --reflect-types --reflect-names "${1}".fbs
mv "${1}"_generated.h "${1}".hpp
clang-format -i "${1}".hpp
