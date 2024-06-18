#pragma once

#include <string>
#include <iostream>

#include "ForexConnect.h"

#include "ThreadSafeAddRefImpl.h"

/** The Price History API communicator request result listener. */
class ResponseListener
    : public TThreadSafeAddRefImpl<pricehistorymgr::IPriceHistoryCommunicatorListener>
{
 public:
    ResponseListener();

    /** Wait for request execution or an error. */
    bool wait();

    /** Get response.*/
    pricehistorymgr::IPriceHistoryCommunicatorResponse* getResponse();

    /** Set the request before waiting for execution response. */
    void setRequest(pricehistorymgr::IPriceHistoryCommunicatorRequest *request);

 public:
    /** @name IPriceHistoryCommunicatorListener interface implementation */
    //@{
    virtual void onRequestCompleted(pricehistorymgr::IPriceHistoryCommunicatorRequest *request,
                                    pricehistorymgr::IPriceHistoryCommunicatorResponse *response);
    virtual void onRequestFailed(pricehistorymgr::IPriceHistoryCommunicatorRequest *request, pricehistorymgr::IError *error);
    virtual void onRequestCancelled(pricehistorymgr::IPriceHistoryCommunicatorRequest *request);
    //@}

 protected:
    virtual ~ResponseListener();

 private:
    O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorRequest> mRequest;
    O2G2Ptr<pricehistorymgr::IPriceHistoryCommunicatorResponse> mResponse;

    HANDLE mSyncResponseEvent;
};

