################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/ATU_TCPClient.cpp 

OBJS += \
./src/ATU_TCPClient.o 

CPP_DEPS += \
./src/ATU_TCPClient.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -D'__SVN_REV__="$(shell svnversion -n /home/qy/Dropbox/nirvana/workspaceCN/atu_tcp_library)"' -D'__SVN_WKSPC_REV__="$(shell /home/qy/Dropbox/nirvana/workspaceCN/loglibrary/getWorkspaceSvnRevision /home/qy/Dropbox/nirvana/workspaceCN)"' -I"/home/qy/Dropbox/nirvana/workspaceCN/atu_tcp_library/include" -I"/home/qy/Dropbox/nirvana/workspaceCN/loglibrary/include" -O3 -g3 -Wall -c -fmessage-length=0 -fPIC -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

