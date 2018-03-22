#include "stdafx.h"
#include "LogReader.h"

// ----------------------------------------------------------------------------------------- //
/*
 *	StateMachine class

 *	state machine implementation which is used to match search expressions
 */
// ----------------------------------------------------------------------------------------- //

class StateMachine {

	friend class StateMachineFactory;
	
	/*
		SingleState struct

		corresponds to each symbol in the filter string (except for the '*' wildcards)
	 */
	
	struct SingleState {
		SingleState() = default;
		SingleState(char s, bool persistent) : symbol{ s }, isPersistent{ persistent } {}
		~SingleState() {
			if (next) {
				delete next;
			}
		}

		char symbol{ 0 };
		bool isPersistent{ false }; // persistent states denote the next non-wildcard symbol following the '*' wildcard
		SingleState* next{ nullptr };
	};
	
	/*
		StatePack struct

		is used to describe the list of currently active states
	 */
	
	struct StatePack {
		StatePack() = default;
		~StatePack() {
			if (buffer) {
				delete[] buffer;
			}
		}
		
		SingleState** buffer{nullptr};
		int count{0};
	};

public:

	enum class SymbolProcessStatus {
		SuccessSkipTheRest, MatchFailed, KeepGoing
	};

private:

	StateMachine() = default;
	
	bool build(const char* filter) {
		SingleState* currentTail{ &m_sentinelState };
		bool asteriskWildcardPending{ false };
		int simultaneousStateCount{ 0 };

		for (const char* currentSymbol = filter; *currentSymbol; ++currentSymbol) {
			switch (*currentSymbol) {
			case '?': {
				SingleState* newState = new(std::nothrow) SingleState{ 0, false };

				if (!newState) {
					return false;
				}

				currentTail = currentTail->next = newState;

				break;
			}
			case '*': {
				asteriskWildcardPending = true;

				break;
			}
			default: {
				SingleState* newState = new(std::nothrow) SingleState{ *currentSymbol, asteriskWildcardPending };

				if (asteriskWildcardPending) {
					simultaneousStateCount += 2;
				}

				asteriskWildcardPending = false;

				if (!newState) {
					return false;
				}

				currentTail = currentTail->next = newState;
			}
			}
		}

		if (asteriskWildcardPending) {
			// the special state which means that the rest of the string in question doesn't matter - it's already a success
			SingleState* newState = new(std::nothrow) SingleState{ 0, true };

			if (!newState) {
				return false;
			}

			currentTail = currentTail->next = newState;
		}
		
		if (!m_sentinelState.next) {
			// empty pattern

			return false;
		}

		if (!simultaneousStateCount) {
			simultaneousStateCount = 1;
		}
		
		for (auto& statePack : m_stateDoubleBuffer) {
			statePack.buffer = new(std::nothrow) SingleState*[simultaneousStateCount] {0};

			if (!statePack.buffer) {
				return false;
			}
		}

		// setting up the state machine with the initial state		
		m_stateDoubleBuffer[0].count = 1;
		m_stateDoubleBuffer[0].buffer[0] = m_sentinelState.next;

		return true;
	}

public:

	SymbolProcessStatus processSymbol (char symbol) {
		auto& curStepStates = m_stateDoubleBuffer[m_currentStepStates];

		if (m_currentStatus != SymbolProcessStatus::KeepGoing) {
			return m_currentStatus;
		}

		if (!curStepStates.count) {
			// no possible states available

			return (m_currentStatus = SymbolProcessStatus::MatchFailed);
		}

		m_fragileSuccess = false;

		auto& nextStepStates = m_stateDoubleBuffer[1 - m_currentStepStates];

		for (auto i = 0; i < curStepStates.count; ++i) {
			const auto& curState = curStepStates.buffer[i];
			
			if (curState->isPersistent) {
				if (!curState->symbol) {
					// we've reached the special case of the skip-the-rest state

					return (m_currentStatus = SymbolProcessStatus::SuccessSkipTheRest);
				}
				
				// an ugly hack for a scenario when two persistent states follow each other
				
				// in this case they both replicate themselves, and the first would add the second one on each turn
				// so the check below makes sure that the persistent state hasn't already been added
				// as the previous state's 'next'
				if (nextStepStates.count == 0 || nextStepStates.buffer[nextStepStates.count - 1] != curState) {
					nextStepStates.buffer[nextStepStates.count++] = curState;
				}				
			}

			if (!curState->symbol || curState->symbol == symbol) {
				if (curState->next) {
					nextStepStates.buffer[nextStepStates.count++] = curState->next;
				} else {
					// we've reached the final state, which will mean success if no symbols follow

					m_fragileSuccess = true;
				}
			}
		}

		curStepStates.count = 0;
		m_currentStepStates = 1 - m_currentStepStates;

		return m_currentStatus;
	}

