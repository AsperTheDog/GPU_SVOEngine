#pragma once
#include <iostream>
#include <vector>
#include <sstream>

class Logger
{
public:
	static void setRootContext(std::string_view context);

	static void pushContext(std::string_view context);
	static void popContext();

	static void print(std::string_view message);

private:

	inline static std::vector<std::string> m_contexts{};
	inline static std::string m_rootContext = "ROOT";

	Logger() = default;
};

inline void Logger::setRootContext(const std::string_view context)
{
	m_rootContext = context;
}

inline void Logger::pushContext(const std::string_view context)
{
	const std::string contextStr = static_cast<std::string>(context);
	m_contexts.push_back(contextStr);
}

inline void Logger::popContext()
{
	const std::string contextStr = m_contexts.back();
	m_contexts.pop_back();
}

inline void Logger::print(const std::string_view message)
{
	std::stringstream context;
	if (!m_contexts.empty())
	{
		for (uint32_t i = 0; i < m_contexts.size(); ++i)
		{
			context << "  ";
		}
		context << "[" << m_contexts.back() << "]: ";
	}
	else
	{
		context << "[" << m_rootContext << "]: ";
	}

	std::cout << context.str() << message << '\n';
}
