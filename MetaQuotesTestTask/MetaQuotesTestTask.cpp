// MetaQuotesTestTask.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "LogReader.h"


int wmain (int argc, wchar_t* argv[]) {
	if (argc < 3 || argc > 4) {
		printf("Usage: <app name> <path to file to scan, may have Unicode symbols> <filter string, ANSI only> [<max length of line within the file>]\n");

		return 0;
	}

	// the main star of this show
	CLogReader logReader{};

	if (!logReader.Open(argv[1])) {
		printf("Failed to open the file, please check if it exists or fix the input\n");

		return 1;
	}
	
	char* filter{ nullptr };
	
	{
		// we have to convert the wide char representation of filter to plain 1-byte char string
		auto filterLength = lstrlenW(argv[2]);

		filter = new(std::nothrow) char[filterLength + 1];

		if (!filter) {
			printf("Memory shortage, aborting...\n");

			return 1;
		}

		for (auto i = 0; i < filterLength; ++i) {
			wchar_t wc = argv[2][i];
			char c = static_cast<char>(wc);

			if (wc != c) {
				printf("Filter string contains non-ANSI characters, please fix the input\n");

				// we're exiting the process, that's why cleaning the memory buffer previously allocated isn't strictly necessary
				return 1;
			}

			filter[i] = c;
		}

		filter[filterLength] = '\0';
	}

	if (!logReader.SetFilter(filter)) {
		printf("Failed to process the filter string, please fix the input\n");

		return 1;
	}

	auto stringBufferLength{ 1024 }; // the assumed max length of the file lines

	if (argc == 4) {
		// trying to employ the user-submitted string length

		auto suggestedBufferLength = wcstoul(argv[3], nullptr, 10);

		if (!suggestedBufferLength) {
			printf("Argument 3 is not a number, please fix the input\n");

			return 1;
		}

		if (suggestedBufferLength > INT_MAX) {
			printf("Value of argument 3 is too large, please fix the input or omit this argument to use the default value\n");

			return 1;
		}

		stringBufferLength = static_cast<decltype(stringBufferLength)>(suggestedBufferLength);
	}

	char* stringBuffer = new(std::nothrow) char[stringBufferLength + 1];

	if (!stringBuffer) {
		printf("Memory shortage, aborting...\n");

		return 1;
	}

	printf("================= Matches found =================\n");

	// let the fun begin
	while (logReader.GetNextLine(stringBuffer, stringBufferLength)) {
		printf("%s\n", stringBuffer);
	}
	
	return 0;
}

