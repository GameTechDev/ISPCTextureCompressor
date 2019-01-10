////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2017-2019 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
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
