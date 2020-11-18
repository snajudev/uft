// -----------------------------------------------------------------------------
// Written by: F. Barney
// Date: 11/17/2020
// -----------------------------------------------------------------------------

#ifndef CMDLINEARGS_HPP
#define CMDLINEARGS_HPP

#include <string>
#include <utility>
#include <unordered_map>

#include <string.h>

typedef void(*CmdLineArgsOnKeyNotFound)(const std::string& key);

class CmdLineArgs
{
	std::unordered_map<std::string, std::string> args;

public:
	explicit CmdLineArgs(int argc, char* argv[])
	{
		for (int i = 0; i < argc; ++i)
		{
			auto arg = argv[i];
			
			if (auto argLength = strlen(arg))
			{
				if (argLength >= 2)
				{
					for (size_t j = 1; j < argLength; ++j)
					{
						if (arg[j] == '=')
						{
							std::string argName(
								&arg[2],
								j - 2
							);

							auto argValueLength = argLength - (j + 1);

							std::string argValue(
								&arg[j + 1],
								argValueLength
							);

							if ((argValue[0] == '"') &&
								(argValue[argValueLength] == '"'))
							{
								argValue = argValue.substr(
									1,
									argValueLength - 2
								);
							}

							args.emplace(
								std::move(argName),
								std::move(argValue)
							);
						}
					}
				}
			}
		}
	}

	virtual ~CmdLineArgs()
	{
	}

	std::size_t GetCount() const
	{
		return args.size();
	}

	template<typename T>
	bool TryGetValue(const std::string& key, T& value) const
	{
		CmdLineArgsOnKeyNotFound onKeyNotFound(
			[](const std::string&)
			{
			}
		);

		return TryGetValue<T>(
			key,
			value,
			onKeyNotFound
		);
	}
	template<typename T>
	bool TryGetValue(const std::string& key, T& value, CmdLineArgsOnKeyNotFound onKeyNotFound) const
	{
		auto it = args.find(
			key
		);

		if (it == args.end())
		{
			onKeyNotFound(
				key
			);

			return false;
		}

		value = GetValueFromString<T>(
			it->second
		);

		return true;
	}

private:
	template<typename T>
	static T GetValueFromString(const std::string& string);
};

template<>
inline std::string CmdLineArgs::GetValueFromString(const std::string& string)
{
	return string;
}
template<>
inline std::uint16_t CmdLineArgs::GetValueFromString(const std::string& string)
{
	return static_cast<std::uint16_t>(
		std::strtoul(string.c_str(), nullptr, 10)
	);
}
template<>
inline std::uint32_t CmdLineArgs::GetValueFromString(const std::string& string)
{
	return static_cast<std::uint32_t>(
		std::strtoul(string.c_str(), nullptr, 10)
	);
}

#endif // !CMDLINEARGS_HPP
