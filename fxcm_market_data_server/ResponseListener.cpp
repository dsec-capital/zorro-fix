#include "ResponseListener.h"

#include <iostream>

#include "spdlog/spdlog.h"

constexpr auto _TIMEOUT = 30000;

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

/** Gets response.*/
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

void ResponseListener::onRequestCompleted(pricehistorymgr::IPriceHistoryCommunicatorRequest *request,
                                          pricehistorymgr::IPriceHistoryCommunicatorResponse *response)
{
    if (mRequest == request)
    {
        mResponse = response;
        mResponse->addRef();
        SetEvent(mSyncResponseEvent);
    }
}

void ResponseListener::onRequestFailed(pricehistorymgr::IPriceHistoryCommunicatorRequest *request,
                                       pricehistorymgr::IError *error)
{
    if (mRequest == request)
    {
        spdlog::error("request failed: {}", error ? error->getMessage() : "unknown error");

        mRequest = NULL;
        mResponse = NULL;

        SetEvent(mSyncResponseEvent);
    }
}

void ResponseListener::onRequestCancelled(pricehistorymgr::IPriceHistoryCommunicatorRequest *request)
{
    if (mRequest == request)
    {
        spdlog::info("request cancelled");

        mRequest = NULL;
        mResponse = NULL;

        SetEvent(mSyncResponseEvent);
    }
}

