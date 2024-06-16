//////////////////////////////////////////////////////////////////////////////
///                                                                        ///
/// This file is part of the VDR mpv plugin and licensed under AGPLv3      ///
///                                                                        ///
/// See the README file for copyright information                          ///
///                                                                        ///
//////////////////////////////////////////////////////////////////////////////

#include <vdr/interface.h>
#include <vdr/plugin.h>
#include <vdr/player.h>

extern "C"
{
#include <stdint.h>
}


/**
**	remote class.
*/
class cSoftRemote:public cRemote, private cThread
{
  private:
    cMutex mutex;
    cCondVar keyReceived;
    cString Command;
    virtual void Action(void);
  public:

    /**
    **	Soft device remote class constructor.
    **
    **	@param name	remote name
    */
    cSoftRemote(void) : cRemote("XKeySym")
    {
      Start();
    }

    virtual ~cSoftRemote()
    {
      Cancel(3);
    }

    /**
    **	Receive keycode.
    **
    **	@param code	key code
    */
    void Receive(const char *code) {
      cMutexLock MutexLock(&mutex);
      Command = code;
      keyReceived.Broadcast();
    }

    void Transfer(eKeys key) {
       cRemote::Put(key, true);
    }
};

void cSoftRemote::Action(void)
{
  // see also VDR's cKbdRemote::Action()
  cTimeMs FirstTime;
  cTimeMs LastTime;
  cString FirstCommand = "";
  cString LastCommand = "";
  bool Delayed = false;
  bool Repeat = false;

  while (Running()) {
        cMutexLock MutexLock(&mutex);
        if (keyReceived.TimedWait(mutex, Setup.RcRepeatDelta * 3 / 2) && **Command) {
           if (strcmp(Command, LastCommand) == 0) {
              // If two keyboard events with the same command come in without an intermediate
              // timeout, this is a long key press that caused the repeat function to kick in:
              Delayed = false;
              FirstCommand = "";
              if (FirstTime.Elapsed() < (uint)Setup.RcRepeatDelay)
                 continue; // repeat function kicks in after a short delay
              if (LastTime.Elapsed() < (uint)Setup.RcRepeatDelta)
                 continue; // skip same keys coming in too fast
              cRemote::Put(Command, true);
              Repeat = true;
              LastTime.Set();
              }
           else if (strcmp(Command, FirstCommand) == 0) {
              // If the same command comes in twice with an intermediate timeout, we
              // need to delay the second command to see whether it is going to be
              // a repeat function or a separate key press:
              Delayed = true;
              }
           else {
              // This is a totally new key press, so we accept it immediately:
              cRemote::Put(Command);
              Delayed = false;
              FirstCommand = Command;
              FirstTime.Set();
              }
           }
        else if (Repeat) {
           // Timeout after a repeat function, so we generate a 'release':
           cRemote::Put(LastCommand, false, true);
           Repeat = false;
           }
        else if (Delayed && *FirstCommand) {
           // Timeout after two normal key presses of the same key, so accept the
           // delayed key:
           cRemote::Put(FirstCommand);
           Delayed = false;
           FirstCommand = "";
           FirstTime.Set();
           }
        else if (**FirstCommand && FirstTime.Elapsed() > (uint)Setup.RcRepeatDelay) {
           Delayed = false;
           FirstCommand = "";
           FirstTime.Set();
           }
        LastCommand = Command;
        Command = "";
        }
}

static cSoftRemote *csoft = NULL;

/**
**	Feed key press as remote input (called from C part).
**
**	@param keymap<->target keymap "XKeymap" name (obsolete, ignored)
**	@param key	pressed/released key name
**	@param repeat	repeated key flag (obsolete, ignored)
**	@param release	released key flag (obsolete, ignored)
**	@param letter	x11 character string (system setting locale)
*/
extern "C" void FeedKeyPress(const char *keymap, const char *key, int repeat,
    int release, const char *letter)
{
    if (!csoft || !keymap || !key) {
	return;
    }

    if (!letter) {
        csoft->Transfer(cKey::FromString(key));
        return;
    }

    csoft->Receive(key);
}

extern "C" void RemoteStart()
{
	csoft = new cSoftRemote;
}

extern "C" void RemoteStop()
{
	if (csoft) {
	    delete csoft;
	    csoft = NULL;
	}
}
