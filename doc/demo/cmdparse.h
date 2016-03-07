#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>


namespace cmdparse {

/// An exception occuring during parsing of a command line
class CommandLineParserException: public std::runtime_error
{
public:
    explicit CommandLineParserException(const char* message)
        : std::runtime_error(message)
    {}

    explicit CommandLineParserException(const std::string& message)
        : std::runtime_error(message)
    {}
};


namespace detail {
    template<typename T>
    T parseValue(const std::string& s) {
        std::istringstream in(s);
        T value;
        if (!(in >> value))
            throw CommandLineParserException(std::string("Couldn't parse value ") + s);
        return value;
    }

    template<>
    std::string parseValue<std::string>(const std::string& s) {
        return s;
    }
}


/// Parses command line arguments into a structure of type Config
template<typename Config>
class CommandLineParser
{
public:
    /// Register a command line option. Returns the same instance of CommandLineParser,
    /// for 'fluent interface'
    template<typename T>
    CommandLineParser& add(const std::string& optionName, T Config::* field)
    {
        validateOptionName(optionName);
        optionHandlers[optionName] = makeOptionHandler(optionName, field);
        return *this;
    }

    /// Parse command line arguments given in the range [first; last)
    template<typename ArgsIterator>
    Config parse(ArgsIterator first, ArgsIterator last) {
        config = Config();

        args.clear();
        std::copy(first, last, std::back_inserter(args));

        for (argsIterator = args.begin(); argsIterator != args.end(); ++argsIterator) {
            const auto& optionName = *argsIterator;

            auto handler = optionHandlers.find(optionName);
            if (handler == optionHandlers.end())
                throw CommandLineParserException(std::string("Invalid option ") + optionName);

            handler->second();
        }

        return config;
    }

private:
    template<typename T>
    std::function<void()> makeOptionHandler(const std::string& optionName, T Config::* field) {
        return [this, optionName, field]() {
            ++argsIterator;
            if (argsIterator == args.end())
                throw CommandLineParserException(std::string("Option value required for ") + optionName);
            config.*field = detail::parseValue<T>(*argsIterator);
        };
    }

    std::function<void()> makeOptionHandler(const std::string& /*optionName*/, bool Config::* field) {
        return [this, field]() {
            config.*field = true;
        };
    }

    void validateOptionName(const std::string& optionName) {
        if (optionHandlers.count(optionName))
            throw CommandLineParserException(std::string("Duplicate option definition ") + optionName);

        if (optionName.empty() || optionName[0] != '-')
            throw CommandLineParserException(std::string("Option name must begin with '-': ") + optionName);
    }

    Config config;

    std::vector<std::string> args;
    std::vector<std::string>::const_iterator argsIterator;
    std::map<std::string, std::function<void()>> optionHandlers;
};

} // namespace cmdparse

