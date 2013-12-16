################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../FileSystem.c \
../Grasa_Dummy.c \
../Grasa_Handler.c \
../Grasa_Read.c \
../Grasa_Write.c \
../grasa_Cache.c 

OBJS += \
./FileSystem.o \
./Grasa_Dummy.o \
./Grasa_Handler.o \
./Grasa_Read.o \
./Grasa_Write.o \
./grasa_Cache.o 

C_DEPS += \
./FileSystem.d \
./Grasa_Dummy.d \
./Grasa_Handler.d \
./Grasa_Read.d \
./Grasa_Write.d \
./grasa_Cache.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=27 -I"/home/utnso/tp-2013-2c-c-o-no-ser/Shared-Library" -Ipthread -Ifuse -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


