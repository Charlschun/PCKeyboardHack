#include "UserClient_kext.hpp"

#define super IOUserClient

OSDefineMetaClassAndStructors(org_pqrs_driver_PCKeyboardHack_UserClient_kext, IOUserClient)

IOExternalMethodDispatch org_pqrs_driver_PCKeyboardHack_UserClient_kext::methods_[BRIDGE_USERCLIENT__END__] = {
  { // BRIDGE_USERCLIENT_OPEN
    reinterpret_cast<IOExternalMethodAction>(&static_callback_open), // Method pointer.
    0,                                                               // No scalar input values.
    0,                                                               // No struct input value.
    0,                                                               // No scalar output values.
    0                                                                // No struct output value.
  },
  { // BRIDGE_USERCLIENT_CLOSE
    reinterpret_cast<IOExternalMethodAction>(&static_callback_close), // Method pointer.
    0,                                                         // No scalar input values.
    0,                                                         // No struct input value.
    0,                                                         // No scalar output values.
    0                                                          // No struct output value.
  },
  { // BRIDGE_USERCLIENT_SYNCHRONIZED_COMMUNICATION
    reinterpret_cast<IOExternalMethodAction>(&static_callback_synchronized_communication), // Method pointer.
    0,                                                                                     // No scalar input values.
    sizeof(BridgeUserClientStruct),                                                        // The size of the input struct.
    1,                                                                                     // One scalar output value.
    0,                                                                                     // No struct output value.
  },
};

// ============================================================
// initWithTask is called as a result of the user process calling IOServiceOpen.
bool
org_pqrs_driver_PCKeyboardHack_UserClient_kext::initWithTask(task_t owningTask, void* securityToken, UInt32 type)
{
  if (clientHasPrivilege(owningTask, kIOClientPrivilegeLocalUser) != KERN_SUCCESS) {
    IOLOG_ERROR("UserClient_kext::initWithTask clientHasPrivilege failed\n");
    return false;
  }

  if (! super::initWithTask(owningTask, securityToken, type)) {
    IOLOG_ERROR("UserClient_kext::initWithTask super::initWithTask failed\n");
    return false;
  }

  task_     = owningTask;
  provider_ = NULL;

  return true;
}

// start is called after initWithTask as a result of the user process calling IOServiceOpen.
bool
org_pqrs_driver_PCKeyboardHack_UserClient_kext::start(IOService* provider)
{
  provider_ = OSDynamicCast(org_pqrs_driver_PCKeyboardHack, provider);
  if (! provider_) {
    IOLOG_ERROR("UserClient_kext::start provider == NULL\n");
    return false;
  }

  // It's important not to call super::start if some previous condition
  // (like an invalid provider) would cause this function to return false.
  // I/O Kit won't call stop on an object if its start function returned false.
  if (! super::start(provider)) {
    IOLOG_ERROR("UserClient_kext::start super::start failed\n");
    return false;
  }

  return true;
}

void
org_pqrs_driver_PCKeyboardHack_UserClient_kext::stop(IOService* provider)
{
  super::stop(provider);
  provider_ = NULL;
}

// clientClose is called as a result of the user process calling IOServiceClose.
IOReturn
org_pqrs_driver_PCKeyboardHack_UserClient_kext::clientClose(void)
{
  // Defensive coding in case the user process called IOServiceClose
  // without calling BRIDGE_USERCLIENT_CLOSE first.
  callback_close();

  // Inform the user process that this user client is no longer available. This will also cause the
  // user client instance to be destroyed.
  //
  // terminate would return false if the user process still had this user client open.
  // This should never happen in our case because this code path is only reached if the user process
  // explicitly requests closing the connection to the user client.
  if (! terminate()) {
    IOLOG_ERROR("UserClient_kext::clientClose terminate() failed\n");
  }

  // DON'T call super::clientClose, which just returns kIOReturnUnsupported.
  return kIOReturnSuccess;
}

bool
org_pqrs_driver_PCKeyboardHack_UserClient_kext::didTerminate(IOService* provider, IOOptionBits options, bool* defer)
{
  // If all pending I/O has been terminated, close our provider. If I/O is still outstanding, set defer to true
  // and the user client will not have stop called on it.
  callback_close();
  *defer = false;

  return super::didTerminate(provider, options, defer);
}

