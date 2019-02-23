flatc --cpp --scoped-enums --gen-object-api --reflect-types --reflect-names "${1}".fbs
mv "${1}"_generated.h "${1}".h
clang-format -i "${1}".h
