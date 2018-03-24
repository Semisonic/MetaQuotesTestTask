# C++ developer test task, [MetaQuotes](https://www.metaquotes.net/)

This project is my implementation of the test task that **MetaQuotes**, the maker of **MetaTrader** trading tool, offers to their **C++ developer** applicants.


# Task description
The goal is to create a log file parser class **CLogReader**, which would have a predefined interface and would satisfy a number of conditions. Also, an applicant is expected to submit a console application **test tool** to prove the correctness of *CLogReade*r's work.
## Class interface
The *CLogReader* class must have the following interface:

    class CLogReader {
    public:
        CLogReader(/* implementation-defined */);
        ~CLogReader();
    
        // opens a log file
        // return value: true - success, false - error        
        bool Open(/* implementation-defined */);                       
        
        // closes the file 
        void Close();
        
        // sets the filter string to match the file lines against
        // filter - zero-terminated string
        // return value: true - success, false - error
        bool SetFilter(const char *filter);

        // retrieves the next line which matches the filter
        // buf - buffer to store the line in
        // bufsize - buffer size (and, therefore, the maximum possible line length)
        // return value: true - success, false - EOF or error
        bool GetNextLine(char *buf, const int bufsize);
    };
## Conditions and limitations

 - The code must support and be optimized for fast processing of large (~hundreds of MBs) log files.
 - The log files contain ANSI characters only (i.e. no Unicode support required).
 - The *filter string* may contain ANSI characters only and must support such wildcards as '*?*' and '*', meaning a single arbitrary symbol and a sequence of  arbitrary symbols of any length (including zero).
 - The code must work correctly on **Windows** (with versions XP and higher) and should be compiled using Visual Studio 2005 or higher.
 - **Caching** of the search results and of the file itself is not required
 - The memory footprint of the code must be reasonably low.
 - No 3rd party libraries and components (including STL) may be used. Only the Win32 API and CRT calls are allowed.
 -  No exceptions (neither the C++ nor the Windows ones) may be used
 - The code must be as failure-proof and robust as possible.
## Implementation
The implementation transforms a filter string into a structure similar to a state machine, which is able to check strings for a match in *O(n)* operations, where *n* is the length of the input string. Therefore, all the input may be processed in linear time.

To work with large files, the code memory maps them chunk by chunk, reading them consecutively.
