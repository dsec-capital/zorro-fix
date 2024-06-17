#include "pch.h"

#include "session_status_listener.h"

SessionStatusListener::SessionStatusListener(IO2GSession *session, bool printSubsessions, const char *sessionID, const char *pin, int timeout)
{
    if (sessionID != 0)
        session_id = sessionID;
    else
        session_id = "";
    if (pin != 0)
        pin = pin;
    else
        pin = "";
    session = session;
    session->addRef();
    reset();
    print_subsessions = printSubsessions;
    mRefCount = 1;
    session_event = CreateEvent(0, FALSE, FALSE, 0);
    timeout = timeout;
    last_result = 0;
}

SessionStatusListener::~SessionStatusListener()
{
    session->release();
    session_id.clear();
    pin.clear();
    CloseHandle(session_event);
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
    connected = false;
    disconnected = false;
    last_result = 0;
}

void SessionStatusListener::onLoginFailed(const char *error)
{
    spdlog::error("SessionStatusListener::onLoginFailed: error {}", error);
    SetEvent(session_event);
}

void SessionStatusListener::onSessionStatusChanged(IO2GSessionStatus::O2GSessionStatus status)
{
    switch (status)
    {
    case IO2GSessionStatus::Disconnected:
        spdlog::debug("SessionStatusListener::onSessionStatusChanged: IO2GSessionStatus::Disconnected");
        connected = false;
        disconnected = true;
        SetEvent(session_event);
        break;

    case IO2GSessionStatus::Connecting:
        spdlog::debug("SessionStatusListener::onSessionStatusChanged: IO2GSessionStatus::Connecting");
        break;

    case IO2GSessionStatus::TradingSessionRequested: 
        {
            spdlog::debug("SessionStatusListener::onSessionStatusChanged: IO2GSessionStatus::TradingSessionRequested");
            O2G2Ptr<IO2GSessionDescriptorCollection> descriptors = session->getTradingSessionDescriptors();
            bool found = false;
            if (descriptors)
            {
                std::stringstream strBuf;
                if (print_subsessions) {
                    strBuf << "SessionStatusListener::onSessionStatusChanged: descriptors available:" << std::endl;
                }
                
                for (int i = 0; i < descriptors->size(); ++i)
                {
                    O2G2Ptr<IO2GSessionDescriptor> descriptor = descriptors->get(i);
                    if (print_subsessions)
                        strBuf << "  id:='" << descriptor->getID()
                               << "' name='" << descriptor->getName()
                               << "' description='" << descriptor->getDescription()
                               << "' " << (descriptor->requiresPin() ? "requires pin" : "") << std::endl;
                    if (session_id == descriptor->getID())
                    {
                        found = true;
                        break;
                    }
                }

                if (print_subsessions) {
                    spdlog::info(strBuf.str());
                }
            }
            if (!found)
            {
                onLoginFailed("specified sub session identifier is not found");
            }
            else
            {
                session->setTradingSession(session_id.c_str(), pin.c_str());
            }
        }
        break;

    case IO2GSessionStatus::Connected:
        spdlog::info("SessionStatusListener::onSessionStatusChanged: IO2GSessionStatus::Connected");
        connected = true;
        disconnected = false;
        SetEvent(session_event);
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

bool SessionStatusListener::is_connected() const
{
    return connected;
}

bool SessionStatusListener::is_disconnected() const
{
    return disconnected;
}

bool SessionStatusListener::wait_events(DWORD t)
{
    last_result = WaitForSingleObject(session_event, t > 0 ? t : timeout);
    return last_result == 0;
}

