#include "pch.h"

#include "communicator_status_listener.h"

CommunicatorStatusListener::CommunicatorStatusListener()
    : ready(false), has_error(false)
{
    sync_communicator_event = CreateEvent(0, FALSE, FALSE, 0);
}

CommunicatorStatusListener::~CommunicatorStatusListener()
{
    CloseHandle(sync_communicator_event);
}

bool CommunicatorStatusListener::is_ready()
{
    return ready;
}

void CommunicatorStatusListener::reset()
{
    ready = false;
    has_error = false;
}

bool CommunicatorStatusListener::wait_events()
{
    int res = WaitForSingleObject(sync_communicator_event, _TIMEOUT);
    if (res != 0) {
        spdlog::error("CommunicatorStatusListener::wait_events: timeout occured while waiting for communicator status to be ready");
    }
    return res == 0;
}

void CommunicatorStatusListener::onCommunicatorStatusChanged(bool ready)
{
    ready = ready;
    SetEvent(sync_communicator_event);
}

void CommunicatorStatusListener::onCommunicatorInitFailed(pricehistorymgr::IError *error)
{
    has_error = true;
    spdlog::error(
        "CommunicatorStatusListener::onCommunicatorInitFailed: error {}",
        error != nullptr ? error->getMessage() : "cannot retrieve error message as handler not initialized"
    );
    SetEvent(sync_communicator_event);
}
