////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License.  You may obtain a copy
// of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
// License for the specific language governing permissions and limitations
// under the License.
////////////////////////////////////////////////////////////////////////////////

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
