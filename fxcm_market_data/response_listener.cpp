#include "pch.h"

#include "response_listener.h"

ResponseListener::ResponseListener()
{
    sync_response_event = CreateEvent(0, FALSE, FALSE, 0);
    response = NULL;
    request = NULL;
}

ResponseListener::~ResponseListener()
{
    CloseHandle(sync_response_event);
}

bool ResponseListener::wait()
{
    return WaitForSingleObject(sync_response_event, INFINITE) == WAIT_OBJECT_0;
}

pricehistorymgr::IPriceHistoryCommunicatorResponse* ResponseListener::get_response()
{
    if (response)
        response->addRef();
    return response;
}

void ResponseListener::set_request(pricehistorymgr::IPriceHistoryCommunicatorRequest *request)
{
    response = NULL;
    request = request;
    request->addRef();
}

void ResponseListener::onRequestCompleted(pricehistorymgr::IPriceHistoryCommunicatorRequest *request, pricehistorymgr::IPriceHistoryCommunicatorResponse *response)
{
    if (request == request)
    {
        response = response;
        response->addRef();
        SetEvent(sync_response_event);
    }
}

void ResponseListener::onRequestFailed(pricehistorymgr::IPriceHistoryCommunicatorRequest *request, pricehistorymgr::IError *error)
{
    if (request == request)
    {
        spdlog::error(
            "ResponseListener::onRequestFailed: error {}",
            error != nullptr ? error->getMessage() : "cannot retrieve error message as handler not initialized"
        );

        request = NULL;
        response = NULL;

        SetEvent(sync_response_event);
    }
}

void ResponseListener::onRequestCancelled(pricehistorymgr::IPriceHistoryCommunicatorRequest *request)
{
    if (request == request)
    {
        spdlog::error("ResponseListener::onRequestCancelled: request cancelled");

        request = NULL;
        response = NULL;

        SetEvent(sync_response_event);
    }
}

