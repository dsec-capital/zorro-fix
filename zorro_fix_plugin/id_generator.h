#ifndef IDGENERATOR_H
#define IDGENERATOR_H

#include <string>
#include <sstream>

class IDGenerator
{
public:
	IDGenerator() : m_orderID(0), m_executionID(0), m_ID(0) {}

	std::string genOrderID()
	{
		return std::to_string(++m_orderID);
	}

	std::string genExecutionID()
	{
		return std::to_string(++m_executionID);
	}

	std::string genID()
	{
		return std::to_string(++m_ID);
	}

private:
	long m_orderID;
	long m_executionID;
	long m_ID;
};

#endif