#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Util/Exceptions.h"

class CriticalSection
{

  // constructors
public:
  CriticalSection();
  ~CriticalSection();
  void init();

  // state
private:
  CRITICAL_SECTION anchor;

  // methods
public:
  friend class Lock;
  class Lock
  {
    // constructors
  public:
    Lock(CriticalSection&);
    Lock();
    ~Lock();

    // state
  private:
    CriticalSection *target;

    // methods
  public:
    void lock(CriticalSection&);
    bool tryLock(CriticalSection&);
    void release();
  };
};
