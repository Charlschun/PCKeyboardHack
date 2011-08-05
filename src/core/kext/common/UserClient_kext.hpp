#ifndef USERCLIENT_KEXT_HPP
#define USERCLIENT_KEXT_HPP

#include "base.hpp"
#include "Driver.hpp"
#include "bridge.h"
#include <IOKit/IOUserClient.h>

class org_pqrs_driver_PCKeyboardHack_UserClient_kext : public IOUserClient
{
  OSDeclareDefaultStructors(org_pqrs_driver_PCKeyboardHack_UserClient_kext)

public:
  // IOUserClient methods
  virtual bool initWithTask(task_t owningTask, void* securityToken, UInt32 type);

  virtual bool start(IOService* provider);
  virtual void stop(IOService* provider);

  virtual IOReturn clientClose(void);

  virtual bool didTerminate(IOService* provider, IOOptionBits options, bool* defer);

protected:
  virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments* arguments,
                                  IOExternalMethodDispatch* dispatch, OSObject* target, void* reference);

private:
  // ------------------------------------------------------------
  static IOReturn static_callback_open(org_pqrs_driver_PCKeyboardHack_UserClient_kext* target, void* reference, IOExternalMethodArguments* arguments);
  IOReturn callback_open(void);

  static IOReturn static_callback_close(org_pqrs_driver_PCKeyboardHack_UserClient_kext* target, void* reference, IOExternalMethodArguments* arguments);
  IOReturn callback_close(void);

  static IOReturn static_callback_synchronized_communication(org_pqrs_driver_PCKeyboardHack_UserClient_kext* target, void* reference, IOExternalMethodArguments* arguments);
  IOReturn callback_synchronized_communication(const BridgeUserClientStruct* inputdata);
  void handle_synchronized_communication(mach_vm_address_t address, mach_vm_size_t size);

  // ------------------------------------------------------------
  org_pqrs_driver_PCKeyboardHack* provider_;
  task_t task_;
  static IOExternalMethodDispatch methods_[BRIDGE_USERCLIENT__END__];
};

#endif
