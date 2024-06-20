#pragma once

#include <string>
#include <iostream>

#include <Windows.h>

#include "ForexConnect.h"
#include "ThreadSafeAddRefImpl.h"

/** The Price History API communicator request status listener. */
class CommunicatorStatusListener :
    public TThreadSafeAddRefImpl<pricehistorymgr::IPriceHistoryCommunicatorStatusListener>
{
 public:
    CommunicatorStatusListener();

    /** Returns true if the communicator is ready. */
    bool isReady();

    /** Reset error information. */
    void reset();

    /** Wait for the communicator's readiness or an error. */
    bool waitEvents();

 protected:
    /** @name IPriceHistoryCommunicatorStatusListener interface implementation */
    //@{
    virtual void onCommunicatorStatusChanged(bool ready);
    virtual void onCommunicatorInitFailed(pricehistorymgr::IError *error);
    //@}

 protected:
    virtual ~CommunicatorStatusListener();

 private:
     bool mReady;
     bool mError;

     HANDLE mSyncCommunicatorEvent;
};

