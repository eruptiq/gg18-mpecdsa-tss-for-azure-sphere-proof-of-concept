/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

   // This sample C application for Azure Sphere illustrates how to use mutable storage.
   //
   // It uses the API for the following Azure Sphere application libraries:
   // - log (messages shown in Visual Studio's Device Output window during debugging)
   // - gpio (digital input for buttons)
   // - storage (managing persistent user data)

#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

// applibs_versions.h defines the API struct versions to use for applibs APIs.
#include "../applibs_versions.h"
#include <applibs/log.h>
#include <applibs/storage.h>
#include <applibs/gpio.h>

// By default, this sample is targeted at the MT3620 Reference Development Board (RDB).
// This can be changed using the project property "Target Hardware Definition Directory".
// This #include imports the sample_hardware abstraction from that hardware definition.
#include <hw/sample_hardware.h>

#include <storage.h>

const size_t FULL_SIZE = 16 * 1024 - 1;
const size_t FULL_STORAGE_SIZE = FULL_SIZE * sizeof(char);

/// <summary>
/// Read an string from this application's persistent data file
/// </summary>
/// <returns>
/// The string that was read from the file.  If the file is empty, this returns 0.  If the storage
/// API fails, this returns -1.
/// </returns>
static char* ReadStringFromMutableFile(int fd, size_t count)
{
	char* val = (char *)malloc(count + 1);
	char c;
	off_t sizeofCount = sizeof(count);
	off_t offset = 0;
	off_t endOffset = count + sizeofCount;
	ssize_t ret;
	while ((ret = pread(fd, &c, 1, offset)) > 0 && offset <= endOffset)
	{
		if (offset >= sizeofCount) {
			val[offset - sizeofCount] = c;
		}
		offset++;
	}
	val[offset] = NULL;

	if (ret < 0) {
		Log_Debug("ERROR: An error occurred while reading file:  %s (%d).\n", strerror(errno),
			errno);
	}

	return val;
}

static int OpenMutableStorage(void) {
	int fd = Storage_OpenMutableFile();
	if (fd < 0) {
		Log_Debug("ERROR: Could not open mutable file:  %s (%d).\n", strerror(errno), errno);
	}
	return fd;
}

void DeleteMutableStorage(void) {
	int ret = Storage_DeleteMutableFile();
	if (ret < 0) {
		Log_Debug("An error occurred while deleting the mutable file: %s (%d).\n",
			strerror(errno), errno);
	}
	else {
		Log_Debug("Successfully deleted the mutable file!\n");
	}
}

/// <summary>
/// Write a string to this application's persistent data file
/// </summary>
static void WriteStringToMutableFile(int fd, char* val)
{
	size_t len = strlen(val);
	ssize_t ret = write(fd, val, len);
	if (ret < 0) {
		// If the file has reached the maximum count specified in the application manifest,
		// then -1 will be returned with errno EDQUOT (122)
		Log_Debug("ERROR: An error occurred while writing to mutable file:  %s (%d).\n",
			strerror(errno), errno);
	}
	else if (ret < len) {
		// For simplicity, this sample logs an error here. In the general case, this should be
		// handled by retrying the write with the remaining data until all the data has been
		// written.
		Log_Debug("ERROR: Only wrote %d of %d bytes requested\n", ret, len);
	}
}

/// <summary>
/// Write a string to this application's persistent data file
/// </summary>
void SaveToStorage(char* val)
{
	DeleteMutableStorage(); //TODO: bool to overwrite, append etc

	int fd = OpenMutableStorage();

	size_t count = strlen(val);
	ssize_t ret = write(fd, &count, sizeof(count));

	if (ret < 0 || count < 0) {
		Log_Debug("ERROR: An error occurred while reading file:  %s (%d).\n", strerror(errno),
			errno);
		return;
	}

	WriteStringToMutableFile(fd, val);
	close(fd);
	Log_Debug("SaveToStorage: result\n****1****\n%s\n**********\n len %d", val, strlen(val));
}

/// <summary>
/// Read a string from this application's persistent data file
/// </summary>
char* ReadFromStorage() {
	int fd = OpenMutableStorage();
	char* result;
	Log_Debug("ReadFromStorage fd: %d\n", fd);
	//ssize_t ret = write(fd, &count, sizeof(count));
	int count = 0;
	ssize_t ret = read(fd, &count, sizeof(count));
	if (ret < 0 || count < 0) {
		Log_Debug("ERROR: An error occurred while reading file:  %s (%d).\n", strerror(errno),
			errno);
		return NULL;
	}

	if (count == 0) {
		Log_Debug("ReadFromStorage count == 0");
		return strdup("");
	}

	Log_Debug("ReadFromStorage  before ReadStringFromMutableFile count :%d ", count);

	if (count > FULL_STORAGE_SIZE) {
		count = FULL_STORAGE_SIZE - sizeof(count);
	}

	result = ReadStringFromMutableFile(fd, count);
	Log_Debug("ReadFromStorage: result \n****1*****\n%s\n**********\n len %d", result, strlen(result));
	close(fd);
	return result;
}