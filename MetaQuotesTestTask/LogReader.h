#pragma once

class StateMachine;
class FileWrapper;

class CLogReader {
public:
	
	CLogReader ();
	~CLogReader ();

	bool Open (LPCTSTR path);
	void Close();

	bool SetFilter (const char *filter);
	bool GetNextLine (char *buf, const int bufsize);

private:

	StateMachine* m_stateMachine { nullptr };
	FileWrapper* m_fileWrapper{ nullptr };
};

