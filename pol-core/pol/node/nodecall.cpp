/** @file
 *
 * @par History
 */

#include "nodecall.h"
#include "../../bscript/impstr.h"
#include "../../clib/logfacility.h"
#include "../../clib/stlutil.h"
#include "../polclock.h"
#include "../uoexec.h"
#include "jsprog.h"
#include "module/objwrap.h"
#include "napi-wrap.h"
#include "nodethread.h"
#include <future>

using namespace Napi;

namespace Pol
{
namespace Node
{
unsigned long requestNumber = 0;

std::atomic_uint nextRequestId( 0 );


std::map<Core::UOExecutor*, Napi::ObjectReference> execToModuleMap;

void emitExecutorShutdowns()
{
  auto call = Node::makeCall<bool>( [&]( Napi::Env env, NodeRequest<bool>* request ) {
    for ( auto& iter : execToModuleMap )
    {
      NODELOG.Format( "[{:04x}] [exec] finalizing executor {}\n" )
          << request->reqId() << iter.first->scriptname();
      bool retVal = iter.second.Get( "emit" )
                        .As<Function>()
                        .Call( iter.second.Value(), {Napi::String::New( env, "shutdown" )} )
                        .ToBoolean()
                        .Value();
      NODELOG.Format( "[{:04x}] [exec] module.emit('shutdown') returned {}\n" )
          << request->reqId() << retVal;
      iter.second.Unref();
    }
    return true;
  } );
  call.getRef();
  execToModuleMap.clear();
}

struct pid_compare
{
  bool operator()( const Core::UOExecutor& lhs, const Core::UOExecutor& rhs ) const
  {
    return lhs.pid() < rhs.pid();
  }
};

Napi::Value OnScriptReturn( const CallbackInfo& cbinfo )
{
  Napi::Env env = cbinfo.Env();
  auto* req = static_cast<NodeRequest<Bscript::BObjectRef>*>( cbinfo.Data() );
  Core::UOExecutor* ex = req->uoexec_;
  Napi::Value obj = cbinfo.Length() > 0 ? cbinfo[0] : env.Undefined();

  NODELOG.Format( "[{:04x}] [exec] script {} {} returned: {}\n" )
      << req->reqId() << ex->pid() << ex->scriptname() << Node::ToUtf8Value( obj );


  bool ret = Core::scriptScheduler.free_externalscript( ex );
  NODELOG.Format( "[{:04x}] [exec] freeing executor: {}\n" ) << req->reqId() << ret;
  return Boolean::New( env, true );
}

Napi::Value OnScriptCatch( const CallbackInfo& cbinfo )
{
  Napi::Env env = cbinfo.Env();
  auto* req = static_cast<NodeRequest<Bscript::BObjectRef>*>( cbinfo.Data() );
  Core::UOExecutor* ex = req->uoexec_;
  Napi::Value obj = cbinfo.Length() > 0 ? cbinfo[0] : env.Undefined();

  NODELOG.Format( "[{:04x}] [exec] script {} {} errored: {}\n" )
      << req->reqId()

      << ex->pid() << ex->scriptname() << Node::ToUtf8Value( obj );

  bool ret = Core::scriptScheduler.free_externalscript( ex );
  NODELOG.Format( "[{:04x}] [exec] freeing executor: {}\n" ) << req->reqId() << ret;

  return Boolean::New( env, false );
}

Bscript::BObjectRef runExecutor( Core::UOExecutor* ex )
{
  passert( ex->programType() == Bscript::Program::ProgramType::JAVASCRIPT );
  // Tell the script scheduler we will manage this executor ourselves

  NODELOG.Format(
      "[core] [exec] Adding executor {} {} to external holdlist (running to completion: {})\n" )
      << ex->pid() << ex->scriptname() << ex->running_to_completion();

  /*
  Add the executor to the script scheduler's "External Scripts" holdlist.
  

  Regarding telling the scheduler "we are done" is a little more complex. The External<>
  Finalize callback runs once the V8 garbage collector has freed the reference to the object.
  However, this is not done immediately after the script finishes -- the object is just
  arked as "ready for deletion". This means having _just_ an External<> Finalize is not enough
  to tell us when a script is finished in the context of async scripts (ie. a script that
  returns a Promise).

  This means we have two options regarding these async scripts:
    1. Trust the programmer resolves the promise when the script should end, and the UOExecutor
       is no longer needed.
       This means we can "free the script" in the scheduler immediately
       on the script's completion. However.
       Any use of NodeModuleWrap functions in the script will fail if the programmer does return
       a promise but and uses a module in another tick, eg. script returns non-Promise but has
       a timeout that uses a module.
       Also, a script that is "still running" but does not return a Promise will not be known
       to the core that it is running and will be lost.

    2. We'll tell the script scheduler we're finished with it in the External<UOExecutor>'s
       Finalize callback, which is ran with the garbage collector has freed the reference to
       our object. We won't know that a script is done until GC is ran. This is undeterministic
       unless we explicitly run it via `global.gc()`

    3. A combination of the two...?

  We'll go with OPTION 1.

  */
  Core::scriptScheduler.add_externalscript( ex );

  Node::JavascriptProgram* prog = static_cast<Node::JavascriptProgram*>( ex->prog_.get() );

  auto call = Node::makeCall<Bscript::BObjectRef>(
      [&]( Napi::Env env, NodeRequest<Bscript::BObjectRef>* request ) {
        auto obj = prog->obj.Value();
        auto reqId = request->reqId();
        NODELOG.Format( "[{:04x}] [exec] pid {}, call {} , argc {}\n" )
            << request->reqId() << ex->pid() << obj.Get( "_refId" ).As<String>().Utf8Value()
            << ex->ValueStack.size();
        // NODELOG.Clear();

        Napi::Array argv = Array::New( env, ex->ValueStack.size() );
        for ( size_t i = 0; !ex->ValueStack.empty(); ++i )
        {
          Bscript::BObjectRef rightref = ex->ValueStack.back();
          ex->ValueStack.pop_back();

          Napi::Value convertedVal = Node::NodeObjectWrap::Wrap( env, rightref, reqId );

          argv[i] = convertedVal;

          NODELOG.Format( "[{:04x}] [exec] argv[{}] = {}\n" )
              << reqId << i << Node::ToUtf8Value( convertedVal );
        }

        try
        {
          auto extUoExec =
              External<Core::UOExecutor>::New( env, ex, [=]( Napi::Env, Core::UOExecutor* uoexec ) {
                passert( uoexec != nullptr );
                NODELOG.Format( "[{:04x}] [exec] finalized\n" ) << reqId;
              } );

          auto jsCall = requireRef.Get( "scriptloader" )
                            .As<Object>()
                            .Get( "runScript" )
                            .As<Function>()
                            .Call( {extUoExec, Napi::String::New( env, prog->scriptname() ),
                                    prog->obj.Value(), argv} )
                            .As<Object>();

          auto jsRetVal = jsCall.Get( "value" );
          auto jsRetObj = jsRetVal.As<Object>();
          NODELOG.Format( "[{:04x}] [exec] returned value {}\n" )
              << request->reqId() << Node::ToUtf8Value( jsRetVal );
          //     /* if ( !ex->running_to_completion() )
          //      {
          //        auto mod = jsCall.Get( "module" ).As<Object>();

          //        if ( mod.Get( "_eventsCount" ).As<Number>().Int32Value() > 0 )
          //          execToModuleMap.emplace( ex, ObjectReference::New( mod, 1 ) );
          //      }
          //*/
          auto scriptRet = Napi::Function::New( env, OnScriptReturn, "OnScriptReturn", request );
          auto scriptCatch = Napi::Function::New( env, OnScriptCatch, "OnScriptCatch", request );
          auto convertedVal = NodeObjectWrap::Wrap( env, jsRetVal, reqId );

          if ( jsRetVal.IsPromise() )
          {
            jsRetObj.Get( "then" ).As<Function>().Call( jsRetVal, {scriptRet} );
            jsRetObj.Get( "catch" ).As<Function>().Call( jsRetVal, {scriptCatch} );
          }
          else
          {
            scriptRet.Call( jsRetVal, {jsRetVal} );
          }

          return convertedVal;
        }
        catch ( std::exception& e )
        {
          NODELOG.Format( "[{:04x}] [exec] errored {}\n" ) << request->reqId() << e.what();
          POLLOG_ERROR.Format( "Error running node script {}: {}\n" )
              << prog->scriptname() << e.what();

          auto scriptCatch = Napi::Function::New( env, OnScriptCatch, "OnScriptCatch", request );
          scriptCatch.Call( {String::New( env, e.what() )} );

          auto convertedVal = Bscript::BObjectRef( new Bscript::BError( e.what() ) );
          return convertedVal;
        }
      },
      ex );

  auto impref = call.getRef();
  NODELOG.Format( "[{:04x}] [exec] returned to core {}\n" )
      << call.reqId() << impref->impptr()->getStringRep();
  return impref;
}


}  // namespace Node
}  // namespace Pol
