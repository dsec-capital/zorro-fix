#pragma once
#include "thread_safe_add_ref_impl.h"

/* 
 *The Price History API communicator request result listener. 
 */
class ResponseListener : public TThreadSafeAddRefImpl<pricehistorymgr::IPriceHistoryCommunicatorListener>
{
 public:
    ResponseListener();

    // Wait for request execution or an error
    bool wait();

    pricehistorymgr::IPriceHistoryCommunicatorResponse* get_response();

    void set_request(pricehistorymgr::IPriceHistoryCommunicatorRequest *request);

    virtual void onRequestCompleted(
        pricehistorymgr::IPriceHistoryCommunicatorRequest *request,
        pricehistorymgr::IPriceHistoryCommunicatorResponse *response
    );

    virtual void onRequestFailed(
        pricehistorymgr::IPriceHistoryCommunicatorRequest *request, 
        pricehistorymgr::IError *error
    );

    virtual void onRequestCancelled(pricehistorymgr::IPriceHistoryCommunicatorRequest *request);

 protected:
    virtual ~ResponseListener();

 private:
    O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorRequest> request;
    O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorResponse> response;

    HANDLE sync_response_event;
};

