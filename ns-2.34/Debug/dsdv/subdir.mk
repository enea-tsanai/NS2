################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../dsdv/dsdv.o \
../dsdv/rtable.o 

CC_SRCS += \
../dsdv/dsdv.cc \
../dsdv/rtable.cc 

OBJS += \
./dsdv/dsdv.o \
./dsdv/rtable.o 

CC_DEPS += \
./dsdv/dsdv.d \
./dsdv/rtable.d 


# Each subdirectory must supply rules for building sources it contributes
dsdv/%.o: ../dsdv/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


