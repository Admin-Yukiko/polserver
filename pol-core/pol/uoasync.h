#ifndef __UOASYNC_H
#define __UOASYNC_H


#include <string>
#include <time.h>

#include "../clib/rawtypes.h"
#include "../clib/weakptr.h"
#include "./globals/script_internals.h"
#include "./mobile/charactr.h"
#include "./network/client.h"
#include "./network/pktboth.h"

namespace Pol
{
namespace Module
{
class OSExecutorModule;
}
namespace Core
{
template <typename Callback, typename RequestData>
class AsyncRequestHandler;

template <typename Callback>
class AsyncRequestHandlerSansData;
class UOExecutor;


/** Request number */
extern u32 nextAsyncRequestId;

class UOAsyncRequest : public ref_counted
{
public:
  enum Type
  {
    TARGET_OBJECT,
    TARGET_CURSOR,
    TEXTENTRY
  };

  UOAsyncRequest( UOExecutor& exec, Mobile::Character* chr, Type type );

  ~UOAsyncRequest();

  /**
   * Create a new request. Will return `nullptr` if a request cannot be made, eg.
   * the executor cannot be suspended. Callback must implement BObjectImp*(RequestData*, ...T args).
   * The AsyncRequestHandler will become the owner of requestData, deleting it when the the the
   * request is responded to or aborted.
   */
  template <typename Callback, typename RequestData>
  static ref_ptr<Core::UOAsyncRequest> makeRequest( Core::UOExecutor& exec, Mobile::Character* chr,
                                                    Type type, Callback* callback,
                                                    RequestData* data );

  template <typename Callback>
  static ref_ptr<Core::UOAsyncRequest> makeRequest( Core::UOExecutor& exec, Mobile::Character* chr,
                                                    Type type, Callback* callback );


  /**
   * Structure encapsulating data for requesting a target from a character.
   */
  struct TargetData
  {
    int target_options;
  };


public:
  // public for now...
  UOExecutor& exec_;
  Mobile::Character* chr_;
  bool handled_;
  u32 reqId_;
  Type type_;

protected:
  /**
   * Used to update all things listening on this request: (1) the UOExecutor with the value +
   * revive, (2) and remove the request from the client game data
   */
  void resolved( Bscript::BObjectImp* resp );

private:
  /**
   * We have two different Target callbacks: one for objects, and one for coords
   */
  using TargetObjectCallback = Bscript::BObjectImp*( TargetData* data, Mobile::Character* chr,
                                                     Core::UObject* obj );

  using TargetCoordsCallback = Bscript::BObjectImp*( TargetData* data, Mobile::Character* chr,
                                                     PKTBI_6C* msg );

  using TextentryCallback = Bscript::BObjectImp*( Network::Client* client,
                                                     PKTIN_AC* msg );

public:
  using TargetObject = Core::AsyncRequestHandler<TargetObjectCallback, TargetData>;
  using TargetCoords = Core::AsyncRequestHandler<TargetCoordsCallback, TargetData>;
  using Textentry = Core::AsyncRequestHandlerSansData<TextentryCallback>;

  /**
   * Abort the request by reviving the executor and deleting the request object.
   */
  bool abort();


};  //

// using UOAsyncRequestsHolder =

class UOAsyncRequestHolder
{
private:
  std::map<Core::UOAsyncRequest::Type, std::vector<ref_ptr<Core::UOAsyncRequest>>> requests;


public:
  UOAsyncRequestHolder();

  /** Returns the number of aborted requests in this holder */
  int abortAll();

  void addRequest( Core::UOAsyncRequest::Type type, ref_ptr<Core::UOAsyncRequest> req );
  bool removeRequest( Core::UOAsyncRequest* req );

  template <typename Handler>
  Handler* findRequest( Core::UOAsyncRequest::Type type );

  template <typename Handler>
  Handler* findRequest( Core::UOAsyncRequest::Type type, u32 hint );

  bool hasRequest( Core::UOAsyncRequest::Type type, u32 hint );
  bool hasRequest( Core::UOAsyncRequest::Type type );
};


// FIXME can be moved to a different h?

template <typename Handler>
inline Handler* UOAsyncRequestHolder::findRequest( Core::UOAsyncRequest::Type type )
{
  auto iter = requests.find( type );

  if ( iter != requests.end() )
  {
    // Since no hint provided, always return the first..
    auto req = iter->second;
    if (!req.empty())
    {
      return static_cast<Handler*>( req.at(0).get() );
    }
  }
  return nullptr;
}
template <typename Handler>
inline Handler* UOAsyncRequestHolder::findRequest( Core::UOAsyncRequest::Type type, u32 hint )
{
  auto iter = requests.find( type );

  if ( iter != requests.end() )
  {
    for ( auto req : iter->second )
    {
      if ( req->reqId_ == hint )
        return Clib::explicit_cast<Handler*, Core::UOAsyncRequest*>( iter->second.get() );
    }
  }
  return nullptr;
}


// With data
template <typename Callback, typename RequestData>
static ref_ptr<Core::UOAsyncRequest> UOAsyncRequest::makeRequest( Core::UOExecutor& exec,
                                                                  Mobile::Character* chr, Type type,
                                                                  Callback* callback,
                                                                  RequestData* data )
{
  if ( !exec.suspend() )
  {
    if ( data != nullptr )
      delete data;
    return ref_ptr<Core::UOAsyncRequest>( nullptr );
  }
  ref_ptr<Core::UOAsyncRequest> req(
      new AsyncRequestHandler<Callback, RequestData>( exec, chr, type, callback, data ) );
  exec.requests.addRequest( type, req );
  chr->client->gd->requests.addRequest( type, req );
  return req;
}

// No data
template <typename Callback>
static ref_ptr<Core::UOAsyncRequest> UOAsyncRequest::makeRequest( Core::UOExecutor& exec,
                                                                  Mobile::Character* chr, Type type,
                                                                  Callback* callback )
{
  if ( !exec.suspend() )
  {
    return ref_ptr<Core::UOAsyncRequest>( nullptr );
  }
  ref_ptr<Core::UOAsyncRequest> req(
      new AsyncRequestHandler<Callback, nullptr_t>( exec, chr, type, callback, nullptr ) );
  exec.requests.addRequest( type, req );
  chr->client->gd->requests.addRequest( type, req );
  return req;
}

}  // namespace Core
}  // namespace Pol

#endif
