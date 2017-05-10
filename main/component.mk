#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

CFLAGS += -Wno-error=switch -Wno-error=char-subscripts -Wno-error=sequence-point -Wno-error=unused-value -Wno-error=enum-compare -Wno-error=format= -fverbose-asm
CXXFLAGS += -Wno-error=switch -Wno-error=char-subscripts -Wno-error=sequence-point -Wno-error=unused-value -Wno-error=enum-compare -Wno-error=format= -fverbose-asm