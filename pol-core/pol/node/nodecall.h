/** @file
 *
 * @par History
 */


#ifndef NODECALL_H
#define NODECALL_H

#include "../../clib/timer.h"
#include "napi-wrap.h"
#include "nodethread.h"
#include <atomic>
#include <future>
#include <tuple>

using namespace Napi;

namespace Pol
{
namespace Bscript
{
class BObjectImp;
}
namespace Core
{
class UOExecutor;
}
namespace Node
{
class JavascriptProgram;

//
//// Callable implements Napi::Value(Env, Request*, Context*)
// template <typename Return, typename Callable, typename DataType>
// std::future<ReturnDetails<Return>> makeCall( Callable callback, DataType* data );

extern std::atomic_uint nextRequestId;

inline std::string ValueTypeToString( napi_valuetype v )
{
  switch ( v )
  {
  case napi_undefined:
    return "undefined";
  case napi_null:
    return "null";
  case napi_boolean:
    return "boolean";
  case napi_number:
    return "number";
  case napi_string:
    return "string";
  case napi_symbol:
    return "symbol";
  case napi_object:
    return "object";
  case napi_function:
    return "function";
  case napi_external:
    return "external";
  case napi_bigint:
    return "bigint";
  default:
    return "unknown";
  }
}
// CAN ONLY RUN IN NODE THREAD
inline std::string ToUtf8Value( const Napi::Value& v )
{
  return !v ? "<unset Napi::Value>"
            : v.IsUndefined() ? "undefined"
                              : v.As<Object>()
                                    .Get( "toString" )
                                    .As<Function>()
                                    .Call( v, {} )
                                    .As<String>()
                                    .Utf8Value();
}
//    auto functCode = funct.As<Object>()
//                         .Get( "toString" )
//                         .As<Function>()
//                         .Call( funct, {} )
//                         .As<String>()
//                         .Utf8Value();

// Callable implements Napi::Value(Env, Request*, Context*)

using timestamp = Tools::HighPerfTimer::time_mu;
using timepoint = std::tuple<std::string, timestamp>;

template <typename ReturnType>
class NodeRequest
{
private:
  unsigned long reqId_;
  Tools::HighPerfTimer timer_;
  std::vector<timepoint> points_;
  Napi::Env env_;
  ReturnType ref_;
  // std::promise<NodeRequest<ReturnType>> promise;

public:
  NodeRequest( Napi::Env env );
  NodeRequest();
  // std::future<ReturnType> val;
  std::future<NodeRequest<ReturnType>> future();
  unsigned int reqId() const;
  timestamp checkpoint( const std::string& key );
  void ref( ReturnType&& r );
  ReturnType getRef();
};

Bscript::BObjectImp* runExecutor( Core::UOExecutor* uoexec );
void emitExecutorShutdowns( );

template <typename ReturnType, typename Callable>
NodeRequest<ReturnType> makeCall( Callable callback, bool blocking = true );


}  // namespace Node
}  // namespace Pol

#include "nodecall-inl.h"

#endif
