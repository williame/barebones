# flags for various build platform targets

ifeq ($(shell uname),MINGW32_NT-6.1) # mingw
	EXE_EXT =.exe
	SDL_CFLAGS =-I/usr/include `sdl-config --cflags`
	SDL_LDFLAGS =-L/usr/lib `sdl-config --static-libs` -static-libgcc -lopengl32 -lglew32
else
	EXE_EXT =
	SDL_CFLAGS =`pkg-config --cflags sdl gl glew`
	SDL_LDFLAGS =`pkg-config --libs sdl gl glew`
endif

NACL_PATH_32 = ${NACL_SDK_ROOT}/pepper_17/toolchain/linux_x86_newlib/bin/
NACL_PATH_64 = ${NACL_SDK_ROOT}/pepper_17/toolchain/linux_x86_newlib/bin/
NACL_CLFLAGS =
NACL_LDFLAGS = -lppapi -lppapi_cpp -lppapi_gles2

# generic flags

CFLAGS = -g3 -Wall -O0 #-pedantic-errors -std=c++98 -Wno-long-long -fdiagnostics-show-option
# -O9 -fomit-frame-pointer -march=native # etc -fprofile-generate/-fprofile-use

BUILD_TIMESTAMP = $(shell date +%y%m%d-%H%M%S)
CFLAGS += -DBUILD_TIMESTAMP=\"$(BUILD_TIMESTAMP)\" -DGIT_INFO=\"$(shell git symbolic-ref -q HEAD)\"

# c++ object files
	
OBJ_BASE_CPP = \
	game.opp \
	barebones/xml.opp \
	barebones/g3d.opp \
	barebones/rand.opp \
	barebones/build_info.opp \
	barebones/main.opp

OBJ_SDL_CPP = $(OBJ_BASE_CPP:%.opp=%.sdl.opp)

OBJ_NACL_32_CPP = $(OBJ_BASE_CPP:%.opp=%.nacl.x86-32.opp)
OBJ_NACL_64_CPP = $(OBJ_BASE_CPP:%.opp=%.nacl.x86-64.opp)

OBJ_CPP = ${OBJ_SDL_CPP} ${OBJ_NACL_32_CPP} ${OBJ_NACL_64_CPP}

# c object files

OBJ_BASE_C = \
	external/SOIL/SOIL.o \
	external/SOIL/image_helper.o \
	external/SOIL/stb_image_aug.o \
	external/SOIL/image_DXT.o

OBJ_SDL_C = $(OBJ_BASE_C:%.o=%.sdl.o)

OBJ_NACL_32_C = $(OBJ_BASE_C:%.o=%.nacl.x86-32.o)
OBJ_NACL_64_C = $(OBJ_BASE_C:%.o=%.nacl.x86-64.o)

OBJ_C = ${OBJ_SDL_C} ${OBJ_NACL_32_C} ${OBJ_NACL_64_C}

OBJ = ${OBJ_CPP} ${OBJ_C}

# targets

TARGET_BIN = game
TARGET = bin/${TARGET_BIN}

TARGETS = ${TARGET}${EXE_EXT} ${TARGET}.x86-32.nexe ${TARGET}.x86-64.nexe

.PHONY:	clean all check_env zip

${TARGET}${EXE_EXT}: ${OBJ_SDL_CPP} ${OBJ_SDL_C}
	g++ ${CFLAGS} -o $@ $^ ${LDFLAGS} ${SDL_LDFLAGS}

${TARGET}.x86-32.nexe: ${OBJ_NACL_32_CPP} ${OBJ_NACL_32_C}
	${NACL_PATH_32}i686-nacl-g++ ${CFLAGS} -o $@ -m32 $^ ${LDFLAGS} ${NACL_LDFLAGS}

${TARGET}.x86-64.nexe: ${OBJ_NACL_64_CPP} ${OBJ_NACL_64_C}
	${NACL_PATH_64}x86_64-nacl-g++ ${CFLAGS} -o $@ -m64 $^ ${LDFLAGS} ${NACL_LDFLAGS}

all:	check_env ${TARGETS}

run:	check_env ${TARGET}${EXE_EXT}
ifeq ($(shell uname),windows32)
	rm -f bin/stderr.txt bin/stdout.txt
	(cd bin && ${TARGET_BIN}${EXE_EXT} && cat bin/stdout.txt) || (cat bin/stdout.txt bin/stderr.txt && exit 1)
else
	cd bin && ./${TARGET_BIN}${EXE_EXT}
endif
	
debug:	check_env ${TARGET}${EXE_EXT}
	cd bin && gdb --args ./${TARGET_BIN}${EXE_EXT}
	
valgrind:	check_env ${TARGET}${EXE_EXT}
	cd bin && valgrind ./${TARGET_BIN}${EXE_EXT}
	
ZIP_FMT:=tar.gz
ZIP_FILENAME:=bin/${TARGET_BIN}-${BUILD_TIMESTAMP}.${ZIP_FMT}

zip:
	git archive --format ${ZIP_FMT} ${ZIP_FILENAME} HEAD

# compile c files
	
%.sdl.o:	%.c
	gcc ${CFLAGS} -c $< -MD -MF $(<:%.c=%.sdl.dep) -o $@ ${SDL_CFLAGS}
	
%.nacl.x86-32.o:	%.c
	${NACL_PATH_32}i686-nacl-gcc ${CFLAGS} -c $< -MD -MF $(<:%.c=%.nacl.x86-32.dep) -m32 -o $@

%.nacl.x86-64.o:	%.c
	${NACL_PATH_64}x86_64-nacl-gcc ${CFLAGS} -c $< -MD -MF $(<:%.c=%.nacl.x86-64.dep) -m64 -o $@

# compile c++ files
	
%.sdl.opp:	%.cpp
	g++ ${CFLAGS} -c $< -MD -MF $(<:%.cpp=%.sdl.dep) -o $@ ${SDL_CFLAGS}

%.nacl.x86-32.opp:	%.cpp
	${NACL_PATH_32}i686-nacl-g++ ${CFLAGS} -c $< -MD -MF $(<:%.cpp=%.nacl.x86-32.dep) -m32 -o $@

%.nacl.x86-64.opp:	%.cpp
	${NACL_PATH_64}x86_64-nacl-g++ ${CFLAGS} -c $< -MD -MF $(<:%.cpp=%.nacl.x86-64.dep) -m64 -o $@

#misc

clean:
	rm -f ${TARGETS}
	rm -f ${OBJ}
	rm -f $(OBJ_C:%.o=%.dep) $(OBJ_CPP:%.opp=%.dep)
	rm -f *.?pp~ Makefile~ core
	
DUMMY := $(shell rm -f build_info.*.opp) # we want these always built and I'm tired of trying to get .PHONY to work nicely

check_env:
ifeq ($(shell uname),MINGW32_NT-6.1) # mingw
	@echo "You are using windows; good luck!"
else
	@echo "You are using $(shell uname); we'll assume this is unix-like"
	@echo ${TARGET}
	`pkg-config --exists sdl gl glew glu`
endif

-include $(OBJ_C:%.o=%.dep) $(OBJ_CPP:%.opp=%.dep)

