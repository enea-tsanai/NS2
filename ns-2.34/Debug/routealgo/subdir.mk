################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../routealgo/bfs.o \
../routealgo/rbitmap.o \
../routealgo/rlookup.o \
../routealgo/rnode.o \
../routealgo/routealgo.o 

CC_SRCS += \
../routealgo/bfs.cc \
../routealgo/dijkstra.cc \
../routealgo/rbitmap.cc \
../routealgo/rlookup.cc \
../routealgo/rnode.cc \
../routealgo/routealgo.cc \
../routealgo/tnode.cc 

OBJS += \
./routealgo/bfs.o \
./routealgo/dijkstra.o \
./routealgo/rbitmap.o \
./routealgo/rlookup.o \
./routealgo/rnode.o \
./routealgo/routealgo.o \
./routealgo/tnode.o 

CC_DEPS += \
./routealgo/bfs.d \
./routealgo/dijkstra.d \
./routealgo/rbitmap.d \
./routealgo/rlookup.d \
./routealgo/rnode.d \
./routealgo/routealgo.d \
./routealgo/tnode.d 


# Each subdirectory must supply rules for building sources it contributes
routealgo/%.o: ../routealgo/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


