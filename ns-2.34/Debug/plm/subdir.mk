################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
O_SRCS += \
../plm/cbr-traffic-PP.o \
../plm/loss-monitor-plm.o 

CC_SRCS += \
../plm/cbr-traffic-PP.cc \
../plm/loss-monitor-plm.cc 

OBJS += \
./plm/cbr-traffic-PP.o \
./plm/loss-monitor-plm.o 

CC_DEPS += \
./plm/cbr-traffic-PP.d \
./plm/loss-monitor-plm.d 


# Each subdirectory must supply rules for building sources it contributes
plm/%.o: ../plm/%.cc
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


