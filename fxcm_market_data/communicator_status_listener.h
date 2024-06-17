#pragma once
#include "thread_safe_add_ref_impl.h"

/*
 * Price History API communicator request status listener
 */
class CommunicatorStatusListener : public TThreadSafeAddRefImpl<pricehistorymgr::IPriceHistoryCommunicatorStatusListener>
{
 public:
    CommunicatorStatusListener();

    // Returns true if the communicator is ready 
    bool is_ready();

    // Reset error information 
    void reset();

    // Wait for the communicator's readiness or an error 
    bool wait_events();

 protected:
    // IPriceHistoryCommunicatorStatusListener interface implementation 
    virtual void onCommunicatorStatusChanged(bool ready);

    virtual void onCommunicatorInitFailed(pricehistorymgr::IError *error);


 protected:
    virtual ~CommunicatorStatusListener();

 private:
     bool ready;
     bool has_error;

     HANDLE sync_communicator_event;
};

