################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../gen/ns_tcl.o \
../gen/ptypes.o \
../gen/version.o 

C_SRCS += \
../gen/version.c 

CC_SRCS += \
../gen/ns_tcl.cc \
../gen/ptypes.cc 

OBJS += \
./gen/ns_tcl.o \
./gen/ptypes.o \
./gen/version.o 

C_DEPS += \
./gen/version.d 

CC_DEPS += \
./gen/ns_tcl.d \
./gen/ptypes.d 


# Each subdirectory must supply rules for building sources it contributes
gen/%.o: ../gen/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

gen/%.o: ../gen/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


