################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ginyu/config.c \
../ginyu/list.c \
../ginyu/log.c \
../ginyu/protocolo.c \
../ginyu/sockets.c 

OBJS += \
./ginyu/config.o \
./ginyu/list.o \
./ginyu/log.o \
./ginyu/protocolo.o \
./ginyu/sockets.o 

C_DEPS += \
./ginyu/config.d \
./ginyu/list.d \
./ginyu/log.d \
./ginyu/protocolo.d \
./ginyu/sockets.d 


# Each subdirectory must supply rules for building sources it contributes
ginyu/%.o: ../ginyu/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


