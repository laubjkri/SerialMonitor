#include <windows.h>
#include <stdio.h>

#pragma warning(disable : 4996)

static char* GetErrorText(DWORD code)
{
	static char buffer[200] = "";

	snprintf(buffer, sizeof(buffer), "%ld", code);

	switch (code)
	{
		case ERROR_FILE_NOT_FOUND:
			strcat(buffer, ": File not found");		

		case ERROR_ACCESS_DENIED:
			strcat(buffer, ": Access denied");		

		default:
			strcat(buffer, ": Unknown error code");		
	}

	return buffer;
}

static void PrintLastError()
{
	DWORD lastError = GetLastError();
	fprintf(stderr, GetErrorText(lastError));
	fprintf(stderr, "\n");
}

static BOOL IsPortAvailable(const char* portName)
{
	// Try to open the serial port
	HANDLE serialHandle = CreateFileA(portName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	BOOL available = serialHandle != INVALID_HANDLE_VALUE;

	if (available)
	{
		CloseHandle(serialHandle); // Close the port if it was successfully opened
	}

	return available;
}

static void MonitorUsbSerial(const char* comPortName)
{
	// Open the serial port
	// HANDLE serialHandle = CreateFile(portName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	HANDLE serialHandle = CreateFileA(comPortName, GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (serialHandle == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "Error in opening serial port\n");
		PrintLastError();

		return;
	}
	else
	{
		printf("Opening serial port successful\n");
	}

	// Flush away any bytes previously read or written.
	//BOOL success = FlushFileBuffers(serialHandle);
	//if (!success)
	//{
	//	fprintf(stderr, "Failed to flush serial port");
	//	PrintLastError();
	//	CloseHandle(serialHandle);
	//	return;
	//}	

	// Set device parameters (115200 baud, 1 start bit, 1 stop bit, no parity)
	DCB serialParams = { 0 };
	serialParams.DCBlength = sizeof(serialParams);

	if (!GetCommState(serialHandle, &serialParams))
	{
		fprintf(stderr, "Error getting device state\n");
		CloseHandle(serialHandle);
		return;
	}

	serialParams.BaudRate = CBR_115200;
	serialParams.ByteSize = 8;
	serialParams.StopBits = ONESTOPBIT;
	serialParams.Parity = NOPARITY;
	serialParams.fDtrControl = DTR_CONTROL_ENABLE; // Flag Data Terminal Ready: Signals to the device that we are ready to receive data
	
	if (!SetCommState(serialHandle, &serialParams))
	{
		fprintf(stderr, "Error setting device parameters\n");
		CloseHandle(serialHandle);
		return;
	}

	// Set COM port timeout settings
	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 50;
	timeouts.ReadTotalTimeoutConstant = 50;
	timeouts.ReadTotalTimeoutMultiplier = 10;
	timeouts.WriteTotalTimeoutConstant = 50;
	timeouts.WriteTotalTimeoutMultiplier = 10;
	if (!SetCommTimeouts(serialHandle, &timeouts))
	{
		fprintf(stderr, "Error setting timeouts\n");
		CloseHandle(serialHandle);
		return;
	}

	// Loop to continuously read data
	DWORD bytesRead;
	char readBuffer[2048] = { 0 };

	while (1)
	{
		BOOL result = ReadFile(serialHandle, readBuffer, sizeof(readBuffer) - 1, &bytesRead, NULL);
		if (!result)
		{
			// If reading fails, assume the device was disconnected
			printf("Device disconnected, waiting for reconnection...\n");
			CloseHandle(serialHandle);
			return; // Exit this function to handle reconnection in the main loop
		}

		if (bytesRead > 0)
		{
			readBuffer[bytesRead] = '\0';  // Null-terminate the string
			printf("> %s", readBuffer);
		}
	}
}


int main(int argc, char* argv[])
{

	// Check if a COM port is specified as a command line argument
	if (argc < 2)
	{
		fprintf(stderr, "COM port must be specified as a parameter. Example: SerialMonitor -COM3\n");
		return 1; // Exit if no COM port is provided
	}

	char* portName = { argv[1] }; 

	if (portName[0] != '-')
	{
		fprintf(stderr, "Invalid format of port name '%s'. Example: SerialMonitor -COM3", portName);
		return 1;
	}

	portName++; // +1 leaves out '-'

	if (strnlen(portName, 100) > 100)
	{
		fprintf(stderr, "Portname cannot exceed 100 characters");
		return 1;
	}


	char portNameSyntax[256] = "\\\\.\\"; // + fx COM3	
	strncat(portNameSyntax, portName, 220);

	printf("Waiting for device to be connected on %s...\n", portName);

	while (1)
	{
		if (IsPortAvailable(portNameSyntax))
		{
			MonitorUsbSerial(portNameSyntax);
		}
		Sleep(1000);
	}

	return 0;
}
