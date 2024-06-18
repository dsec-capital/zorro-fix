#include "CommunicatorStatusListener.h"

constexpr auto _TIMEOUT = 30000;

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
    if (res != 0)
        std::cout << "Timeout occurred during waiting for communicator status is ready" << std::endl;
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
    std::cout << "Communicator initialization error: " << error->getMessage() << std::endl;
    SetEvent(mSyncCommunicatorEvent);
}
