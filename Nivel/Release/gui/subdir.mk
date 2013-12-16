################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../gui/nivel.c \
../gui/tad_items.c 

OBJS += \
./gui/nivel.o \
./gui/tad_items.o 

C_DEPS += \
./gui/nivel.d \
./gui/tad_items.d 


# Each subdirectory must supply rules for building sources it contributes
gui/%.o: ../gui/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -Ipthread -I"../../Shared-Library" -Im -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


