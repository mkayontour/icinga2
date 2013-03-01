/******************************************************************************
 * Icinga 2                                                                   *
 * Copyright (C) 2012 Icinga Development Team (http://www.icinga.org/)        *
 *                                                                            *
 * This program is free software; you can redistribute it and/or              *
 * modify it under the terms of the GNU General Public License                *
 * as published by the Free Software Foundation; either version 2             *
 * of the License, or (at your option) any later version.                     *
 *                                                                            *
 * This program is distributed in the hope that it will be useful,            *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with this program; if not, write to the Free Software Foundation     *
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.             *
 ******************************************************************************/

#include "i2-base.h"

using namespace icinga;

REGISTER_TYPE(Logger);

/**
 * Constructor for the Logger class.
 *
 * @param properties A serialized dictionary containing attributes.
 */
Logger::Logger(const Dictionary::Ptr& properties)
	: DynamicObject(properties)
{
	RegisterAttribute("type", Attribute_Config, &m_Type);
	RegisterAttribute("path", Attribute_Config, &m_Path);
	RegisterAttribute("severity", Attribute_Config, &m_Severity);

	if (!IsLocal())
		BOOST_THROW_EXCEPTION(runtime_error("Logger objects must be local."));
}

void Logger::Start(void)
{
	String type = m_Type;
	if (type.IsEmpty())
		BOOST_THROW_EXCEPTION(runtime_error("Logger objects must have a 'type' property."));

	ILogger::Ptr impl;

	if (type == "syslog") {
#ifndef _WIN32
		impl = boost::make_shared<SyslogLogger>();
#else /* _WIN32 */
		BOOST_THROW_EXCEPTION(invalid_argument("Syslog is not supported on Windows."));
#endif /* _WIN32 */
	} else if (type == "file") {
		String path = m_Path;
		if (path.IsEmpty())
			BOOST_THROW_EXCEPTION(invalid_argument("'log' object of type 'file' must have a 'path' property"));

		StreamLogger::Ptr slogger = boost::make_shared<StreamLogger>();
		slogger->OpenFile(path);

		impl = slogger;
	} else if (type == "console") {
		impl = boost::make_shared<StreamLogger>(&std::cout);
	} else {
		BOOST_THROW_EXCEPTION(runtime_error("Unknown log type: " + type));
	}

	impl->m_Config = GetSelf();
	m_Impl = impl;

}

/**
 * Writes a message to the application's log.
 *
 * @param severity The message severity.
 * @param facility The log facility.
 * @param message The message.
 */
void Logger::Write(LogSeverity severity, const String& facility,
    const String& message)
{
	LogEntry entry;
	entry.Timestamp = Utility::GetTime();
	entry.Severity = severity;
	entry.Facility = facility;
	entry.Message = message;

	ForwardLogEntry(entry);
}

/**
 * Retrieves the minimum severity for this logger.
 *
 * @returns The minimum severity.
 */
LogSeverity Logger::GetMinSeverity(void) const
{
	String severity = m_Severity;
	if (severity.IsEmpty())
		return LogInformation;
	else
		return Logger::StringToSeverity(severity);
}

/**
 * Forwards a log entry to the registered loggers.
 *
 * @param entry The log entry.
 */
void Logger::ForwardLogEntry(const LogEntry& entry)
{
	bool processed = false;

	BOOST_FOREACH(const DynamicObject::Ptr& object, DynamicType::GetObjects("Logger")) {
		Logger::Ptr logger = dynamic_pointer_cast<Logger>(object);

		{
			ObjectLock llock(logger);

			if (entry.Severity >= logger->GetMinSeverity())
				logger->m_Impl->ProcessLogEntry(entry);
		}

		processed = true;
	}

	LogSeverity defaultLogLevel;

	if (Application::IsDebugging())
		defaultLogLevel = LogDebug;
	else
		defaultLogLevel = LogInformation;

	if (!processed && entry.Severity >= defaultLogLevel) {
		static bool tty = StreamLogger::IsTty(std::cout);

		StreamLogger::ProcessLogEntry(std::cout, tty, entry);
	}
}

/**
 * Converts a severity enum value to a string.
 *
 * @param severity The severity value.
 */
String Logger::SeverityToString(LogSeverity severity)
{
	switch (severity) {
		case LogDebug:
			return "debug";
		case LogInformation:
			return "information";
		case LogWarning:
			return "warning";
		case LogCritical:
			return "critical";
		default:
			BOOST_THROW_EXCEPTION(invalid_argument("Invalid severity."));
	}
}

/**
 * Converts a string to a severity enum value.
 *
 * @param severity The severity.
 */
LogSeverity Logger::StringToSeverity(const String& severity)
{
	if (severity == "debug")
		return LogDebug;
	else if (severity == "information")
		return LogInformation;
	else if (severity == "warning")
		return LogWarning;
	else if (severity == "critical")
		return LogCritical;
	else
		BOOST_THROW_EXCEPTION(invalid_argument("Invalid severity: " + severity));
}

/**
 * Retrieves the configuration object that belongs to this logger.
 *
 * @returns The configuration object.
 */
DynamicObject::Ptr ILogger::GetConfig(void) const
{
	return m_Config.lock();
}