	void reset() {
		m_currentStepStates = 0;
		
		m_stateDoubleBuffer[0].count = 1;
		m_stateDoubleBuffer[0].buffer[0] = m_sentinelState.next;

		m_stateDoubleBuffer[1].count = 0;

		m_currentStatus = SymbolProcessStatus::KeepGoing;
		m_fragileSuccess = false;
	}

	bool isMatchSuccessful() const {
		if ((m_currentStatus == SymbolProcessStatus::SuccessSkipTheRest) ||
			(m_currentStatus == SymbolProcessStatus::KeepGoing) && m_fragileSuccess) {
			// the basic scenario
			
			return true;
		}

		// this covers the case of the special skip-the-rest state being queued but never triggered
		// because of the end-of-line scenario

		auto& curStepStates = m_stateDoubleBuffer[m_currentStepStates];
		
		if (!curStepStates.count) {
			return false;
		}
		
		const auto& lastCurStep = *curStepStates.buffer[curStepStates.count - 1];

		return (lastCurStep.isPersistent && !lastCurStep.symbol);		
	}

private:

	SingleState m_sentinelState;
	
	StatePack m_stateDoubleBuffer[2];
	int m_currentStepStates{ 0 };

	SymbolProcessStatus m_currentStatus{ SymbolProcessStatus::KeepGoing };
	bool m_fragileSuccess{ false };
};

// ----------------------------------------------------------------------------------------- //

class StateMachineFactory {
public:

	static StateMachine* create(const char* filter) {
		StateMachine* machine = new(std::nothrow) StateMachine{};

		if (machine) {
			if (!machine->build(filter)) {
				delete machine;

				machine = nullptr;
			}
		}

		return machine;
	}
};

// ----------------------------------------------------------------------------------------- //
/*
	FileWrapper class

	handles working with files in a (hopefully) efficient way
 */
 // ----------------------------------------------------------------------------------------- //

class FileWrapper {
	
	friend class FileWrapperFactory;

	FileWrapper() = default;

	bool openFile(LPCTSTR path) {
		m_file = CreateFile(path,
							GENERIC_READ,
							FILE_SHARE_READ,
							NULL, OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
							NULL);
		if (!m_file) {
			return false;
		}

		if (!GetFileSizeEx(m_file, &m_fileSize) || !m_fileSize.QuadPart) {
			return false;
		}

		m_memoryMapping = CreateFileMapping(m_file,
											NULL,
											PAGE_READONLY,
											m_fileSize.HighPart,
											m_fileSize.LowPart,
											NULL);
		if (!m_memoryMapping) {
			return false;
		}
		
		
		return (mapNextChunk() == ChunkMapState::Success);
	}

public:

	enum class SymbolReadStatus {
		Success, EndOfLine, EndOfFile, Failure
	};

	~FileWrapper() {
		unmapCurrentChunk();

		if (m_memoryMapping) {
			CloseHandle(m_memoryMapping);
		}

		if (m_file) {
			CloseHandle(m_file);
		}
	}
	
