/*
// Copyright (c) 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/


#ifndef OS_ANDROID_HWC_TEST_LOGENTRY_H
#define OS_ANDROID_HWC_TEST_LOGENTRY_H

/* for PRI?64 */
#define __STDC_FORMAT_MACROS 1
#include <inttypes.h>
#include <utils/Timers.h>
#include <utils/String8.h>

using namespace hwcomposer;

class LogEntry : public LightFlattenable<LogEntry>
{
public:
    LogEntry();
    LogEntry(nsecs_t timestamp, String8 description);
    ~LogEntry();

    pid_t                               getTID() const                      { return mTID; }
    nsecs_t                             getTimestamp() const                { return mTimestamp; }
    const String8&                      getDescription() const              { return mDescription; }
    inline bool                         isFixedSize() const { return false; }
    size_t                              getFlattenedSize() const { return 0; }
    status_t                            flatten(void* buffer, size_t size) const;
    status_t                            unflatten(void const* buffer, size_t size);
    status_t                            read(sp<IDiagnostic> pDiagnostic);
    void                                print(bool bVeryVerbose = false, bool bVerbose = false, bool bFences = false, bool bBufferManager = false, bool bQueue = false);

    static void                         discardAll(sp<IDiagnostic> pDiagnostic);
    static void                         printAll(sp<IDiagnostic> pDiagnostic, bool bVeryVerbose = false, bool bVerbose = false, bool bFences = false, bool bBufferManager = false, bool bQueue = false);

private:
    pid_t                               mTID;
    nsecs_t                             mTimestamp;
    String8                             mDescription;
};

LogEntry::LogEntry() :
    mTID(0),
    mTimestamp(0)
{
}

LogEntry::LogEntry(nsecs_t timestamp, String8 description) :
    mTID(0),
    mTimestamp(timestamp),
    mDescription(description)
{
}

LogEntry::~LogEntry()
{
}

template <typename T>
void unflattenFromBuffer(const char*& pBuffer, const char* pBufferEnd, T& value)
{
    if ((pBufferEnd - pBuffer) >= (int)sizeof(T))
    {
        value = *(reinterpret_cast<const T*>(pBuffer));
        pBuffer += sizeof(T);
    }
}

status_t LogEntry::unflatten(void const* buffer, size_t size)
{
    // Delete any existing data structures
    char const * pBuffer = (char const *) buffer;
    char const * pBufferEnd = ((char const *) buffer) + size;

    unflattenFromBuffer(pBuffer, pBufferEnd, mTID);
    unflattenFromBuffer(pBuffer, pBufferEnd, mTimestamp);

    mDescription = pBuffer;
    return NO_ERROR;
}

status_t LogEntry::read(sp<IDiagnostic> pDiagnostic)
{
    status_t ret = NO_ERROR;

    static Parcel* spReply = NULL;
    if (spReply)
    {
        ret = spReply->readInt32();

        if (ret == NOT_ENOUGH_DATA)
        {
            // Marks end of parcel, time to fetch a new one
            delete spReply;
            spReply = 0;
        }
    }

    if (spReply == 0)
    {
        spReply = new Parcel;
        pDiagnostic = NULL;
        //ret = pDiagnostic->readLogParcel(spReply);

        if (ret >= 0)
            ret = spReply->readInt32();
   }

    if (ret < 0)
    {
        delete spReply;
        spReply = 0;
        return ret;
    }

    spReply->read(*this);
    return ret;
}


void LogEntry::print(bool bVeryVerbose, bool bVerbose, bool bFences, bool bBufferManager, bool bQueue)
{
    if (strstr(getDescription(), "SF0 onPrepare Entry") ||
        strstr(getDescription(), "InputAnalyzer SF0"))
    {

        printf("\n\n");
    }

    if (!bVeryVerbose && bVerbose == false)
    {
        // Strip out any 'verbose' matching strings
        if (strstr(getDescription(), "onPrepare Entry") ||
            strstr(getDescription(), "onPrepare Exit") ||
            strstr(getDescription(), "onSet Exit") ||
            strncmp(getDescription(), "InternalBuffer", 14) == 0 ||
            strncmp(getDescription(), "drm", 3) == 0 ||
            strncmp(getDescription(), "adf", 3) == 0)
            return;
    }
    if (!bVeryVerbose && bFences == false)
    {
        // Strip out any 'fence' matching strings.
        if (strncmp(getDescription(), "Fence:", 6) == 0)
            return;
        if (strncmp(getDescription(), "NativeFence:", 12) == 0)
            return;
    }
    if (!bVeryVerbose && bBufferManager == false)
    {
        // Strip out any 'buffer manager' matching strings.
        if (strncmp(getDescription(), "BufferManager:", 14) == 0)
            return;
    }
    if (!bVeryVerbose && bQueue == false)
    {
        // Strip out any 'display queue' matching strings.
        if (strncmp(getDescription(), "Queue:", 6) == 0)
            return;
    }


    printf("%" PRIi64 "s %03" PRIi64 "ms", getTimestamp()/1000000000, (getTimestamp() % 1000000000)/1000000);
    if (bVerbose)
    {
        printf(" %06" PRIi64 "ns", getTimestamp() % 1000000);
        printf(" TID:%d", getTID());
    }
    printf(" %s\n", getDescription().string());
}

void LogEntry::discardAll(sp<IDiagnostic> pDiagnostic)
{
    while (1)
    {
        LogEntry entry;
        status_t ret = entry.read(pDiagnostic);
        if (ret != OK)
            break;
    }
}

void LogEntry::printAll(sp<IDiagnostic> pDiagnostic, bool bVeryVerbose, bool bVerbose, bool bFences, bool bBufferManager, bool bQueue)
{
    while (1)
    {
        LogEntry entry;
        status_t ret = entry.read(pDiagnostic);
        if (ret != OK)
            break;
        entry.print(bVeryVerbose, bVerbose, bFences, bBufferManager, bQueue);
    }
}


#endif // OS_ANDROID_HWC_TEST_LOGENTRY_H
