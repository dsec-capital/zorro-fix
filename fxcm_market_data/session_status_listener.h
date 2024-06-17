#pragma once

class SessionStatusListener : public IO2GSessionStatus
{
 private:
    volatile unsigned int mRefCount;
    std::string session_id;
    std::string pin;
    bool connected;
    bool disconnected;
    bool print_subsessions;
    IO2GSession *session;
    HANDLE session_event;
    DWORD timeout;  // in milliseconds
    DWORD last_result;

 protected:
    ~SessionStatusListener();

 public:
    /** Constructor.
        @param session          Session to listen to.
        @param printSubsessions To print subsessions or not.
        @param sessionID        Identifier of the subsession or NULL in case
                                no subsession selector is expected.
        @param pin              Pin code or NULL in case no pin code request is expected.
        @param timeout          Timeout in milliseconds
    */
    SessionStatusListener(IO2GSession *session, bool print_subsessions, const char *session_id = 0, const char *pin = 0, int timeout = 10000);

    // Increase reference counter 
    virtual long addRef();

    // Decrease reference counter  
    virtual long release();

    // Callback called when login has been failed
    virtual void onLoginFailed(const char *error);

    // Callback called when session status has been changed
    virtual void onSessionStatusChanged(IO2GSessionStatus::O2GSessionStatus status);

    // Check whether session is connected
    bool is_connected() const;

    // Check whether session is disconnected
    bool is_disconnected() const;

    // Reset error information (use before login/logout)
    void reset();

    // Wait for connection or error, timeout in millis, returns true if wait is successful 
    // i.e. if result returned from WaitForSingleObject is 0, otherwise false and result in last_result
    bool wait_events(DWORD timeout = 0);
};

