#include "pch.h"

#include "CommunicatorStatusListener.h"

CommunicatorStatusListener::CommunicatorStatusListener()
    : mReady(false), mError(false)
{
    mSyncCommunicatorEvent = CreateEvent(0, FALSE, FALSE, 0);
}

CommunicatorStatusListener::~CommunicatorStatusListener()
{
    CloseHandle(mSyncCommunicatorEvent);
}

bool CommunicatorStatusListener::isReady()
{
    return mReady;
}

void CommunicatorStatusListener::reset()
{
    mReady = false;
    mError = false;
}

bool CommunicatorStatusListener::waitEvents()
{
    int res = WaitForSingleObject(mSyncCommunicatorEvent, _TIMEOUT);
    if (res != 0) {
        spdlog::error("CommunicatorStatusListener::waitEvents: timeout occured while waiting for communicator status to be ready");
    }
    return res == 0;
}

void CommunicatorStatusListener::onCommunicatorStatusChanged(bool ready)
{
    mReady = ready;
    SetEvent(mSyncCommunicatorEvent);
}

void CommunicatorStatusListener::onCommunicatorInitFailed(pricehistorymgr::IError *error)
{
    mError = true;
    spdlog::error(
        "CommunicatorStatusListener::onCommunicatorInitFailed: error {}",
        error != nullptr ? error->getMessage() : "cannot retrieve error message as handler not initialized"
    );
    SetEvent(mSyncCommunicatorEvent);
}
