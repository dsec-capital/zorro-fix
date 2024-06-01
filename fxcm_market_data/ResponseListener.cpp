#include "pch.h"

#include "ResponseListener.h"

ResponseListener::ResponseListener()
{
    mSyncResponseEvent = CreateEvent(0, FALSE, FALSE, 0);

    mResponse = NULL;
    mRequest = NULL;
}

ResponseListener::~ResponseListener()
{
    CloseHandle(mSyncResponseEvent);
}

bool ResponseListener::wait()
{
    return WaitForSingleObject(mSyncResponseEvent, INFINITE) == WAIT_OBJECT_0;
}

pricehistorymgr::IPriceHistoryCommunicatorResponse* ResponseListener::getResponse()
{
    if (mResponse)
        mResponse->addRef();
    return mResponse;
}

void ResponseListener::setRequest(pricehistorymgr::IPriceHistoryCommunicatorRequest *request)
{
    mResponse = NULL;
    mRequest = request;
    request->addRef();
}

void ResponseListener::onRequestCompleted(pricehistorymgr::IPriceHistoryCommunicatorRequest *request, pricehistorymgr::IPriceHistoryCommunicatorResponse *response)
{
    if (mRequest == request)
    {
        mResponse = response;
        mResponse->addRef();
        SetEvent(mSyncResponseEvent);
    }
}

void ResponseListener::onRequestFailed(pricehistorymgr::IPriceHistoryCommunicatorRequest *request, pricehistorymgr::IError *error)
{
    if (mRequest == request)
    {
        spdlog::error(
            "ResponseListener::onRequestFailed: error {}",
            error != nullptr ? error->getMessage() : "cannot retrieve error message as handler not initialized"
        );

        mRequest = NULL;
        mResponse = NULL;

        SetEvent(mSyncResponseEvent);
    }
}

void ResponseListener::onRequestCancelled(pricehistorymgr::IPriceHistoryCommunicatorRequest *request)
{
    if (mRequest == request)
    {
        spdlog::error("ResponseListener::onRequestCancelled: request cancelled");

        mRequest = NULL;
        mResponse = NULL;

        SetEvent(mSyncResponseEvent);
    }
}

