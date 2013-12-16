################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: plataforma

dependents:
	-cd ../../Shared-Library/Release && $(MAKE) all

# Tool invocations
plataforma: $(OBJS) $(USER_OBJS) ../../Shared-Library/Release/libShared-Library.so
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc -L"../../Shared-Library/Release" -o "plataforma" $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) plataforma
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:
../../Shared-Library/Release/libShared-Library.so:

-include ../makefile.targets
