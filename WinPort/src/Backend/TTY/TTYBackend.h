#pragma once
//#define __USE_BSD 
#include <termios.h> 
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <Event.h>
#include <StackSerializer.h>
#include "Backend.h"
#include "TTYOutput.h"
#include "TTYInput.h"
#include "IFar2lInterractor.h"

class TTYBackend : IConsoleOutputBackend, ITTYInputSpecialSequenceHandler, IFar2lInterractor
{
	std::mutex _output_mutex;
	int _stdin = 0, _stdout = 1, _kickass[2] = {-1, -1};
	bool _far2l_tty = false;
	int _far2l_cursor_height = -1;
	unsigned int _cur_width = 0, _cur_height = 0;
	unsigned int _prev_width = 0, _prev_height = 0;
	std::vector<CHAR_INFO> _cur_output, _prev_output;

	struct termios _ts;
	int _ts_r = -1;
	long _terminal_size_change_id;

	pthread_t _reader_trd = 0, _writer_trd = 0;
	volatile bool _exiting = false;

	static void *sWriterThread(void *p) { ((TTYBackend *)p)->WriterThread(); return nullptr; }
	static void *sReaderThread(void *p) { ((TTYBackend *)p)->ReaderThread(); return nullptr; }

	void WriterThread();
	void ReaderThread();


	std::condition_variable _async_cond;
	std::mutex _async_mutex;

	COORD _largest_window_size;
	std::atomic<bool> _largest_window_size_ready;


	struct Far2lInterractData
	{
		Event evnt;
		StackSerializer stk_ser;
		bool waited;
	};

	struct Far2lInterractV : std::vector<std::shared_ptr<Far2lInterractData> > {} _far2l_interracts_queued;
	struct Far2lInterractsM : std::map<uint8_t, std::shared_ptr<Far2lInterractData> >, std::mutex
	{
		uint8_t _id_counter = 0;
	} _far2l_interracts_sent;

	union AsyncEvent
	{
		struct {
			bool term_resized : 1;
			bool output : 1;
			bool title_changed : 1;
			bool far2l_interract : 1;
		} flags;
		uint32_t all;
	} _ae;

	void DispatchTermResized(TTYOutput &tty_out);
	void DispatchOutput(TTYOutput &tty_out);
	void DispatchFar2lInterract(TTYOutput &tty_out);

	void OnFar2lKey(bool down, StackSerializer &stk_ser);
	void OnFar2lMouse(StackSerializer &stk_ser);

	std::shared_ptr<IClipboardBackend> _clipboard_backend;

protected:
	// IFar2lInterractor
	virtual bool Far2lInterract(StackSerializer &stk_ser, bool wait);

	// IConsoleOutputBackend
	virtual void OnConsoleOutputUpdated(const SMALL_RECT *areas, size_t count);
	virtual void OnConsoleOutputResized();
	virtual void OnConsoleOutputTitleChanged();
	virtual void OnConsoleOutputWindowMoved(bool absolute, COORD pos);
	virtual COORD OnConsoleGetLargestWindowSize();
	virtual void OnConsoleAdhocQuickEdit();
	virtual DWORD OnConsoleSetTweaks(DWORD tweaks);
	virtual void OnConsoleChangeFont();
	virtual void OnConsoleSetMaximized(bool maximized);
	virtual void OnConsoleExit();
	virtual bool OnConsoleIsActive();

	// ITTYInputSpecialSequenceHandler
	virtual void OnFar2lEvent(StackSerializer &stk_ser);
	virtual void OnFar2lReply(StackSerializer &stk_ser);
	virtual void OnInputBroken();
	virtual DWORD OnQueryControlKeys();


public:
	TTYBackend(int std_in, int std_out, bool far2l_tty);
	~TTYBackend();
	void KickAss();
	bool Startup();
};

