//--------------------------------------------------------------------------------------
// Copyright 2011 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//
//--------------------------------------------------------------------------------------

#include "StopWatch.h"
#include <cassert>

// Initialize member variables.
StopWatch::StopWatch() :
	frequency(0),
	start(0),
	stop(0),
	affinityMask(0)
{
	// Initialize the performance counter frequency.
	LARGE_INTEGER perfQuery;
	BOOL supported = QueryPerformanceFrequency(&perfQuery);
	assert(supported == TRUE);
	this->frequency = perfQuery.QuadPart;
}

// Start the stopwatch.
void StopWatch::Start()
{
	// MSDN recommends setting the thread affinity to avoid bugs in the BIOS and HAL.
	// Create an affinity mask for the current processor.
	affinityMask = (DWORD_PTR)1 << GetCurrentProcessorNumber();
	HANDLE currThread = GetCurrentThread();
	DWORD_PTR prevAffinityMask = SetThreadAffinityMask(currThread, affinityMask);
	assert(prevAffinityMask != 0);

	// Query the performance counter.
	LARGE_INTEGER perfQuery;
	BOOL result = QueryPerformanceCounter(&perfQuery);
    assert(result);
    start = perfQuery.QuadPart;

	// Restore the thread's affinity mask.
	prevAffinityMask = SetThreadAffinityMask(currThread, prevAffinityMask);
	assert(prevAffinityMask != 0);
}

// Stop the stopwatch.
void StopWatch::Stop()
{
	// MSDN recommends setting the thread affinity to avoid bugs in the BIOS and HAL.
	// Use the affinity mask that was created in the Start function.
	HANDLE currThread = GetCurrentThread();
	DWORD_PTR prevAffinityMask = SetThreadAffinityMask(currThread, affinityMask);
	assert(prevAffinityMask != 0);

	// Query the performance counter.
	LARGE_INTEGER perfQuery;
	BOOL result = QueryPerformanceCounter(&perfQuery);
    assert(result);
    stop = perfQuery.QuadPart;

	// Restore the thread's affinity mask.
	prevAffinityMask = SetThreadAffinityMask(currThread, prevAffinityMask);
	assert(prevAffinityMask != 0);
}

// Reset the stopwatch.
void StopWatch::Reset()
{
	start = 0;
	stop = 0;
	affinityMask = 0;
}

// Get the elapsed time in seconds.
double StopWatch::TimeInSeconds() const
{
	// Return the elapsed time in seconds.
	assert((stop - start) > 0);
	return double(stop - start) / double(frequency);
}

// Get the elapsed time in milliseconds.
double StopWatch::TimeInMilliseconds() const
{
	// Return the elapsed time in milliseconds.
	assert((stop - start) > 0);
	return double(stop - start) / double(frequency) * 1000.0;
}

// Get the elapsed time in microseconds.
double StopWatch::TimeInMicroseconds() const
{
	// Return the elapsed time in microseconds.
	assert((stop - start) > 0);
	return double(stop - start) / double(frequency) * 1000000.0;
}