	inline SymbolReadStatus readNextSymbol (OUT char* symbolAddress) {
		assert(symbolAddress);
		
		while (m_currentStatus == SymbolReadStatus::Success) {
			if (m_curMemoryPos < m_curMemoryEndPos) {
				char curSymbol = *(m_curMemoryPos++);

				if (curSymbol != '\r') {
					*symbolAddress = curSymbol;

					return m_currentStatus;
				} else {
					char testChar{ 0 };
					SymbolReadStatus s = readNextSymbol(&testChar);

					if (s != SymbolReadStatus::Success || testChar != '\n') {
						// checking the consistency of the Windows-style newline character sequence

						return (m_currentStatus = SymbolReadStatus::Failure);
					}

					// the commented out part is important: the end-of-line status is strictly transient
					// and doesn't get to be associated with the object itself
					return (/*m_currentStatus = */SymbolReadStatus::EndOfLine);
				}
			}

			// if we're here then the current memory chunk is depleted

			switch (switchToNextChunk()) {
			case ChunkMapState::Failure: return (m_currentStatus = SymbolReadStatus::Failure);
			case ChunkMapState::EndOfFile: return (m_currentStatus = SymbolReadStatus::EndOfFile);
			default: /* do nothing */;
			}
		}

		return m_currentStatus;
	}

	SymbolReadStatus skipCurrentLine() {
		// this method could simply call readNextSymbol() in a loop but that's suboptimal.
		// so this method replicates some of the readNextSymbol() code, make sure you keep the logic in sync
		
		while (m_currentStatus == SymbolReadStatus::Success) {
			if (m_curMemoryPos < m_curMemoryEndPos) {
				if (*(m_curMemoryPos++) != '\r') {
					continue;
				}

				char testChar{ 0 };
				SymbolReadStatus s = readNextSymbol(&testChar);

				if (s != SymbolReadStatus::Success || testChar != '\n') {
					return (m_currentStatus = SymbolReadStatus::Failure);
				}

				// this is an important difference. this method does one thing - skipping the current line till the end
				// so if it finds the end of line then it's actually success for it, hence the return value
				return (/*m_currentStatus = */SymbolReadStatus::Success);				
			}

			switch (switchToNextChunk()) {
			case ChunkMapState::Failure: return (m_currentStatus = SymbolReadStatus::Failure);
			case ChunkMapState::EndOfFile: return (m_currentStatus = SymbolReadStatus::EndOfFile);
			default: /* do nothing */;
			}
		}

		return m_currentStatus;
	}

private:

	enum class ChunkMapState {
		Success, Failure, EndOfFile
	};
	
	bool unmapCurrentChunk() {
		bool result = true;
		
		if (m_curChunkBaseAddress) {
			result = UnmapViewOfFile(m_curChunkBaseAddress) && result;

			m_curChunkBaseAddress = NULL;
			m_curMemoryPos = m_curMemoryEndPos = nullptr;
		}

		return result;
	}
	
	ChunkMapState mapNextChunk() {
		if (m_curChunkBaseAddress) {
			// the prev chunk wasn't unmapped properly

			return ChunkMapState::Failure;
		}
		
		if (m_curChunkOffset.QuadPart >= m_fileSize.QuadPart) {
			return ChunkMapState::EndOfFile;
		}

		SIZE_T chunkSize = static_cast<SIZE_T>(m_fileSize.QuadPart - m_curChunkOffset.QuadPart > getMemoryChunkSize() ? getMemoryChunkSize() : m_fileSize.QuadPart - m_curChunkOffset.QuadPart);
		
		m_curChunkBaseAddress = MapViewOfFile(m_memoryMapping,
											  FILE_MAP_READ,
											  m_curChunkOffset.HighPart,
											  m_curChunkOffset.LowPart,
											  chunkSize);
		if (!m_curChunkBaseAddress) {
			return ChunkMapState::Failure;
		}

		m_curMemoryPos = static_cast<char*>(m_curChunkBaseAddress);
		m_curMemoryEndPos = m_curMemoryPos + chunkSize;

		// moving to the next segment
		m_curChunkOffset.QuadPart += getMemoryChunkSize();

		return ChunkMapState::Success;
	}

	ChunkMapState switchToNextChunk() {
		if (!unmapCurrentChunk()) {
			return ChunkMapState::Failure;
		}

		return mapNextChunk();
	}
	
	static DWORD getMemoryChunkSize() {
		static DWORD chunkSize{ 0 };

		if (!chunkSize) {
			SYSTEM_INFO si;

			::GetNativeSystemInfo(&si);

			chunkSize = si.dwAllocationGranularity;
		}

		return chunkSize;
	}

private:

	HANDLE m_file{ NULL };
	HANDLE m_memoryMapping{ NULL };

