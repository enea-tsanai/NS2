################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../trace/basetrace.o \
../trace/cmu-trace.o \
../trace/trace-ip.o \
../trace/trace.o \
../trace/traffictrace.o 

CC_SRCS += \
../trace/basetrace.cc \
../trace/cmu-trace.cc \
../trace/trace-ip.cc \
../trace/trace.cc \
../trace/traffictrace.cc 

OBJS += \
./trace/basetrace.o \
./trace/cmu-trace.o \
./trace/trace-ip.o \
./trace/trace.o \
./trace/traffictrace.o 

CC_DEPS += \
./trace/basetrace.d \
./trace/cmu-trace.d \
./trace/trace-ip.d \
./trace/trace.d \
./trace/traffictrace.d 


# Each subdirectory must supply rules for building sources it contributes
trace/%.o: ../trace/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


