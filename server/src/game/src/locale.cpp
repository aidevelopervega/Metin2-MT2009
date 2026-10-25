#include "stdafx.h"
#include "locale_service.h"
#include <vector>

typedef std::map< std::string, std::string > LocaleStringMapType;

LocaleStringMapType localeString;

int g_iUseLocale = 0;

static bool locale_extract_format_specs(const char* str, std::vector<std::string>& outSpecs)
{
	outSpecs.clear();
	if (!str)
		return false;

	for (const char* p = str; *p != '\0'; ++p)
	{
		if (*p != '%')
			continue;
		++p;
		if (*p == '\0')
			return false; // dangling '%' at end of format string
		if (*p == '%')
			continue; // escaped '%%'

		// Skip flags
		while (*p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0')
			++p;

		// Dynamic width '*' not allowed in static locale strings
		if (*p == '*')
			return false;
		while (*p >= '0' && *p <= '9')
			++p;

		// Precision
		if (*p == '.')
		{
			++p;
			if (*p == '*')
				return false;
			while (*p >= '0' && *p <= '9')
				++p;
		}

		// Length modifier
		std::string spec;
		if (*p == 'h' || *p == 'l')
		{
			char firstMod = *p++;
			spec.push_back(firstMod);
			if (*p == firstMod)
				spec.push_back(*p++);
		}
		else if (*p == 'z' || *p == 'j' || *p == 't' || *p == 'L')
		{
			spec.push_back(*p++);
		}

		if (*p == '\0')
			return false;

		// Reject %n memory write specifier unconditionally
		if (*p == 'n')
			return false;

		switch (*p)
		{
			case 'd':
			case 'i':
				spec.push_back('d');
				break;
			case 'u':
			case 'x':
			case 'X':
			case 'o':
				spec.push_back('u');
				break;
			case 'f':
			case 'F':
			case 'e':
			case 'E':
			case 'g':
			case 'G':
				spec.push_back('f');
				break;
			case 's':
			case 'c':
			case 'p':
				spec.push_back(*p);
				break;
			default:
				return false;
		}
		outSpecs.push_back(spec);
	}
	return true;
}

static bool locale_is_safe_translation(const char* key, const char* translated)
{
	if (!key || !translated)
		return false;

	std::vector<std::string> keySpecs;
	std::vector<std::string> transSpecs;
	if (!locale_extract_format_specs(key, keySpecs))
		return false;
	if (!locale_extract_format_specs(translated, transSpecs))
		return false;

	// If the source key explicitly specifies printf placeholders, the translation
	// must not demand more placeholders than provided or incompatible types.
	if (!keySpecs.empty())
	{
		if (transSpecs.size() > keySpecs.size())
			return false;
		for (size_t i = 0; i < transSpecs.size(); ++i)
		{
			if (transSpecs[i] != keySpecs[i])
				return false;
		}
	}
	return true;
}

void locale_add(const char **strings)
{
	if (!strings || !strings[0] || !strings[1])
		return;

	if (!locale_is_safe_translation(strings[0], strings[1]))
	{
		sys_err("LOCALE_FORMAT_ERROR: unsafe format in translation for key \"%s\" -> \"%s\"", strings[0], strings[1]);
		return;
	}

	LocaleStringMapType::const_iterator iter = localeString.find( strings[0] );

	if( iter == localeString.end() )
	{
		localeString.emplace(strings[0], strings[1]);
	}
}

const char * locale_find(const char *string)
{
	if (!string)
		return "";

	if (0 == g_iUseLocale || LC_IsKorea() || LC_IsWE_Korea())
	{
		return (string);
	}

	LocaleStringMapType::const_iterator iter = localeString.find( string );

	if( iter == localeString.end() )
	{
		static char s_line[1024] = "@0949";
		strlcpy(s_line + 5, string, sizeof(s_line) - 5);
		for (char* p = s_line + 5; *p != '\0'; ++p)
		{
			if (*p == '%')
				*p = '#';
		}

		sys_err("LOCALE_ERROR: \"%s\";", string);
		return s_line;
	}

	return iter->second.c_str();
}

const char *quote_find_end(const char *string)
{
	const char  *tmp = string;
	int         quote = 0;

	while (*tmp)
	{
		if (quote && *tmp == '\\' && *(tmp + 1))
		{
			switch (*(tmp + 1))
			{
				case '"':
					tmp += 2;
					continue;
			}
		}
		else if (*tmp == '"')
		{
			quote = !quote;
		}
		else if (!quote && *tmp == ';')
			return (tmp);

		tmp++;
	}

	return (NULL);
}

char *locale_convert(const char *src, int len)
{
	const char	*tmp;
	int		i, j;
	char	*buf, *dest;
	int		start = 0;
	char	last_char = 0;

	if (len <= 0)
		return NULL;

	buf = M2_NEW char[len + 1];

	for (j = i = 0, tmp = src, dest = buf; i < len; i++, tmp++)
	{
		if (*tmp == '"')
		{
			if (last_char != '\\')
				start = !start;
			else
				goto ENCODE;
		}
		else if (*tmp == ';')
		{
			if (last_char != '\\' && !start)
				break;
			else
				goto ENCODE;
		}
		else if (start)
		{
ENCODE:
			if (*tmp == '\\' && *(tmp + 1) == 'n')
			{
				*(dest++) = '\n';
				tmp++;
				last_char = '\n';
			}
			else
			{
				*(dest++) = *tmp;
				last_char = *tmp;
			}

			j++;
		}
	}

	if (!j)
	{
		M2_DELETE_ARRAY(buf);
		return NULL;
	}

	*dest = '\0';
	return (buf);
}

#define NUM_LOCALES 2

void locale_init(const char *filename)
{
	FILE        *fp = fopen(filename, "rb");
	char        *buf;

	if (!fp) return;

	fseek(fp, 0L, SEEK_END);
	long fileLen = ftell(fp);
	fseek(fp, 0L, SEEK_SET);

	if (fileLen <= 0 || fileLen >= INT32_MAX)
	{
		fclose(fp);
		return;
	}

	size_t allocSize = static_cast<size_t>(fileLen) + 1;

	buf = M2_NEW char[allocSize];

	memset(buf, 0, allocSize);

	fread(buf, static_cast<size_t>(fileLen), sizeof(char), fp);

	fclose(fp);

	const char * tmp;
	const char * end;

	char *	strings[NUM_LOCALES];

	if (!buf)
	{
		sys_err("locale_read: no file %s", filename);
		exit(1);
	}

	tmp = buf;

	do
	{
		for (int i = 0; i < NUM_LOCALES; i++)
			strings[i] = NULL;

		if (*tmp == '"')
		{
			for (int i = 0; i < NUM_LOCALES; i++)
			{
				if (!(end = quote_find_end(tmp)))
					break;

				const ptrdiff_t spanLen = end - tmp;
				strings[i] = (spanLen > 0 && spanLen <= INT32_MAX)
					? locale_convert(tmp, static_cast<int>(spanLen))
					: NULL;
				tmp = ++end;

				while (*tmp == '\n' || *tmp == '\r' || *tmp == ' ') tmp++;

				if (i + 1 == NUM_LOCALES)
					break;

				if (*tmp != '"')
				{
					sys_err("locale_init: invalid format filename %s", filename);
					break;
				}
			}

			if (strings[0] == NULL || strings[1] == NULL)
			{
				for (int i = 0; i < NUM_LOCALES; i++)
					if (strings[i])
						M2_DELETE_ARRAY(strings[i]);
				break;
			}

			locale_add((const char**)strings);

			for (int i = 0; i < NUM_LOCALES; i++)
				if (strings[i])
					M2_DELETE_ARRAY(strings[i]);
		}
		else
		{
			tmp = strchr(tmp, '\n');

			if (tmp)
				tmp++;
		}
	}
	while (tmp && *tmp);

	M2_DELETE_ARRAY(buf);
}