	LARGE_INTEGER m_fileSize{ 0 };
	LARGE_INTEGER m_curChunkOffset{ 0 };

	LPVOID m_curChunkBaseAddress{ nullptr };
	char* m_curMemoryPos{ nullptr };
	char* m_curMemoryEndPos{ nullptr };

	SymbolReadStatus m_currentStatus{ SymbolReadStatus::Success };
};

// ----------------------------------------------------------------------------------------- //

class FileWrapperFactory {
public:

	static FileWrapper* create(LPCTSTR path) {
		FileWrapper* file = new(std::nothrow) FileWrapper{};

		if (file) {
			if (!file->openFile(path)) {
				delete file;

				file = nullptr;
			}
		}

		return file;
	}
};

// ----------------------------------------------------------------------------------------- //
/*
	CLogReader methods implementation
 */
// ----------------------------------------------------------------------------------------- //


CLogReader::CLogReader() {
}

// ----------------------------------------------------------------------------------------- //

CLogReader::~CLogReader() {
	if (m_stateMachine) {
		delete m_stateMachine;
	}

	if (m_fileWrapper) {
		delete m_fileWrapper;
	}
}

// ----------------------------------------------------------------------------------------- //

bool CLogReader::Open (LPCTSTR path) {
	if (m_fileWrapper) {
		// calling Open without a prior Close might be considered an error
		// but we're writing a fool-proof code so we take care of it ourselves
		Close();
	}
	
	m_fileWrapper = FileWrapperFactory::create(path);
	
	if (!m_fileWrapper) {
		return false;
	}

	return true;
}

// ----------------------------------------------------------------------------------------- //

void CLogReader::Close () {
	if (m_fileWrapper) {
		delete m_fileWrapper;

		m_fileWrapper = nullptr;
	}
}

// ----------------------------------------------------------------------------------------- //

bool CLogReader::SetFilter (const char* filter) {
	if (m_stateMachine) {
		delete m_stateMachine;

		m_stateMachine = nullptr;
	}

	return (m_stateMachine = StateMachineFactory::create(filter));	
}

// ----------------------------------------------------------------------------------------- //

bool CLogReader::GetNextLine (char* buf, const int bufsize) {
	using SymbolReadStatus = FileWrapper::SymbolReadStatus;
	using SymbolProcessStatus = StateMachine::SymbolProcessStatus;
	
	if (!m_stateMachine || !m_fileWrapper || !buf || bufsize <= 0) {
		return false;
	}

	auto readStatus{ SymbolReadStatus::Success };
	
	while (readStatus == SymbolReadStatus::Success) {
		m_stateMachine->reset();

		auto processStatus{ SymbolProcessStatus::KeepGoing };
		auto curBufferPos{ buf };
		auto bufEnd{ buf + bufsize };

		for (; curBufferPos < bufEnd; ++curBufferPos) {
			readStatus = m_fileWrapper->readNextSymbol(curBufferPos);

			if (readStatus == SymbolReadStatus::Failure) {
				return false;
			}
				
			if (readStatus == SymbolReadStatus::EndOfLine ||
				readStatus == SymbolReadStatus::EndOfFile) {
				// this might be the moment we return, but the interface has no way of returning the number of chars written
				// also we have no guarantee that the buffer passed to us is zero-initialized
				// so we put a trailing zero ourselves, just in case
				*curBufferPos = '\0';

				break;
			}
			
			if (processStatus == SymbolProcessStatus::KeepGoing) {
				processStatus = m_stateMachine->processSymbol(*curBufferPos);
			}
		
			if (processStatus == SymbolProcessStatus::MatchFailed) {
				break;
			}
		}

		// if we're here then either the line is over or it definitely can't be matched

		if (m_stateMachine->isMatchSuccessful()) {
			return true;
		}

		if (readStatus == SymbolReadStatus::EndOfFile) {
			// it's EOF and we haven't matched
			break;
		}

		if (readStatus != SymbolReadStatus::EndOfLine) {
			readStatus = m_fileWrapper->skipCurrentLine();
		} else {
			// end of line, starting over
			
			readStatus = SymbolReadStatus::Success;
		}
	}

	// if we're here then it's either EOF or a failure
	return false;
}
