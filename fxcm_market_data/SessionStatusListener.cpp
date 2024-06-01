#include "pch.h"

#include "SessionStatusListener.h"

SessionStatusListener::SessionStatusListener(IO2GSession *session, bool printSubsessions, const char *sessionID, const char *pin, int timeout)
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
    mTimeout = timeout;
}

SessionStatusListener::~SessionStatusListener()
{
    mSession->release();
    mSessionID.clear();
    mPin.clear();
    CloseHandle(mSessionEvent);
}

long SessionStatusListener::addRef()
{
    return O2GAtomic::InterlockedInc(mRefCount);
}

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

void SessionStatusListener::onLoginFailed(const char *error)
{
    spdlog::error("SessionStatusListener::onLoginFailed: error {}", error);
    SetEvent(mSessionEvent);
}

void SessionStatusListener::onSessionStatusChanged(IO2GSessionStatus::O2GSessionStatus status)
{
    switch (status)
    {
    case IO2GSessionStatus::Disconnected:
        spdlog::debug("SessionStatusListener::onSessionStatusChanged: IO2GSessionStatus::Disconnected");
        mConnected = false;
        mDisconnected = true;
        SetEvent(mSessionEvent);
        break;

    case IO2GSessionStatus::Connecting:
        spdlog::debug("SessionStatusListener::onSessionStatusChanged: IO2GSessionStatus::Connecting");
        break;

    case IO2GSessionStatus::TradingSessionRequested: 
        {
            spdlog::debug("SessionStatusListener::onSessionStatusChanged: IO2GSessionStatus::TradingSessionRequested");
            O2G2Ptr<IO2GSessionDescriptorCollection> descriptors = mSession->getTradingSessionDescriptors();
            bool found = false;
            if (descriptors)
            {
                std::stringstream strBuf;
                if (mPrintSubsessions) {
                    strBuf << "SessionStatusListener::onSessionStatusChanged: descriptors available:" << std::endl;
                }
                
                for (int i = 0; i < descriptors->size(); ++i)
                {
                    O2G2Ptr<IO2GSessionDescriptor> descriptor = descriptors->get(i);
                    if (mPrintSubsessions)
                        strBuf << "  id:='" << descriptor->getID()
                               << "' name='" << descriptor->getName()
                               << "' description='" << descriptor->getDescription()
                               << "' " << (descriptor->requiresPin() ? "requires pin" : "") << std::endl;
                    if (mSessionID == descriptor->getID())
                    {
                        found = true;
                        break;
                    }
                }

                if (mPrintSubsessions) {
                    spdlog::info(strBuf.str());
                }
            }
            if (!found)
            {
                onLoginFailed("specified sub session identifier is not found");
            }
            else
            {
                mSession->setTradingSession(mSessionID.c_str(), mPin.c_str());
            }
        }
        break;

    case IO2GSessionStatus::Connected:
        spdlog::info("SessionStatusListener::onSessionStatusChanged: IO2GSessionStatus::Connected");
        mConnected = true;
        mDisconnected = false;
        SetEvent(mSessionEvent);
        break;

    case IO2GSessionStatus::Reconnecting:
        spdlog::info("SessionStatusListener::onSessionStatusChanged: IO2GSessionStatus::Reconnecting");
        break;

    case IO2GSessionStatus::Disconnecting:
        spdlog::info("SessionStatusListener::onSessionStatusChanged: IO2GSessionStatus::Disconnecting");
        break;

    case IO2GSessionStatus::SessionLost:
        spdlog::info("SessionStatusListener::onSessionStatusChanged: IO2GSessionStatus::SessionLost");
        break;

    }
}

bool SessionStatusListener::isConnected() const
{
    return mConnected;
}

bool SessionStatusListener::isDisconnected() const
{
    return mDisconnected;
}

bool SessionStatusListener::waitEvents()
{
    return WaitForSingleObject(mSessionEvent, mTimeout) == 0;
}