IOReturn
org_pqrs_driver_PCKeyboardHack_UserClient_kext::externalMethod(uint32_t selector, IOExternalMethodArguments* arguments,
                                                               IOExternalMethodDispatch* dispatch, OSObject* target, void* reference)
{
  if (selector < static_cast<uint32_t>(BRIDGE_USERCLIENT__END__)) {
    dispatch = &(methods_[selector]);

    if (! target) {
      target = this;
    }
  }

  return super::externalMethod(selector, arguments, dispatch, target, reference);
}


// ======================================================================
IOReturn
org_pqrs_driver_PCKeyboardHack_UserClient_kext::static_callback_open(org_pqrs_driver_PCKeyboardHack_UserClient_kext* target, void* reference, IOExternalMethodArguments* arguments)
{
  if (! target) return kIOReturnBadArgument;
  return target->callback_open();
}

IOReturn
org_pqrs_driver_PCKeyboardHack_UserClient_kext::callback_open(void)
{
  if (provider_ == NULL || isInactive()) {
    // Return an error if we don't have a provider. This could happen if the user process
    // called callback_open without calling IOServiceOpen first. Or, the user client could be
    // in the process of being terminated and is thus inactive.
    IOLOG_ERROR("UserClient_kext::callback_open kIOReturnNotAttached\n");
    return kIOReturnNotAttached;
  }

  if (! provider_->open(this)) {
    // The most common reason this open call will fail is because the provider is already open
    // and it doesn't support being opened by more than one client at a time.
    IOLOG_ERROR("UserClient_kext::callback_open kIOReturnExclusiveAccess\n");
    return kIOReturnExclusiveAccess;
  }

  return kIOReturnSuccess;
}

// ----------------------------------------------------------------------
IOReturn
org_pqrs_driver_PCKeyboardHack_UserClient_kext::static_callback_close(org_pqrs_driver_PCKeyboardHack_UserClient_kext* target, void* reference, IOExternalMethodArguments* arguments)
{
  if (! target) return kIOReturnBadArgument;
  return target->callback_close();
}

IOReturn
org_pqrs_driver_PCKeyboardHack_UserClient_kext::callback_close(void)
{
  if (! provider_) {
    // Return an error if we don't have a provider. This could happen if the user process
    // called callback_close without calling IOServiceOpen first.
    IOLOG_ERROR("UserClient_kext::callback_close kIOReturnNotAttached\n");
    return kIOReturnNotAttached;
  }

  if (! provider_->isOpen(this)) {
    // This error can occur in the normal usage. (by fast user switching)
    // So we suppress the log message.
#if 0
    IOLOG_ERROR("UserClient_kext::callback_close kIOReturnNotOpen\n");
#endif
    return kIOReturnNotOpen;
  }

  // Make sure we're the one who opened our provider before we tell it to close.
  provider_->close(this);

  return kIOReturnSuccess;
}

// ----------------------------------------------------------------------
IOReturn
org_pqrs_driver_PCKeyboardHack_UserClient_kext::static_callback_synchronized_communication(org_pqrs_driver_PCKeyboardHack_UserClient_kext* target, void* reference, IOExternalMethodArguments* arguments)
{
  if (! target) return kIOReturnBadArgument;
  return target->callback_synchronized_communication(static_cast<const BridgeUserClientStruct*>(arguments->structureInput), &arguments->scalarOutput[0]);
}

IOReturn
org_pqrs_driver_PCKeyboardHack_UserClient_kext::callback_synchronized_communication(const BridgeUserClientStruct* inputdata, uint64_t* outputdata)
{
  if (! inputdata || ! outputdata) {
    IOLOG_ERROR("UserClient_kext::callback_synchronized_communication kIOReturnBadArgument\n");
    return kIOReturnBadArgument;
  }

  if (provider_ == NULL || isInactive()) {
    // Return an error if we don't have a provider. This could happen if the user process
    // called callback_synchronized_communication without calling IOServiceOpen first.
    // Or, the user client could be in the process of being terminated and is thus inactive.
    IOLOG_ERROR("UserClient_kext::callback_synchronized_communication kIOReturnNotAttached\n");
    return kIOReturnNotAttached;
  }

  if (! provider_->isOpen(this)) {
    // Return an error if we do not have the driver open. This could happen if the user process
    // did not call callback_open before calling this function.
    IOLOG_ERROR("UserClient_kext::callback_synchronized_communication kIOReturnNotOpen\n");
    return kIOReturnNotOpen;
  }

  // --------------------------------------------------
  org_pqrs_driver_PCKeyboardHack::setConfiguration(*inputdata);

  return kIOReturnSuccess;
}
