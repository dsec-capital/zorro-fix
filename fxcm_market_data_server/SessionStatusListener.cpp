#include "SessionStatusListener.h"

constexpr auto _TIMEOUT = 30000;

#include "spdlog/spdlog.h"

/** Constructor. */
SessionStatusListener::SessionStatusListener(IO2GSession *session, bool printSubsessions, const char *sessionID, const char *pin)
{
    if (sessionID != 0)
        mSessionID = sessionID;
    else
        mSessionID = "";
    if (pin != 0)
        mPin = pin;
    else
        mPin = "";
    mSession = session;
    mSession->addRef();
    reset();
    mPrintSubsessions = printSubsessions;
    mRefCount = 1;
    mSessionEvent = CreateEvent(0, FALSE, FALSE, 0);
}

/** Destructor. */
SessionStatusListener::~SessionStatusListener()
{
    mSession->release();
    mSessionID.clear();
    mPin.clear();
    CloseHandle(mSessionEvent);
}

/** Increase reference counter. */
long SessionStatusListener::addRef()
{
    return O2GAtomic::InterlockedInc(mRefCount);
}

/** Decrease reference counter. */
long SessionStatusListener::release()
{
    long rc = O2GAtomic::InterlockedDec(mRefCount);
    if (rc == 0)
        delete this;
    return rc;
}

void SessionStatusListener::reset()
{
    mConnected = false;
    mDisconnected = false;
}

/** Callback called when login has been failed. */
void SessionStatusListener::onLoginFailed(const char *error)
{
    spdlog::error("login error: {}", error );
    SetEvent(mSessionEvent);
}

/** Callback called when session status has been changed. */
void SessionStatusListener::onSessionStatusChanged(IO2GSessionStatus::O2GSessionStatus status)
{
    switch (status)
    {
    case IO2GSessionStatus::Disconnected:
        spdlog::info("session status disconnected");
        mConnected = false;
        mDisconnected = true;
        SetEvent(mSessionEvent);
        break;
    case IO2GSessionStatus::Connecting:
        spdlog::info("session status connecting");
        break;
    case IO2GSessionStatus::TradingSessionRequested:
    {
        spdlog::info("session status trading session requested");
        O2G2Ptr<IO2GSessionDescriptorCollection> descriptors = mSession->getTradingSessionDescriptors();
        bool found = false;
        if (descriptors)
        {
            std::stringstream ss;
            if (mPrintSubsessions)
                ss << "descriptors available:" << std::endl;
            else
                ss << "no descriptors available" << std::endl;
            for (int i = 0; i < descriptors->size(); ++i)
            {
                O2G2Ptr<IO2GSessionDescriptor> descriptor = descriptors->get(i);
                if (mPrintSubsessions)
                    ss << "  id:='" << descriptor->getID()
                       << "' name='" << descriptor->getName()
                       << "' description='" << descriptor->getDescription()
                       << "' " << (descriptor->requiresPin() ? "requires pin" : "") << std::endl;
                if (mSessionID == descriptor->getID())
                {
                    found = true;
                    break;
                }
            }
            spdlog::info(ss.str());
        }
        if (!found)
        {
            onLoginFailed("The specified sub session identifier is not found");
        }
        else
        {
            mSession->setTradingSession(mSessionID.c_str(), mPin.c_str());
        }
    }
    break;
    case IO2GSessionStatus::Connected:
        spdlog::info("session status connected");
        mConnected = true;
        mDisconnected = false;
        SetEvent(mSessionEvent);
        break;
    case IO2GSessionStatus::Reconnecting:
        spdlog::info("session status reconnecting");
        break;
    case IO2GSessionStatus::Disconnecting:
        spdlog::info("session status disconnecting");
        break;
    case IO2GSessionStatus::SessionLost:
        spdlog::info("session status session lost");
        break;
    }
}

/** Check whether session is connected */
bool SessionStatusListener::isConnected() const
{
    return mConnected;
}

/** Check whether session is disconnected */
bool SessionStatusListener::isDisconnected() const
{
    return mDisconnected;
}

/** Wait for connection or error. */
bool SessionStatusListener::waitEvents()
{
    return WaitForSingleObject(mSessionEvent, _TIMEOUT) == 0;
}

