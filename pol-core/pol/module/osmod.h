/** @file
 *
 * @par History
 * - 2006/09/17 Shinigami: OSExecutorModule::signal_event() will return error on full evene queue
 */


#ifndef BSCRIPT_OSEMOD_H
#define BSCRIPT_OSEMOD_H

#ifndef BSCRIPT_EXECMODL_H
#include "../../bscript/execmodl.h"
#endif

#include <ctime>
#include <deque>
#include <map>

#include "../globals/script_internals.h"
#include "../polclock.h"
#include "../uoasync.h"
#include "../uoexhelp.h"
#include "threadmod.h"

namespace Pol
{
namespace Bscript
{
class BObject;
class BObjectImp;
class Executor;
template <class T>
class TmplExecutorModule;
}  // namespace Bscript
namespace Mobile
{
class Character;
}  // namespace Mobile
}  // namespace Pol

namespace Pol
{
namespace Core
{
class UOExecutor;

void run_ready( void );
void check_blocked( polclock_t* pclocksleft );
void deschedule_executor( UOExecutor* ex );
polclock_t calc_script_clocksleft( polclock_t now );
}  // namespace Core
namespace Module
{
class OSExecutorModule : public Bscript::TmplExecutorModule<OSExecutorModule>,
                         public Module::ThreadInterface
{
public:
  virtual bool signal_event( Bscript::BObjectImp* event, Core::ULWObject* target ) override;

  virtual void suspend( Core::polclock_t sleep_until = 0 ) override;
  virtual void revive() override;

  explicit OSExecutorModule( Core::UOExecutor& exec );
  ~OSExecutorModule();


  virtual Bscript::BObjectImp* SleepForMs( int msecs, Bscript::BObjectImp* returnValue ) override;

  virtual unsigned int pid() const override;
  virtual bool blocked() const override;

  virtual bool in_debugger_holdlist() const override;
  virtual void revive_debugged() override;
  virtual Bscript::BObjectImp* clear_event_queue() override;  // DAVE

  virtual size_t sizeEstimate() const override;

  virtual bool critical() const override;
  virtual void critical( bool critical ) override;

  virtual bool warn_on_runaway() const override;
  virtual void warn_on_runaway( bool warn_on_runaway ) override;

  virtual unsigned char priority() const override;
  virtual void priority( unsigned char priority ) override;

  virtual Core::polclock_t sleep_until_clock() const override;
  virtual void sleep_until_clock( Core::polclock_t sleep_until_clock ) override;

  virtual Core::TimeoutHandle hold_itr() const override;
  virtual void hold_itr( Core::TimeoutHandle hold_itr ) override;

  virtual Core::HoldListType in_hold_list() const override;
  virtual void in_hold_list( Core::HoldListType in_hold_list ) override;

protected:
  bool getCharacterParam( unsigned param, Mobile::Character*& chrptr );

  friend class Bscript::TmplExecutorModule<OSExecutorModule>;

  Bscript::BObjectImp* mf_Create_Debug_Context();
  Bscript::BObjectImp* mf_GetPid();
  Bscript::BObjectImp* mf_GetProcess();
  Bscript::BObjectImp* mf_Sleep();
  Bscript::BObjectImp* mf_Sleepms();
  Bscript::BObjectImp* mf_Wait_For_Event();
  Bscript::BObjectImp* mf_Events_Waiting();
  Bscript::BObjectImp* mf_Set_Critical();
  Bscript::BObjectImp* mf_Is_Critical();

  Bscript::BObjectImp* mf_Start_Script();
  Bscript::BObjectImp* mf_Start_Skill_Script();
  Bscript::BObjectImp* mf_Run_Script_To_Completion();
  Bscript::BObjectImp* mf_Run_Script();
  Bscript::BObjectImp* mf_Set_Debug();
  Bscript::BObjectImp* mf_SysLog();
  Bscript::BObjectImp* mf_System_RPM();
  Bscript::BObjectImp* mf_Set_Priority();
  Bscript::BObjectImp* mf_Unload_Scripts();
  Bscript::BObjectImp* mf_Set_Script_Option();
  Bscript::BObjectImp* mf_Set_Event_Queue_Size();  // DAVE 11/24
  Bscript::BObjectImp* mf_OpenURL();
  Bscript::BObjectImp* mf_OpenConnection();
  Bscript::BObjectImp* mf_Debugger();
  Bscript::BObjectImp* mf_HTTPRequest();

  Bscript::BObjectImp* mf_Clear_Event_Queue();  // DAVE

  Bscript::BObjectImp* mf_PerformanceMeasure();

  bool critical_;
  unsigned char priority_;
  bool warn_on_runaway_;
  bool blocked_;
  Core::polclock_t sleep_until_clock_;  // 0 if wait forever

  Core::TimeoutHandle hold_itr_;
  Core::HoldListType in_hold_list_;
  Core::WAIT_TYPE wait_type;


  ref_ptr<Core::UOAsyncRequest> sleepRequest_;
  Bscript::BObjectImp* sleepReturnImp_;
  Core::UOExecutor& uoexec;

  unsigned int pid_;


  unsigned short max_eventqueue_size;  // DAVE 11/24
  std::deque<Bscript::BObjectImp*> events_;


  friend class NPCExecutorModule;
  friend void step_scripts( void );
  // friend void Core::run_ready( void );
  friend void Core::check_blocked( Core::polclock_t* pclocksleft );
  friend void new_check_blocked( void );
  friend void Core::deschedule_executor( Core::UOExecutor* ex );
  friend Core::polclock_t Core::calc_script_clocksleft( Core::polclock_t now );


  void event_occurred( Bscript::BObject event );
};

inline bool OSExecutorModule::getCharacterParam( unsigned param, Mobile::Character*& chrptr )
{
  return Core::getCharacterParam( exec, param, chrptr );
}

inline bool OSExecutorModule::blocked() const
{
  return blocked_;
}
}  // namespace Module
}  // namespace Pol

#endif
