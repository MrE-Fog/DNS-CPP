/**
 *  Operation.h
 * 
 *  When you make a call to Context::query() a pointer to an operation
 *  object is returned. This pointer can be ignored (it is managed
 *  internally by the DNS library. However, you can store it and 
 *  call methods on it (for example when you are no longer interested
 *  in the operation.
 * 
 *  @author Emiel Bruijntjes <emiel.bruijntjes@copernica.com>
 *  @copyright 2020 Copernica BV
 */

/**
 *  Include guard
 */
#pragma once

/**
 *  Begin of namespace
 */
namespace DNS {

/**
 *  Forward declarations
 */
class Handler;

/**
 *  Class definition
 */
class Operation
{
protected:
    /**
     *  The user-space handler
     *  @var Handler
     */
    Handler *_handler;
    
protected:
    /**
     *  Constructor is protected - user-space is not supposed to instantiate
     *  @param  handler
     */
    Operation(Handler *handler) : _handler(handler) {}
    
    /**
     *  No copying
     *  @param  that
     */
    Operation(const Operation &that) = delete;
    
    /**
     *  Destructor
     */
    virtual ~Operation() = default;
    
    /**
     *  Change the handler / install a different object to be notified of changes
     *  @param  handler
     */
    void install(Handler *handler)
    {
        // update the handler
        _handler = handler;
    }
    
    /**
     *  Cancel the operation
     */
    void cancel()
    {
        // the object can just be destructed
        delete this;
    }
};

/**
 *  End of namespace
 */
}

